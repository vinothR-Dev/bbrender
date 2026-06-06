#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <psapi.h>
  using SocketFd = SOCKET;
  #define INVALID_SOCK INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <sys/resource.h>
  #include <sys/wait.h>
  #ifdef __APPLE__
    #include <mach/mach.h>
    #include <sys/sysctl.h>
  #else
    #include <sys/sysinfo.h>
  #endif
  using SocketFd = int;
  #define INVALID_SOCK (-1)
#endif

#include "../../shared/types/bb_types.h"
#include "../network/network_server.h" // for Json helpers

namespace BBRender {

// ─── System info helpers ─────────────────────────────────────────────────────
namespace SysInfo {
    inline std::string hostname() {
        char buf[256] = {};
        ::gethostname(buf, sizeof(buf));
        return buf;
    }

    inline std::string platform() {
#ifdef BB_PLATFORM_WINDOWS
        return "windows";
#elif defined(BB_PLATFORM_MACOS)
        return "macos";
#else
        return "linux";
#endif
    }

    inline uint32_t cpuCores() {
        return std::max(1u, std::thread::hardware_concurrency());
    }

    inline uint64_t ramMB() {
#ifdef _WIN32
        MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        return ms.ullTotalPhys / (1024*1024);
#elif defined(__APPLE__)
        int64_t mem = 0; size_t sz = sizeof(mem);
        sysctlbyname("hw.memsize", &mem, &sz, nullptr, 0);
        return mem / (1024*1024);
#else
        struct sysinfo si{}; sysinfo(&si);
        return (uint64_t)si.totalram * si.mem_unit / (1024*1024);
#endif
    }

    inline double cpuUsage() {
        // Simplified: returns 0-100 estimate
#ifdef _WIN32
        static FILETIME prevIdle{}, prevKernel{}, prevUser{};
        FILETIME idle, kernel, user;
        GetSystemTimes(&idle, &kernel, &user);
        auto sub = [](FILETIME a, FILETIME b) -> uint64_t {
            return (((uint64_t)a.dwHighDateTime << 32) | a.dwLowDateTime)
                 - (((uint64_t)b.dwHighDateTime << 32) | b.dwLowDateTime);
        };
        uint64_t idleDiff   = sub(idle,   prevIdle);
        uint64_t kernelDiff = sub(kernel, prevKernel);
        uint64_t userDiff   = sub(user,   prevUser);
        prevIdle = idle; prevKernel = kernel; prevUser = user;
        uint64_t total = kernelDiff + userDiff;
        if (!total) return 0.0;
        return 100.0 * (total - idleDiff) / total;
#else
        return 0.0; // Platform-specific; use /proc/stat in production
#endif
    }
}

// ─── Process runner ──────────────────────────────────────────────────────────
struct ProcessHandle {
#ifdef _WIN32
    HANDLE hProcess = nullptr;
    HANDLE hThread  = nullptr;
    DWORD  pid      = 0;
#else
    pid_t pid = -1;
#endif
    bool valid() const {
#ifdef _WIN32
        return hProcess != nullptr;
#else
        return pid > 0;
#endif
    }
};

inline ProcessHandle launchProcess(const std::string& cmd, const std::string& args) {
    ProcessHandle h;
    std::string full = cmd + " " + args;
#ifdef _WIN32
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(full.begin(), full.end());
    cmdBuf.push_back(0);
    if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                       FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        h.hProcess = pi.hProcess;
        h.hThread  = pi.hThread;
        h.pid      = pi.dwProcessId;
    }
#else
    h.pid = fork();
    if (h.pid == 0) {
        // Child
        execl("/bin/sh", "sh", "-c", full.c_str(), nullptr);
        _exit(127);
    }
#endif
    return h;
}

inline int waitProcess(ProcessHandle& h) {
#ifdef _WIN32
    WaitForSingleObject(h.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(h.hProcess, &code);
    CloseHandle(h.hProcess);
    CloseHandle(h.hThread);
    return (int)code;
#else
    int status = 0;
    waitpid(h.pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

inline void killProcess(ProcessHandle& h) {
#ifdef _WIN32
    if (h.hProcess) TerminateProcess(h.hProcess, 1);
#else
    if (h.pid > 0) ::kill(h.pid, SIGTERM);
#endif
}

// ─── Render engine command builders ──────────────────────────────────────────
inline std::string buildRenderCommand(const JobInfo& job, const TaskInfo& task) {
    std::string cmd;
    switch (job.engine) {
        case RenderEngine::Nuke:
            cmd = job.enginePath.empty() ? "Nuke" : job.enginePath;
            cmd += " -x -f";
            if (!job.sceneFile.empty()) cmd += " \"" + job.sceneFile + "\"";
            cmd += " -F " + std::to_string(task.frameStart) + "-" + std::to_string(task.frameEnd);
            if (!job.outputPath.empty()) cmd += " -X Write1 \"" + job.outputPath + "\"";
            break;

        case RenderEngine::Silhouette:
            cmd = job.enginePath.empty() ? "silhouette" : job.enginePath;
            cmd += " -render";
            if (!job.sceneFile.empty()) cmd += " \"" + job.sceneFile + "\"";
            cmd += " -start " + std::to_string(task.frameStart);
            cmd += " -end "   + std::to_string(task.frameEnd);
            break;

        case RenderEngine::Blender:
            cmd = job.enginePath.empty() ? "blender" : job.enginePath;
            cmd += " -b \"" + job.sceneFile + "\"";
            cmd += " -o \""  + job.outputPath + "\"";
            cmd += " -s "    + std::to_string(task.frameStart);
            cmd += " -e "    + std::to_string(task.frameEnd);
            cmd += " -a";
            break;

        case RenderEngine::Maya:
            cmd = job.enginePath.empty() ? "Render" : job.enginePath;
            cmd += " -s " + std::to_string(task.frameStart);
            cmd += " -e " + std::to_string(task.frameEnd);
            if (!job.camera.empty()) cmd += " -cam " + job.camera;
            if (!job.outputPath.empty()) cmd += " -rd \"" + job.outputPath + "\"";
            cmd += " \"" + job.sceneFile + "\"";
            break;

        case RenderEngine::Houdini:
            cmd = job.enginePath.empty() ? "hython" : job.enginePath;
            cmd += " \"" + job.sceneFile + "\"";
            cmd += " -f " + std::to_string(task.frameStart)
                 + " "    + std::to_string(task.frameEnd);
            break;

        case RenderEngine::Custom:
            cmd = job.enginePath + " " + job.engineArgs;
            cmd += " -frame_start " + std::to_string(task.frameStart);
            cmd += " -frame_end "   + std::to_string(task.frameEnd);
            break;

        default:
            cmd = "echo Unsupported engine";
            break;
    }
    return cmd;
}

// ─── Worker agent ─────────────────────────────────────────────────────────────
class WorkerAgent {
public:
    WorkerAgent(const std::string& serverHost, uint16_t serverPort = 9876);
    ~WorkerAgent();

    bool connect();
    void disconnect();
    void run();  // blocking main loop
    bool isConnected() const { return m_connected.load(); }

    using LogCallback = std::function<void(const std::string&)>;
    void setLogCallback(LogCallback cb) { m_logCb = std::move(cb); }

private:
    void heartbeatLoop();
    void receiveLoop();
    void handleServerMessage(const NetMessage& msg);
    void handleTaskAssign(const std::string& payload);
    void executeTask(JobInfo job, TaskInfo task);
    void sendMessage(const NetMessage& msg);
    void sendHeartbeat();
    void sendTaskProgress(TaskID tid, float progress);
    void sendTaskComplete(TaskID tid);
    void sendTaskFail(TaskID tid, const std::string& error);
    void log(const std::string& msg);

    WorkerInfo buildWorkerInfo() const;

    std::string  m_serverHost;
    uint16_t     m_serverPort;
    SocketFd     m_fd = INVALID_SOCK;
    WorkerID     m_workerId = 0;

    std::atomic<bool>  m_connected{false};
    std::atomic<bool>  m_shouldStop{false};
    std::atomic<bool>  m_rendering{false};
    std::atomic<float> m_renderProgress{0.0f};
    std::atomic<TaskID> m_currentTask{0};

    std::mutex  m_sendMutex;
    std::mutex  m_recvBufMutex;
    std::vector<uint8_t> m_recvBuf;

    std::thread m_heartbeatThread;
    std::thread m_recvThread;
    std::thread m_renderThread;

    ProcessHandle m_currentProcess;
    std::mutex    m_processMutex;

    std::atomic<uint32_t> m_seqCounter{0};
    LogCallback m_logCb;
};

inline WorkerAgent::WorkerAgent(const std::string& host, uint16_t port)
    : m_serverHost(host), m_serverPort(port) {}

inline WorkerAgent::~WorkerAgent() {
    m_shouldStop = true;
    disconnect();
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
    if (m_recvThread.joinable()) m_recvThread.join();
    if (m_renderThread.joinable()) m_renderThread.join();
}

inline WorkerInfo WorkerAgent::buildWorkerInfo() const {
    WorkerInfo wi;
    wi.hostname   = SysInfo::hostname();
    wi.cpuCores   = SysInfo::cpuCores();
    wi.ramTotalMB = SysInfo::ramMB();
    wi.platform   = SysInfo::platform();
    wi.version    = "1.0.0";
    return wi;
}

inline bool WorkerAgent::connect() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == INVALID_SOCK) return false;

    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(m_serverHost.c_str(), std::to_string(m_serverPort).c_str(), &hints, &res) != 0)
        return false;

    if (::connect(m_fd, res->ai_addr, (int)res->ai_addrlen) != 0) {
        freeaddrinfo(res); return false;
    }
    freeaddrinfo(res);
    m_connected = true;

    // Register
    auto wi = buildWorkerInfo();
    NetMessage reg;
    reg.type = MsgType::WorkerRegister;
    reg.seq  = ++m_seqCounter;
    reg.payload = Json::obj({
        Json::kv("hostname",   wi.hostname),
        Json::kv("platform",   wi.platform),
        Json::kv("cpu_cores",  (int64_t)wi.cpuCores),
        Json::kv("ram_mb",     (int64_t)wi.ramTotalMB),
        Json::kv("gpu_count",  (int64_t)wi.gpuCount),
        Json::kv("version",    wi.version)
    });
    sendMessage(reg);

    m_recvThread      = std::thread([this]{ receiveLoop(); });
    m_heartbeatThread = std::thread([this]{ heartbeatLoop(); });

    log("Connected to BB Render Server " + m_serverHost + ":" + std::to_string(m_serverPort));
    return true;
}

inline void WorkerAgent::disconnect() {
    m_connected = false;
#ifdef _WIN32
    if (m_fd != INVALID_SOCK) { closesocket(m_fd); WSACleanup(); }
#else
    if (m_fd != INVALID_SOCK) ::close(m_fd);
#endif
    m_fd = INVALID_SOCK;
}

inline void WorkerAgent::run() {
    while (!m_shouldStop) {
        if (!m_connected) {
            log("Reconnecting in 5s...");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            connect();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

inline void WorkerAgent::receiveLoop() {
    constexpr size_t CHUNK = 4096;
    std::vector<uint8_t> buf(CHUNK);
    while (m_connected && !m_shouldStop) {
        int n = ::recv(m_fd, (char*)buf.data(), (int)buf.size(), 0);
        if (n <= 0) { m_connected = false; break; }
        {
            std::lock_guard<std::mutex> lk(m_recvBufMutex);
            m_recvBuf.insert(m_recvBuf.end(), buf.begin(), buf.begin() + n);
        }
        // Parse
        while (true) {
            std::lock_guard<std::mutex> lk(m_recvBufMutex);
            if (m_recvBuf.size() < 16) break;
            uint32_t sz; memcpy(&sz, m_recvBuf.data() + 12, 4); sz = ntohl(sz);
            if (m_recvBuf.size() < 16 + sz) break;
            NetMessage msg;
            uint16_t ty; memcpy(&ty, m_recvBuf.data() + 4, 2);
            msg.type = static_cast<MsgType>(ntohs(ty));
            if (sz > 0) msg.payload.assign(m_recvBuf.begin() + 16, m_recvBuf.begin() + 16 + sz);
            m_recvBuf.erase(m_recvBuf.begin(), m_recvBuf.begin() + 16 + sz);
            handleServerMessage(msg);
        }
    }
}

inline void WorkerAgent::heartbeatLoop() {
    while (m_connected && !m_shouldStop) {
        sendHeartbeat();
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

inline void WorkerAgent::sendHeartbeat() {
    NetMessage hb;
    hb.type = MsgType::WorkerHeartbeat;
    hb.seq  = ++m_seqCounter;
    hb.payload = Json::obj({
        Json::kv("cpu",      SysInfo::cpuUsage()),
        Json::kv("ram_used", (int64_t)0),
        Json::kv("gpu",      0.0),
        Json::kv("progress", (double)m_renderProgress.load())
    });
    sendMessage(hb);
}

inline void WorkerAgent::handleServerMessage(const NetMessage& msg) {
    switch (msg.type) {
        case MsgType::HelloAck:
            m_workerId = static_cast<WorkerID>(Json::getInt(msg.payload, "worker_id"));
            log("Registered as worker #" + std::to_string(m_workerId));
            break;
        case MsgType::TaskAssign:
            handleTaskAssign(msg.payload);
            break;
        case MsgType::JobCancel:
        case MsgType::Shutdown:
            m_shouldStop = true;
            break;
        default: break;
    }
}

inline void WorkerAgent::handleTaskAssign(const std::string& payload) {
    if (m_rendering) { log("Already rendering, ignoring task assign"); return; }

    // Parse minimal job/task info from payload
    JobInfo  job;
    TaskInfo task;
    job.id          = static_cast<JobID>(Json::getInt(payload, "job_id"));
    task.id         = static_cast<TaskID>(Json::getInt(payload, "task_id"));
    task.frameStart = static_cast<FrameNum>(Json::getInt(payload, "frame_start"));
    task.frameEnd   = static_cast<FrameNum>(Json::getInt(payload, "frame_end"));
    job.sceneFile   = Json::getStr(payload, "scene_file");
    job.outputPath  = Json::getStr(payload, "output_path");
    job.enginePath  = Json::getStr(payload, "engine_path");
    job.engineArgs  = Json::getStr(payload, "engine_args");
    job.engine      = static_cast<RenderEngine>(Json::getInt(payload, "engine"));

    m_currentTask  = task.id;
    m_renderThread = std::thread([this, job, task]{ executeTask(job, task); });
    m_renderThread.detach();
}

inline void WorkerAgent::executeTask(JobInfo job, TaskInfo task) {
    m_rendering       = true;
    m_renderProgress  = 0.0f;
    log("Starting render: frames " + std::to_string(task.frameStart)
        + "-" + std::to_string(task.frameEnd));

    std::string cmd = buildRenderCommand(job, task);
    log("CMD: " + cmd);

    auto startTime = std::chrono::steady_clock::now();
    ProcessHandle h = launchProcess("/bin/sh", "-c \"" + cmd + "\"");

    if (!h.valid()) {
        sendTaskFail(task.id, "Failed to launch render process");
        m_rendering = false;
        return;
    }

    {
        std::lock_guard<std::mutex> lk(m_processMutex);
        m_currentProcess = h;
    }

    int frames = task.frameEnd - task.frameStart + 1;
    // Simulate progress updates while process runs
    std::thread progressThread([&]{
        for (int f = 0; f < frames && m_rendering; ++f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            float p = static_cast<float>(f + 1) / frames;
            m_renderProgress = p;
            sendTaskProgress(task.id, p);
        }
    });

    int exitCode = waitProcess(h);
    m_rendering  = false;
    if (progressThread.joinable()) progressThread.join();

    if (exitCode == 0) {
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - startTime).count();
        log("Render complete in " + std::to_string(elapsed) + "s");
        sendTaskComplete(task.id);
    } else {
        sendTaskFail(task.id, "Render process exited with code " + std::to_string(exitCode));
    }
    m_renderProgress = 0.0f;
}

inline void WorkerAgent::sendMessage(const NetMessage& msg) {
    auto data = msg.serialize();
    std::lock_guard<std::mutex> lk(m_sendMutex);
    size_t sent = 0;
    while (sent < data.size() && m_connected) {
        int n = ::send(m_fd, (const char*)(data.data() + sent), (int)(data.size() - sent), 0);
        if (n <= 0) { m_connected = false; return; }
        sent += n;
    }
}

inline void WorkerAgent::sendTaskProgress(TaskID tid, float progress) {
    NetMessage msg;
    msg.type    = MsgType::TaskProgress;
    msg.seq     = ++m_seqCounter;
    msg.payload = Json::obj({Json::kv("task_id",(int64_t)tid), Json::kv("progress",(double)progress)});
    sendMessage(msg);
}

inline void WorkerAgent::sendTaskComplete(TaskID tid) {
    NetMessage msg;
    msg.type    = MsgType::TaskComplete;
    msg.seq     = ++m_seqCounter;
    msg.payload = Json::obj({Json::kv("task_id",(int64_t)tid)});
    sendMessage(msg);
}

inline void WorkerAgent::sendTaskFail(TaskID tid, const std::string& error) {
    NetMessage msg;
    msg.type    = MsgType::TaskFail;
    msg.seq     = ++m_seqCounter;
    msg.payload = Json::obj({Json::kv("task_id",(int64_t)tid), Json::kv("error",error)});
    sendMessage(msg);
}

inline void WorkerAgent::log(const std::string& msg) {
    if (m_logCb) m_logCb(msg);
}

} // namespace BBRender
