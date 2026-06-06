#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include "scheduler/scheduler.h"
#include "network/network_server.h"

static std::atomic<bool> g_running{true};

void signalHandler(int sig) {
    std::cout << "\n[BBRenderServer] Signal " << sig << " received. Shutting down...\n";
    g_running = false;
}

void printBanner() {
    std::cout << R"(
 ╔══════════════════════════════════════════════════════════╗
 ║           BB RENDER FARM  —  Server Daemon v1.0          ║
 ║        Nuke | Silhouette | Blender | Houdini | Maya      ║
 ╚══════════════════════════════════════════════════════════╝
)" << std::endl;
}

void printStats(const BBRender::FarmStats& s) {
    std::cout << "\r[FARM]"
              << "  Workers: " << s.busyWorkers << "/" << s.onlineWorkers
              << "  Jobs: "    << s.activeJobs  << " active, " << s.pendingJobs << " queued"
              << "  Frames: "  << s.framesRenderedToday << "/day"
              << "  Eff: "     << (int)(s.farmEfficiency * 100) << "%"
              << "  CPU: "     << (int)s.avgCpuPercent << "%"
              << std::flush;
}

int main(int argc, char* argv[]) {
    uint16_t port = 9876;

    // Simple CLI parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc)
            port = (uint16_t)std::stoi(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: BBRenderServer [--port PORT]\n"
                      << "  --port, -p PORT   Listen port (default: 9876)\n";
            return 0;
        }
    }

    printBanner();
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Init scheduler
    BBRender::Scheduler scheduler(std::thread::hardware_concurrency());

    scheduler.addEventCallback([](const BBRender::SchedulerEvent& ev) {
        using T = BBRender::SchedulerEvent::Type;
        switch (ev.type) {
            case T::JobStarted:
                std::cout << "\n[JOB]    #" << ev.jobId << " STARTED\n"; break;
            case T::JobCompleted:
                std::cout << "\n[JOB] ✓  #" << ev.jobId << " COMPLETED\n"; break;
            case T::JobFailed:
                std::cout << "\n[JOB] ✗  #" << ev.jobId << " FAILED\n"; break;
            case T::WorkerJoined:
                std::cout << "\n[NODE]   Worker #" << ev.workerId << " connected\n"; break;
            case T::WorkerLeft:
                std::cout << "\n[NODE]   Worker #" << ev.workerId << " disconnected\n"; break;
            case T::TaskAssigned:
                std::cout << "\n[TASK]   Task #" << ev.taskId
                          << " → Worker #" << ev.workerId << "\n"; break;
            case T::TaskFailed:
                std::cout << "\n[TASK] ✗ Task #" << ev.taskId << " FAILED\n"; break;
            default: break;
        }
    });

    scheduler.start();
    std::cout << "[OK]  Scheduler started with "
              << std::thread::hardware_concurrency() << " threads\n";

    // Init network server
    BBRender::NetworkServer server(scheduler, port);
    server.setLogCallback([](const std::string& msg) {
        std::cout << "[NET] " << msg << "\n";
    });

    if (!server.start()) {
        std::cerr << "[ERROR] Failed to start server on port " << port << "\n";
        return 1;
    }
    std::cout << "[OK]  Server listening on port " << port << "\n";
    std::cout << "[OK]  BB Render Farm ready. Press Ctrl+C to stop.\n\n";

    // Main loop — print stats every second
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto stats = scheduler.getFarmStats();
        printStats(stats);
    }

    std::cout << "\n[OK]  Stopping server...\n";
    server.stop();
    scheduler.stop();
    std::cout << "[OK]  BB Render Server stopped cleanly.\n";
    return 0;
}
