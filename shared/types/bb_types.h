#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <functional>

namespace BBRender {

// ─── IDs ────────────────────────────────────────────────────────────────────
using JobID    = uint64_t;
using TaskID   = uint64_t;
using WorkerID = uint64_t;
using FrameNum = int32_t;

// ─── Enumerations ────────────────────────────────────────────────────────────
enum class JobStatus : uint8_t {
    Pending = 0,
    Queued,
    Running,
    Paused,
    Completed,
    Failed,
    Cancelled
};

enum class TaskStatus : uint8_t {
    Pending = 0,
    Queued,
    Running,
    Completed,
    Failed,
    Retrying,
    Cancelled
};

enum class WorkerStatus : uint8_t {
    Offline  = 0,
    Idle,
    Rendering,
    Paused,
    Error,
    Maintenance
};

enum class RenderEngine : uint8_t {
    Nuke      = 0,
    Silhouette,
    Blender,
    Houdini,
    Maya,
    Cinema4D,
    AfterEffects,
    Custom
};

enum class Priority : uint8_t {
    Critical  = 0,
    High      = 1,
    Normal    = 2,
    Low       = 3,
    Background = 4
};

enum class MsgType : uint16_t {
    // Handshake
    Hello         = 0x0001,
    HelloAck      = 0x0002,
    // Worker lifecycle
    WorkerRegister = 0x0010,
    WorkerStatus   = 0x0011,
    WorkerHeartbeat= 0x0012,
    WorkerShutdown = 0x0013,
    // Job management
    JobSubmit     = 0x0020,
    JobStatus     = 0x0021,
    JobCancel     = 0x0022,
    JobPause      = 0x0023,
    JobResume     = 0x0024,
    JobList       = 0x0025,
    // Task management
    TaskAssign    = 0x0030,
    TaskProgress  = 0x0031,
    TaskComplete  = 0x0032,
    TaskFail      = 0x0033,
    // Monitoring
    FarmStats     = 0x0040,
    WorkerList    = 0x0041,
    // Control
    Shutdown      = 0x00FF,
    Error         = 0xFFFF
};

// ─── Core structs ────────────────────────────────────────────────────────────
struct TimePoint {
    int64_t epoch_ms = 0;  // milliseconds since epoch
    static TimePoint now();
    std::string toISO() const;
};

struct FrameRange {
    FrameNum start  = 1;
    FrameNum end    = 1;
    FrameNum step   = 1;
    int32_t  chunkSize = 1;  // frames per task

    int32_t totalFrames() const { return (end - start) / step + 1; }
    int32_t totalTasks()  const { return (totalFrames() + chunkSize - 1) / chunkSize; }
};

struct ResourceSpec {
    uint32_t cpuCores   = 0;    // 0 = any
    uint64_t ramMB      = 0;    // 0 = any
    uint32_t gpuCount   = 0;    // 0 = not required
    uint64_t vramMB     = 0;
    std::string platform;        // "linux", "windows", "macos", "" = any
};

struct RenderStats {
    double   cpuPercent   = 0.0;
    uint64_t ramUsedMB    = 0;
    uint64_t ramTotalMB   = 0;
    double   gpuPercent   = 0.0;
    uint64_t vramUsedMB   = 0;
    uint64_t vramTotalMB  = 0;
    double   diskReadMBs  = 0.0;
    double   diskWriteMBs = 0.0;
    double   netInMBs     = 0.0;
    double   netOutMBs    = 0.0;
    uint64_t framesRendered = 0;
    double   avgFrameTimeSec = 0.0;
};

struct TaskInfo {
    TaskID     id         = 0;
    JobID      jobId      = 0;
    FrameNum   frameStart = 1;
    FrameNum   frameEnd   = 1;
    WorkerID   workerId   = 0;
    TaskStatus status     = TaskStatus::Pending;
    float      progress   = 0.0f;      // 0–1
    uint32_t   retryCount = 0;
    uint32_t   maxRetries = 3;
    TimePoint  submittedAt;
    TimePoint  startedAt;
    TimePoint  finishedAt;
    std::string workerHost;
    std::string errorMsg;
    std::string outputPath;
    double     elapsedSec = 0.0;
};

struct JobInfo {
    JobID       id          = 0;
    std::string name;
    std::string projectPath;
    std::string outputPath;
    std::string sceneFile;
    std::string camera;
    RenderEngine engine     = RenderEngine::Nuke;
    std::string enginePath;
    std::string engineArgs;
    Priority    priority    = Priority::Normal;
    JobStatus   status      = JobStatus::Pending;
    FrameRange  frames;
    ResourceSpec resources;
    TimePoint   submittedAt;
    TimePoint   startedAt;
    TimePoint   finishedAt;
    std::string submittedBy;
    std::string department;
    std::string notes;
    // Runtime
    int32_t     totalTasks      = 0;
    int32_t     completedTasks  = 0;
    int32_t     failedTasks     = 0;
    int32_t     runningTasks    = 0;
    float       progress        = 0.0f;
    double      etaSec          = 0.0;
    std::vector<TaskInfo> tasks;
};

struct WorkerInfo {
    WorkerID    id        = 0;
    std::string hostname;
    std::string ipAddress;
    uint16_t    port      = 0;
    WorkerStatus status   = WorkerStatus::Offline;
    std::string platform;
    std::string osVersion;
    uint32_t    cpuCores  = 0;
    std::string cpuModel;
    uint64_t    ramTotalMB = 0;
    uint32_t    gpuCount  = 0;
    std::string gpuModel;
    uint64_t    vramMB    = 0;
    TaskID      currentTaskId = 0;
    JobID       currentJobId  = 0;
    float       currentProgress = 0.0f;
    RenderStats stats;
    TimePoint   lastSeen;
    TimePoint   connectedAt;
    std::string version;
    uint32_t    totalFramesRendered = 0;
    double      totalRenderHours   = 0.0;
    uint32_t    errorCount         = 0;
    bool        enabled = true;
};

struct FarmStats {
    uint32_t totalWorkers   = 0;
    uint32_t onlineWorkers  = 0;
    uint32_t busyWorkers    = 0;
    uint32_t idleWorkers    = 0;
    uint32_t activeJobs     = 0;
    uint32_t pendingJobs    = 0;
    uint32_t completedToday = 0;
    uint32_t failedToday    = 0;
    uint32_t runningTasks   = 0;
    uint32_t queuedTasks    = 0;
    double   avgCpuPercent  = 0.0;
    double   avgRamPercent  = 0.0;
    double   avgGpuPercent  = 0.0;
    double   farmEfficiency = 0.0;  // 0–1
    uint64_t framesRenderedToday = 0;
    double   avgFrameTimeSec     = 0.0;
};

// ─── Network message ─────────────────────────────────────────────────────────
struct NetMessage {
    static constexpr uint32_t MAGIC = 0x42425246; // 'BBRF'
    uint32_t magic   = MAGIC;
    MsgType  type    = MsgType::Error;
    uint32_t seq     = 0;
    uint32_t payloadSize = 0;
    std::string payload; // JSON

    std::vector<uint8_t> serialize() const;
    static std::optional<NetMessage> deserialize(const uint8_t* data, size_t len);
};

// ─── Callbacks ───────────────────────────────────────────────────────────────
using JobCallback    = std::function<void(const JobInfo&)>;
using WorkerCallback = std::function<void(const WorkerInfo&)>;
using StatsCallback  = std::function<void(const FarmStats&)>;
using ErrorCallback  = std::function<void(const std::string&)>;

// ─── String helpers ───────────────────────────────────────────────────────────
inline const char* statusStr(JobStatus s) {
    switch(s) {
        case JobStatus::Pending:   return "Pending";
        case JobStatus::Queued:    return "Queued";
        case JobStatus::Running:   return "Running";
        case JobStatus::Paused:    return "Paused";
        case JobStatus::Completed: return "Completed";
        case JobStatus::Failed:    return "Failed";
        case JobStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

inline const char* statusStr(WorkerStatus s) {
    switch(s) {
        case WorkerStatus::Offline:     return "Offline";
        case WorkerStatus::Idle:        return "Idle";
        case WorkerStatus::Rendering:   return "Rendering";
        case WorkerStatus::Paused:      return "Paused";
        case WorkerStatus::Error:       return "Error";
        case WorkerStatus::Maintenance: return "Maintenance";
    }
    return "Unknown";
}

inline const char* engineStr(RenderEngine e) {
    switch(e) {
        case RenderEngine::Nuke:        return "Nuke";
        case RenderEngine::Silhouette:  return "Silhouette";
        case RenderEngine::Blender:     return "Blender";
        case RenderEngine::Houdini:     return "Houdini";
        case RenderEngine::Maya:        return "Maya";
        case RenderEngine::Cinema4D:    return "Cinema4D";
        case RenderEngine::AfterEffects:return "After Effects";
        case RenderEngine::Custom:      return "Custom";
    }
    return "Unknown";
}

} // namespace BBRender
