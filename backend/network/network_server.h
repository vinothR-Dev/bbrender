#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <string>
#include <queue>
#include <condition_variable>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketFd = SOCKET;
  #define INVALID_SOCK INVALID_SOCKET
  #define SOCK_ERR     SOCKET_ERROR
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  using SocketFd = int;
  #define INVALID_SOCK (-1)
  #define SOCK_ERR     (-1)
#endif

#include "../../shared/types/bb_types.h"
#include "../scheduler/scheduler.h"

namespace BBRender {

// ─── Connection ──────────────────────────────────────────────────────────────
struct Connection {
    SocketFd   fd       = INVALID_SOCK;
    WorkerID   workerId = 0;
    std::string remoteIP;
    uint16_t   remotePort = 0;
    std::atomic<bool> alive{true};
    std::mutex  sendMutex;
    std::vector<uint8_t> recvBuf;

    // Stats
    uint64_t bytesSent     = 0;
    uint64_t bytesReceived = 0;
    TimePoint connectedAt;
};

using ConnectionPtr = std::shared_ptr<Connection>;

// ─── Network server ──────────────────────────────────────────────────────────
class NetworkServer {
public:
    explicit NetworkServer(Scheduler& sched, uint16_t port = 9876);
    ~NetworkServer();

    bool start();
    void stop();
    bool isRunning() const { return m_running.load(); }

    // Broadcast to all connected workers
    void broadcast(const NetMessage& msg);
    // Send to specific worker
    bool sendTo(WorkerID id, const NetMessage& msg);

    uint32_t connectedCount() const;

    using LogCallback = std::function<void(const std::string&)>;
    void setLogCallback(LogCallback cb) { m_logCb = std::move(cb); }

private:
    void acceptLoop();
    void clientLoop(ConnectionPtr conn);
    void handleMessage(ConnectionPtr conn, const NetMessage& msg);
    void handleWorkerRegister(ConnectionPtr conn, const std::string& payload);
    void handleWorkerHeartbeat(ConnectionPtr conn, const std::string& payload);
    void handleTaskProgress(ConnectionPtr conn, const std::string& payload);
    void handleTaskComplete(ConnectionPtr conn, const std::string& payload);
    void handleTaskFail(ConnectionPtr conn, const std::string& payload);

    bool sendRaw(ConnectionPtr conn, const NetMessage& msg);
    void removeConnection(ConnectionPtr conn);
    void log(const std::string& msg);

    // Socket helpers
    static bool setNonBlocking(SocketFd fd);
    static bool setTcpNoDelay(SocketFd fd);
    static bool setKeepAlive(SocketFd fd);

    Scheduler&  m_scheduler;
    uint16_t    m_port;
    SocketFd    m_listenFd = INVALID_SOCK;
    std::atomic<bool> m_running{false};

    mutable std::mutex              m_connMutex;
    std::vector<ConnectionPtr>      m_connections;
    std::unordered_map<WorkerID, ConnectionPtr> m_workerConns;

    std::thread m_acceptThread;
    std::vector<std::thread> m_clientThreads;

    std::atomic<uint32_t> m_seqCounter{0};
    LogCallback m_logCb;
};

// ─── JSON helpers (minimal, no dependency) ───────────────────────────────────
namespace Json {
    inline std::string str(const std::string& s) {
        return "\"" + s + "\"";
    }
    inline std::string kv(const std::string& k, const std::string& v) {
        return "\"" + k + "\":" + "\"" + v + "\"";
    }
    inline std::string kv(const std::string& k, int64_t v) {
        return "\"" + k + "\":" + std::to_string(v);
    }
    inline std::string kv(const std::string& k, double v) {
        return "\"" + k + "\":" + std::to_string(v);
    }
    inline std::string kv(const std::string& k, bool v) {
        return "\"" + k + "\":" + (v ? "true" : "false");
    }
    inline std::string obj(std::initializer_list<std::string> fields) {
        std::string s = "{";
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            if (it != fields.begin()) s += ",";
            s += *it;
        }
        return s + "}";
    }
    // Minimal field extractor
    inline std::string getStr(const std::string& json, const std::string& key) {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return {};
        pos = json.find(':', pos); if (pos == std::string::npos) return {};
        pos = json.find('"', pos); if (pos == std::string::npos) return {};
        auto end = json.find('"', pos + 1); if (end == std::string::npos) return {};
        return json.substr(pos + 1, end - pos - 1);
    }
    inline int64_t getInt(const std::string& json, const std::string& key) {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        pos = json.find(':', pos); if (pos == std::string::npos) return 0;
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ')) ++pos;
        return std::stoll(json.substr(pos));
    }
    inline double getDbl(const std::string& json, const std::string& key) {
        auto pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0.0;
        pos = json.find(':', pos); if (pos == std::string::npos) return 0.0;
        while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ')) ++pos;
        return std::stod(json.substr(pos));
    }
}

// ─── NetMessage serialize/deserialize ────────────────────────────────────────
inline std::vector<uint8_t> NetMessage::serialize() const {
    std::vector<uint8_t> buf;
    buf.resize(12 + payload.size());
    auto* p = buf.data();
    // magic (4) + type (2) + seq (4) + size (4)
    uint32_t mg = htonl(magic);
    uint16_t ty = htons(static_cast<uint16_t>(type));
    uint32_t sq = htonl(seq);
    uint32_t sz = htonl(static_cast<uint32_t>(payload.size()));
    memcpy(p,      &mg, 4); p += 4;
    memcpy(p,      &ty, 2); p += 2;
    // padding 2 bytes
    p[0] = 0; p[1] = 0; p += 2;
    memcpy(p,      &sq, 4); p += 4;
    memcpy(p,      &sz, 4); p += 4;
    if (!payload.empty())
        memcpy(buf.data() + 16, payload.data(), payload.size());
    buf.resize(16 + payload.size());
    return buf;
}

// ─── NetworkServer implementation ────────────────────────────────────────────
inline NetworkServer::NetworkServer(Scheduler& sched, uint16_t port)
    : m_scheduler(sched), m_port(port) {}

inline NetworkServer::~NetworkServer() { stop(); }

inline bool NetworkServer::start() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    m_listenFd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenFd == INVALID_SOCK) { log("Failed to create socket"); return false; }

    int opt = 1;
#ifdef _WIN32
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(m_port);

    if (::bind(m_listenFd, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        log("Bind failed on port " + std::to_string(m_port)); return false;
    }
    if (::listen(m_listenFd, 64) == SOCK_ERR) {
        log("Listen failed"); return false;
    }

    setNonBlocking(m_listenFd);
    m_running = true;
    m_acceptThread = std::thread([this]{ acceptLoop(); });
    log("BB Render Server listening on port " + std::to_string(m_port));
    return true;
}

inline void NetworkServer::stop() {
    m_running = false;
#ifdef _WIN32
    if (m_listenFd != INVALID_SOCK) closesocket(m_listenFd);
    WSACleanup();
#else
    if (m_listenFd != INVALID_SOCK) ::close(m_listenFd);
#endif
    m_listenFd = INVALID_SOCK;
    if (m_acceptThread.joinable()) m_acceptThread.join();
}

inline void NetworkServer::acceptLoop() {
    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t   addrLen = sizeof(clientAddr);
        SocketFd clientFd = ::accept(m_listenFd, (sockaddr*)&clientAddr, &addrLen);
        if (clientFd == INVALID_SOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        setTcpNoDelay(clientFd);
        setKeepAlive(clientFd);

        auto conn = std::make_shared<Connection>();
        conn->fd          = clientFd;
        conn->remoteIP    = inet_ntoa(clientAddr.sin_addr);
        conn->remotePort  = ntohs(clientAddr.sin_port);
        conn->connectedAt = TimePoint::now();

        {
            std::lock_guard<std::mutex> lk(m_connMutex);
            m_connections.push_back(conn);
        }

        m_clientThreads.emplace_back([this, conn]{ clientLoop(conn); });
        m_clientThreads.back().detach();
        log("Worker connected: " + conn->remoteIP + ":" + std::to_string(conn->remotePort));
    }
}

inline void NetworkServer::clientLoop(ConnectionPtr conn) {
    constexpr size_t CHUNK = 4096;
    std::vector<uint8_t> buf(CHUNK);

    while (m_running && conn->alive) {
        int n = ::recv(conn->fd, (char*)buf.data(), (int)buf.size(), 0);
        if (n <= 0) { conn->alive = false; break; }

        conn->recvBuf.insert(conn->recvBuf.end(), buf.begin(), buf.begin() + n);
        conn->bytesReceived += n;

        // Parse complete messages (header = 16 bytes)
        while (conn->recvBuf.size() >= 16) {
            uint32_t mg; memcpy(&mg, conn->recvBuf.data(), 4);
            if (ntohl(mg) != NetMessage::MAGIC) { conn->alive = false; break; }
            uint32_t sz; memcpy(&sz, conn->recvBuf.data() + 12, 4);
            sz = ntohl(sz);
            if (conn->recvBuf.size() < 16 + sz) break;

            NetMessage msg;
            msg.magic = NetMessage::MAGIC;
            uint16_t ty; memcpy(&ty, conn->recvBuf.data() + 4, 2);
            msg.type = static_cast<MsgType>(ntohs(ty));
            uint32_t sq; memcpy(&sq, conn->recvBuf.data() + 8, 4);
            msg.seq  = ntohl(sq);
            if (sz > 0)
                msg.payload.assign(conn->recvBuf.begin() + 16,
                                   conn->recvBuf.begin() + 16 + sz);
            conn->recvBuf.erase(conn->recvBuf.begin(), conn->recvBuf.begin() + 16 + sz);
            handleMessage(conn, msg);
        }
    }
    removeConnection(conn);
}

inline void NetworkServer::handleMessage(ConnectionPtr conn, const NetMessage& msg) {
    switch (msg.type) {
        case MsgType::WorkerRegister: handleWorkerRegister(conn, msg.payload); break;
        case MsgType::WorkerHeartbeat:handleWorkerHeartbeat(conn, msg.payload); break;
        case MsgType::TaskProgress:   handleTaskProgress(conn, msg.payload);   break;
        case MsgType::TaskComplete:   handleTaskComplete(conn, msg.payload);    break;
        case MsgType::TaskFail:       handleTaskFail(conn, msg.payload);        break;
        default: break;
    }
}

inline void NetworkServer::handleWorkerRegister(ConnectionPtr conn, const std::string& p) {
    WorkerInfo wi;
    wi.hostname   = Json::getStr(p, "hostname");
    wi.ipAddress  = conn->remoteIP;
    wi.port       = static_cast<uint16_t>(Json::getInt(p, "port"));
    wi.platform   = Json::getStr(p, "platform");
    wi.osVersion  = Json::getStr(p, "os_version");
    wi.cpuCores   = static_cast<uint32_t>(Json::getInt(p, "cpu_cores"));
    wi.cpuModel   = Json::getStr(p, "cpu_model");
    wi.ramTotalMB = static_cast<uint64_t>(Json::getInt(p, "ram_mb"));
    wi.gpuCount   = static_cast<uint32_t>(Json::getInt(p, "gpu_count"));
    wi.gpuModel   = Json::getStr(p, "gpu_model");
    wi.vramMB     = static_cast<uint64_t>(Json::getInt(p, "vram_mb"));
    wi.version    = Json::getStr(p, "version");

    WorkerID wid = m_scheduler.registerWorker(wi);
    conn->workerId = wid;

    {
        std::lock_guard<std::mutex> lk(m_connMutex);
        m_workerConns[wid] = conn;
    }

    // Send ack
    NetMessage ack;
    ack.type    = MsgType::HelloAck;
    ack.seq     = ++m_seqCounter;
    ack.payload = Json::obj({Json::kv("worker_id", (int64_t)wid), Json::kv("status","ok")});
    sendRaw(conn, ack);
    log("Worker registered: " + wi.hostname + " id=" + std::to_string(wid));
}

inline void NetworkServer::handleWorkerHeartbeat(ConnectionPtr conn, const std::string& p) {
    if (!conn->workerId) return;
    RenderStats stats;
    stats.cpuPercent  = Json::getDbl(p, "cpu");
    stats.ramUsedMB   = static_cast<uint64_t>(Json::getInt(p, "ram_used"));
    stats.gpuPercent  = Json::getDbl(p, "gpu");
    stats.vramUsedMB  = static_cast<uint64_t>(Json::getInt(p, "vram_used"));
    m_scheduler.updateWorkerStats(conn->workerId, stats);
    m_scheduler.updateWorkerStatus(conn->workerId, WorkerStatus::Rendering,
        static_cast<float>(Json::getDbl(p, "progress")));
}

inline void NetworkServer::handleTaskProgress(ConnectionPtr conn, const std::string& p) {
    if (!conn->workerId) return;
    float progress = static_cast<float>(Json::getDbl(p, "progress"));
    m_scheduler.updateWorkerStatus(conn->workerId, WorkerStatus::Rendering, progress);
}

inline void NetworkServer::handleTaskComplete(ConnectionPtr conn, const std::string& p) {
    if (!conn->workerId) return;
    TaskID tid = static_cast<TaskID>(Json::getInt(p, "task_id"));
    m_scheduler.workerTaskComplete(conn->workerId, tid, true);
    m_scheduler.updateWorkerStatus(conn->workerId, WorkerStatus::Idle);
}

inline void NetworkServer::handleTaskFail(ConnectionPtr conn, const std::string& p) {
    if (!conn->workerId) return;
    TaskID tid = static_cast<TaskID>(Json::getInt(p, "task_id"));
    std::string err = Json::getStr(p, "error");
    m_scheduler.workerTaskComplete(conn->workerId, tid, false, err);
    m_scheduler.updateWorkerStatus(conn->workerId, WorkerStatus::Idle);
}

inline bool NetworkServer::sendRaw(ConnectionPtr conn, const NetMessage& msg) {
    auto data = msg.serialize();
    std::lock_guard<std::mutex> lk(conn->sendMutex);
    size_t sent = 0;
    while (sent < data.size()) {
        int n = ::send(conn->fd, (const char*)(data.data() + sent), (int)(data.size() - sent), 0);
        if (n <= 0) { conn->alive = false; return false; }
        sent += n;
        conn->bytesSent += n;
    }
    return true;
}

inline bool NetworkServer::sendTo(WorkerID id, const NetMessage& msg) {
    std::lock_guard<std::mutex> lk(m_connMutex);
    auto it = m_workerConns.find(id);
    if (it == m_workerConns.end()) return false;
    return sendRaw(it->second, msg);
}

inline void NetworkServer::broadcast(const NetMessage& msg) {
    std::lock_guard<std::mutex> lk(m_connMutex);
    for (auto& conn : m_connections) if (conn->alive) sendRaw(conn, msg);
}

inline void NetworkServer::removeConnection(ConnectionPtr conn) {
    if (conn->workerId) {
        m_scheduler.removeWorker(conn->workerId);
        std::lock_guard<std::mutex> lk(m_connMutex);
        m_workerConns.erase(conn->workerId);
    }
    std::lock_guard<std::mutex> lk(m_connMutex);
    m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), conn),
                        m_connections.end());
    log("Worker disconnected: " + conn->remoteIP);
}

inline uint32_t NetworkServer::connectedCount() const {
    std::lock_guard<std::mutex> lk(m_connMutex);
    return static_cast<uint32_t>(m_connections.size());
}

inline void NetworkServer::log(const std::string& msg) {
    if (m_logCb) m_logCb(msg);
}

inline bool NetworkServer::setNonBlocking(SocketFd fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

inline bool NetworkServer::setTcpNoDelay(SocketFd fd) {
    int flag = 1;
#ifdef _WIN32
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag)) == 0;
#else
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
#endif
}

inline bool NetworkServer::setKeepAlive(SocketFd fd) {
    int flag = 1;
#ifdef _WIN32
    return setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char*)&flag, sizeof(flag)) == 0;
#else
    return setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) == 0;
#endif
}

} // namespace BBRender
