#pragma once
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <unordered_map>
#include "../../shared/types/bb_types.h"

namespace BBRender {

// ─── Scheduler events ────────────────────────────────────────────────────────
struct SchedulerEvent {
    enum class Type {
        JobAdded, JobStarted, JobPaused, JobResumed,
        JobCompleted, JobFailed, JobCancelled,
        TaskAssigned, TaskCompleted, TaskFailed,
        WorkerJoined, WorkerLeft, WorkerIdle
    };
    Type type;
    JobID    jobId    = 0;
    TaskID   taskId   = 0;
    WorkerID workerId = 0;
    std::string detail;
};

using SchedulerEventCallback = std::function<void(const SchedulerEvent&)>;

// ─── Priority queue comparator ────────────────────────────────────────────────
struct JobPriorityComparator {
    bool operator()(const JobInfo* a, const JobInfo* b) const {
        if (a->priority != b->priority)
            return static_cast<int>(a->priority) > static_cast<int>(b->priority);
        return a->submittedAt.epoch_ms > b->submittedAt.epoch_ms;
    }
};

// ─── Scheduler ───────────────────────────────────────────────────────────────
class Scheduler {
public:
    explicit Scheduler(uint32_t schedulerThreads = 2);
    ~Scheduler();

    // Job API
    JobID   submitJob(JobInfo job);
    bool    cancelJob(JobID id);
    bool    pauseJob(JobID id);
    bool    resumeJob(JobID id);
    bool    reprioritizeJob(JobID id, Priority p);
    bool    retryFailedTasks(JobID id);

    // Query
    std::optional<JobInfo>    getJob(JobID id) const;
    std::vector<JobInfo>      allJobs() const;
    std::vector<JobInfo>      activeJobs() const;
    FarmStats                 getFarmStats() const;

    // Worker management
    WorkerID registerWorker(WorkerInfo info);
    void     updateWorkerStatus(WorkerID id, WorkerStatus status, float progress = 0.0f);
    void     updateWorkerStats(WorkerID id, const RenderStats& stats);
    bool     removeWorker(WorkerID id);
    void     workerTaskComplete(WorkerID wid, TaskID tid, bool success, const std::string& errMsg = {});
    std::vector<WorkerInfo>   allWorkers() const;

    // Events
    void addEventCallback(SchedulerEventCallback cb);

    // Lifecycle
    void start();
    void stop();
    bool isRunning() const { return m_running.load(); }

private:
    // Internal scheduling loop
    void schedulerLoop();
    void assignPendingTasks();
    void processCompletedTasks();

    // Task decomposition
    std::vector<TaskInfo> decomposeTasks(JobInfo& job);
    TaskID nextTaskID() { return ++m_taskSeq; }
    JobID  nextJobID()  { return ++m_jobSeq;  }

    // Worker matching
    WorkerID findBestWorker(const TaskInfo& task, const ResourceSpec& spec) const;
    bool     workerMeetsSpec(const WorkerInfo& w, const ResourceSpec& spec) const;

    // Job state transitions
    void updateJobStatus(JobID id);
    void emitEvent(const SchedulerEvent& ev);

    // Data
    mutable std::mutex              m_mutex;
    std::unordered_map<JobID,    JobInfo>    m_jobs;
    std::unordered_map<TaskID,   TaskInfo>   m_tasks;
    std::unordered_map<WorkerID, WorkerInfo> m_workers;

    // Priority queue (pointers into m_jobs)
    std::vector<JobID> m_jobQueue; // sorted by priority+time

    // ID counters
    std::atomic<JobID>    m_jobSeq{0};
    std::atomic<TaskID>   m_taskSeq{0};
    std::atomic<WorkerID> m_workerSeq{0};

    // Threading
    std::atomic<bool>       m_running{false};
    std::vector<std::thread> m_threads;
    std::condition_variable m_cv;
    std::mutex              m_cvMutex;

    // Callbacks
    std::vector<SchedulerEventCallback> m_callbacks;

    // Stats
    std::atomic<uint64_t> m_framesRenderedToday{0};
    std::atomic<uint64_t> m_completedToday{0};
    std::atomic<uint64_t> m_failedToday{0};
};

// ─── Implementation ───────────────────────────────────────────────────────────
inline Scheduler::Scheduler(uint32_t threads) {
    m_threads.reserve(threads);
}

inline Scheduler::~Scheduler() {
    stop();
}

inline void Scheduler::start() {
    m_running = true;
    auto n = m_threads.capacity() > 0 ? m_threads.capacity() : 2;
    for (size_t i = 0; i < n; ++i)
        m_threads.emplace_back([this]{ schedulerLoop(); });
}

inline void Scheduler::stop() {
    m_running = false;
    m_cv.notify_all();
    for (auto& t : m_threads) if (t.joinable()) t.join();
    m_threads.clear();
}

inline JobID Scheduler::submitJob(JobInfo job) {
    std::lock_guard<std::mutex> lk(m_mutex);
    job.id          = nextJobID();
    job.submittedAt = TimePoint::now();
    job.status      = JobStatus::Queued;
    job.tasks       = decomposeTasks(job);
    job.totalTasks  = static_cast<int32_t>(job.tasks.size());

    for (auto& t : job.tasks) {
        m_tasks[t.id] = t;
    }
    JobID id = job.id;
    m_jobs[id] = std::move(job);
    m_jobQueue.push_back(id);

    // Keep queue sorted by priority
    std::sort(m_jobQueue.begin(), m_jobQueue.end(), [&](JobID a, JobID b){
        auto& ja = m_jobs[a]; auto& jb = m_jobs[b];
        if (ja.priority != jb.priority)
            return static_cast<int>(ja.priority) < static_cast<int>(jb.priority);
        return ja.submittedAt.epoch_ms < jb.submittedAt.epoch_ms;
    });

    emitEvent({SchedulerEvent::Type::JobAdded, id});
    m_cv.notify_one();
    return id;
}

inline bool Scheduler::cancelJob(JobID id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return false;
    auto& job = it->second;
    job.status = JobStatus::Cancelled;
    for (auto& t : job.tasks) {
        if (t.status == TaskStatus::Pending || t.status == TaskStatus::Queued)
            t.status = TaskStatus::Cancelled;
    }
    emitEvent({SchedulerEvent::Type::JobCancelled, id});
    m_cv.notify_one();
    return true;
}

inline bool Scheduler::pauseJob(JobID id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return false;
    it->second.status = JobStatus::Paused;
    emitEvent({SchedulerEvent::Type::JobPaused, id});
    return true;
}

inline bool Scheduler::resumeJob(JobID id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return false;
    it->second.status = JobStatus::Queued;
    emitEvent({SchedulerEvent::Type::JobResumed, id});
    m_cv.notify_one();
    return true;
}

inline bool Scheduler::reprioritizeJob(JobID id, Priority p) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return false;
    it->second.priority = p;
    std::sort(m_jobQueue.begin(), m_jobQueue.end(), [&](JobID a, JobID b){
        auto& ja = m_jobs[a]; auto& jb = m_jobs[b];
        if (ja.priority != jb.priority)
            return static_cast<int>(ja.priority) < static_cast<int>(jb.priority);
        return ja.submittedAt.epoch_ms < jb.submittedAt.epoch_ms;
    });
    return true;
}

inline bool Scheduler::retryFailedTasks(JobID id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return false;
    auto& job = it->second;
    for (auto& t : job.tasks) {
        if (t.status == TaskStatus::Failed && t.retryCount < t.maxRetries) {
            t.status = TaskStatus::Queued;
            ++t.retryCount;
            t.workerId = 0;
        }
    }
    job.status = JobStatus::Queued;
    m_cv.notify_one();
    return true;
}

inline WorkerID Scheduler::registerWorker(WorkerInfo info) {
    std::lock_guard<std::mutex> lk(m_mutex);
    info.id          = ++m_workerSeq;
    info.connectedAt = TimePoint::now();
    info.lastSeen    = info.connectedAt;
    info.status      = WorkerStatus::Idle;
    WorkerID id      = info.id;
    m_workers[id]    = std::move(info);
    emitEvent({SchedulerEvent::Type::WorkerJoined, 0, 0, id});
    m_cv.notify_one();
    return id;
}

inline void Scheduler::updateWorkerStatus(WorkerID id, WorkerStatus status, float progress) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_workers.find(id);
    if (it == m_workers.end()) return;
    it->second.status           = status;
    it->second.currentProgress  = progress;
    it->second.lastSeen         = TimePoint::now();
    if (status == WorkerStatus::Idle) {
        m_cv.notify_one();
        emitEvent({SchedulerEvent::Type::WorkerIdle, 0, 0, id});
    }
}

inline void Scheduler::updateWorkerStats(WorkerID id, const RenderStats& stats) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_workers.find(id);
    if (it != m_workers.end()) it->second.stats = stats;
}

inline bool Scheduler::removeWorker(WorkerID id) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_workers.find(id);
    if (it == m_workers.end()) return false;
    // Re-queue any running tasks assigned to this worker
    for (auto& [tid, task] : m_tasks) {
        if (task.workerId == id && task.status == TaskStatus::Running) {
            task.status   = TaskStatus::Queued;
            task.workerId = 0;
        }
    }
    m_workers.erase(it);
    emitEvent({SchedulerEvent::Type::WorkerLeft, 0, 0, id});
    m_cv.notify_one();
    return true;
}

inline void Scheduler::workerTaskComplete(WorkerID wid, TaskID tid, bool success, const std::string& errMsg) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto tit = m_tasks.find(tid);
    if (tit == m_tasks.end()) return;
    auto& task = tit->second;

    task.finishedAt = TimePoint::now();
    task.elapsedSec = (task.finishedAt.epoch_ms - task.startedAt.epoch_ms) / 1000.0;

    if (success) {
        task.status   = TaskStatus::Completed;
        task.progress = 1.0f;
        ++m_framesRenderedToday;
        emitEvent({SchedulerEvent::Type::TaskCompleted, task.jobId, tid, wid});
    } else {
        task.errorMsg = errMsg;
        if (task.retryCount < task.maxRetries) {
            task.status = TaskStatus::Retrying;
            ++task.retryCount;
            task.workerId = 0;
        } else {
            task.status = TaskStatus::Failed;
            ++m_failedToday;
        }
        emitEvent({SchedulerEvent::Type::TaskFailed, task.jobId, tid, wid});
    }

    // Free the worker
    auto wit = m_workers.find(wid);
    if (wit != m_workers.end()) {
        wit->second.status          = WorkerStatus::Idle;
        wit->second.currentTaskId   = 0;
        wit->second.currentJobId    = 0;
        wit->second.currentProgress = 0.0f;
        if (success) ++wit->second.totalFramesRendered;
    }
    updateJobStatus(task.jobId);
    m_cv.notify_one();
}

inline std::vector<TaskInfo> Scheduler::decomposeTasks(JobInfo& job) {
    std::vector<TaskInfo> tasks;
    int32_t chunk = std::max(1, job.frames.chunkSize);
    for (FrameNum f = job.frames.start; f <= job.frames.end; f += chunk * job.frames.step) {
        TaskInfo t;
        t.id         = nextTaskID();
        t.jobId      = job.id;
        t.frameStart = f;
        t.frameEnd   = std::min(f + (chunk - 1) * job.frames.step, job.frames.end);
        t.status     = TaskStatus::Queued;
        t.maxRetries = 3;
        t.submittedAt = TimePoint::now();
        tasks.push_back(t);
    }
    return tasks;
}

inline WorkerID Scheduler::findBestWorker(const TaskInfo&, const ResourceSpec& spec) const {
    // Find idle worker that meets spec, prefer least-loaded
    WorkerID best = 0;
    double   bestScore = -1.0;
    for (auto& [wid, w] : m_workers) {
        if (!w.enabled) continue;
        if (w.status != WorkerStatus::Idle) continue;
        if (!workerMeetsSpec(w, spec)) continue;
        // Score = free CPU weight
        double score = (100.0 - w.stats.cpuPercent) * 0.5
                     + (static_cast<double>(w.ramTotalMB - w.stats.ramUsedMB) / 1024.0) * 0.5;
        if (score > bestScore) { bestScore = score; best = wid; }
    }
    return best;
}

inline bool Scheduler::workerMeetsSpec(const WorkerInfo& w, const ResourceSpec& spec) const {
    if (spec.cpuCores  && w.cpuCores  < spec.cpuCores)  return false;
    if (spec.ramMB     && w.ramTotalMB < spec.ramMB)     return false;
    if (spec.gpuCount  && w.gpuCount  < spec.gpuCount)   return false;
    if (spec.vramMB    && w.vramMB    < spec.vramMB)     return false;
    if (!spec.platform.empty() && w.platform != spec.platform) return false;
    return true;
}

inline void Scheduler::updateJobStatus(JobID id) {
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return;
    auto& job = it->second;

    int completed = 0, failed = 0, running = 0, pending = 0;
    for (auto& t : job.tasks) {
        if (t.status == TaskStatus::Completed) ++completed;
        else if (t.status == TaskStatus::Failed)    ++failed;
        else if (t.status == TaskStatus::Running)   ++running;
        else ++pending;
    }
    job.completedTasks = completed;
    job.failedTasks    = failed;
    job.runningTasks   = running;
    job.progress       = job.totalTasks > 0
        ? static_cast<float>(completed) / job.totalTasks : 0.0f;

    if (completed + failed == job.totalTasks) {
        job.status      = (failed > 0) ? JobStatus::Failed : JobStatus::Completed;
        job.finishedAt  = TimePoint::now();
        if (job.status == JobStatus::Completed) ++m_completedToday;
        emitEvent({job.status == JobStatus::Completed
            ? SchedulerEvent::Type::JobCompleted
            : SchedulerEvent::Type::JobFailed, id});
    } else if (running > 0) {
        job.status = JobStatus::Running;
    }
}

inline void Scheduler::schedulerLoop() {
    while (m_running) {
        {
            std::unique_lock<std::mutex> lk(m_cvMutex);
            m_cv.wait_for(lk, std::chrono::milliseconds(100));
        }
        if (!m_running) break;
        assignPendingTasks();
    }
}

inline void Scheduler::assignPendingTasks() {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto jobId : m_jobQueue) {
        auto jit = m_jobs.find(jobId);
        if (jit == m_jobs.end()) continue;
        auto& job = jit->second;
        if (job.status == JobStatus::Paused || job.status == JobStatus::Cancelled) continue;
        if (job.status == JobStatus::Completed || job.status == JobStatus::Failed) continue;

        for (auto& task : job.tasks) {
            if (task.status != TaskStatus::Queued && task.status != TaskStatus::Retrying) continue;
            WorkerID wid = findBestWorker(task, job.resources);
            if (!wid) continue;

            auto& worker   = m_workers[wid];
            task.status    = TaskStatus::Running;
            task.workerId  = wid;
            task.startedAt = TimePoint::now();
            task.workerHost = worker.hostname;

            worker.status         = WorkerStatus::Rendering;
            worker.currentTaskId  = task.id;
            worker.currentJobId   = job.id;
            worker.currentProgress = 0.0f;

            if (job.status == JobStatus::Queued) {
                job.status     = JobStatus::Running;
                job.startedAt  = TimePoint::now();
                emitEvent({SchedulerEvent::Type::JobStarted, job.id});
            }
            emitEvent({SchedulerEvent::Type::TaskAssigned, job.id, task.id, wid});
        }
    }
}

inline void Scheduler::emitEvent(const SchedulerEvent& ev) {
    for (auto& cb : m_callbacks) cb(ev);
}

inline void Scheduler::addEventCallback(SchedulerEventCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_callbacks.push_back(std::move(cb));
}

inline std::optional<JobInfo> Scheduler::getJob(JobID id) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_jobs.find(id);
    if (it == m_jobs.end()) return std::nullopt;
    return it->second;
}

inline std::vector<JobInfo> Scheduler::allJobs() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<JobInfo> v;
    v.reserve(m_jobs.size());
    for (auto& [id, j] : m_jobs) v.push_back(j);
    return v;
}

inline std::vector<WorkerInfo> Scheduler::allWorkers() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<WorkerInfo> v;
    v.reserve(m_workers.size());
    for (auto& [id, w] : m_workers) v.push_back(w);
    return v;
}

inline FarmStats Scheduler::getFarmStats() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    FarmStats s;
    s.totalWorkers     = static_cast<uint32_t>(m_workers.size());
    double cpuSum = 0, ramSum = 0, gpuSum = 0;
    for (auto& [id, w] : m_workers) {
        if (w.status != WorkerStatus::Offline) ++s.onlineWorkers;
        if (w.status == WorkerStatus::Rendering) ++s.busyWorkers;
        if (w.status == WorkerStatus::Idle)      ++s.idleWorkers;
        cpuSum += w.stats.cpuPercent;
        ramSum += w.ramTotalMB > 0 ? 100.0 * w.stats.ramUsedMB / w.ramTotalMB : 0;
        gpuSum += w.stats.gpuPercent;
    }
    if (s.onlineWorkers) {
        s.avgCpuPercent = cpuSum / s.onlineWorkers;
        s.avgRamPercent = ramSum / s.onlineWorkers;
        s.avgGpuPercent = gpuSum / s.onlineWorkers;
    }
    for (auto& [id, j] : m_jobs) {
        if (j.status == JobStatus::Running) ++s.activeJobs;
        else if (j.status == JobStatus::Queued) ++s.pendingJobs;
    }
    s.completedToday = static_cast<uint32_t>(m_completedToday.load());
    s.failedToday    = static_cast<uint32_t>(m_failedToday.load());
    s.framesRenderedToday = m_framesRenderedToday.load();
    s.farmEfficiency = s.onlineWorkers > 0
        ? static_cast<double>(s.busyWorkers) / s.onlineWorkers : 0.0;
    return s;
}

inline TimePoint TimePoint::now() {
    using namespace std::chrono;
    TimePoint tp;
    tp.epoch_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    return tp;
}

} // namespace BBRender
