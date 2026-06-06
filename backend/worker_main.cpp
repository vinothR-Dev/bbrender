#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include "core/worker_agent.h"

static std::atomic<bool> g_stop{false};
static BBRender::WorkerAgent* g_agent = nullptr;

void signalHandler(int sig) {
    std::cout << "\n[BBRenderWorker] Signal " << sig << " — shutting down...\n";
    g_stop = true;
    if (g_agent) g_agent->disconnect();
}

void printBanner(const std::string& host, uint16_t port) {
    std::cout << R"(
 ╔══════════════════════════════════════════╗
 ║    BB RENDER FARM  —  Worker Agent v1.0  ║
 ╚══════════════════════════════════════════╝
)" << "\n";
    std::cout << "[INFO] Hostname:  " << BBRender::SysInfo::hostname()  << "\n";
    std::cout << "[INFO] Platform:  " << BBRender::SysInfo::platform()  << "\n";
    std::cout << "[INFO] CPU Cores: " << BBRender::SysInfo::cpuCores()  << "\n";
    std::cout << "[INFO] RAM:       " << BBRender::SysInfo::ramMB() / 1024 << " GB\n";
    std::cout << "[INFO] Server:    " << host << ":" << port << "\n\n";
}

int main(int argc, char* argv[]) {
    std::string serverHost = "127.0.0.1";
    uint16_t    serverPort = 9876;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--server" || arg == "-s") && i + 1 < argc)
            serverHost = argv[++i];
        else if ((arg == "--port" || arg == "-p") && i + 1 < argc)
            serverPort = (uint16_t)std::stoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: BBRenderWorker [--server HOST] [--port PORT]\n"
                      << "  --server, -s HOST   Farm server address (default: 127.0.0.1)\n"
                      << "  --port,   -p PORT   Farm server port   (default: 9876)\n";
            return 0;
        }
    }

    printBanner(serverHost, serverPort);
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    BBRender::WorkerAgent agent(serverHost, serverPort);
    g_agent = &agent;

    agent.setLogCallback([](const std::string& msg) {
        std::cout << "[WORKER] " << msg << "\n" << std::flush;
    });

    // Keep trying to connect
    while (!g_stop) {
        if (!agent.isConnected()) {
            std::cout << "[WORKER] Connecting to " << serverHost << ":" << serverPort << "...\n";
            if (agent.connect()) {
                std::cout << "[WORKER] Connected. Waiting for tasks...\n";
                agent.run(); // blocks until disconnected
            } else {
                std::cout << "[WORKER] Connection failed. Retrying in 5s...\n";
                for (int i = 0; i < 50 && !g_stop; ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    std::cout << "[WORKER] Worker agent stopped.\n";
    return 0;
}
