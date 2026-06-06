# ⬛ BB Render Farm v1.0

> **High-performance distributed render farm manager**  
> Built with Qt6 C++ frontend + C++20 backend  
> Cross-platform: Windows · Linux · macOS

---

## Overview

BB Render Farm is a professional-grade distributed rendering solution inspired by Thinkbox Deadline and SideFX Houdini Farm. It coordinates render jobs across any number of machines, with full support for:

| Engine | Command |
|--------|---------|
| **Nuke** | `Nuke -x -f scene.nk -F 1-100` |
| **Silhouette** | `silhouette -render scene.sfx -start 1 -end 100` |
| **Blender** | `blender -b scene.blend -o /out/ -s 1 -e 100 -a` |
| **Houdini** | `hython scene.hip -f 1 100` |
| **Maya** | `Render -s 1 -e 100 scene.ma` |
| **Cinema4D** | `Cinema4D -render scene.c4d` |
| **After Effects** | `aerender -project comp.aep -s 1 -e 100` |
| **Custom** | any executable with custom args |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                   BB RENDER FARM SYSTEM                         │
│                                                                 │
│  ┌──────────────────────────────────┐                           │
│  │   BBRenderFarm  (Qt6 GUI)        │  ← Main Desktop App      │
│  │   ┌─────────────────────────┐    │                           │
│  │   │  Job Queue Tab          │    │                           │
│  │   │  Worker Monitor Tab     │    │                           │
│  │   │  Dashboard / Stats Tab  │    │                           │
│  │   │  System Log Tab         │    │                           │
│  │   └─────────────────────────┘    │                           │
│  │         │ embeds                 │                           │
│  │   ┌─────▼──────────────────┐     │                           │
│  │   │  Scheduler (C++20)     │     │                           │
│  │   │  • Priority queue      │     │                           │
│  │   │  • Task decomposition  │     │                           │
│  │   │  • Worker matching     │     │                           │
│  │   │  • Retry logic         │     │                           │
│  │   └─────────────────────────┘    │                           │
│  │         │ TCP/9876               │                           │
│  └─────────┼────────────────────────┘                           │
│            │ Network (binary protocol)                          │
│    ┌───────┴──────────────────────────────┐                     │
│    │                                      │                     │
│  ┌─▼──────────────┐   ┌──────────────────▼─┐                   │
│  │ BBRenderWorker │   │  BBRenderWorker     │   (N workers)     │
│  │  render-node-1 │   │   render-node-2     │                   │
│  │  ┌──────────┐  │   │  ┌──────────────┐  │                   │
│  │  │ Nuke     │  │   │  │ Blender      │  │                   │
│  │  │ process  │  │   │  │ process      │  │                   │
│  │  └──────────┘  │   │  └──────────────┘  │                   │
│  └────────────────┘   └────────────────────┘                   │
└─────────────────────────────────────────────────────────────────┘

  OR: Run BBRenderServer (headless) on a dedicated machine
      and connect BBRenderFarm GUI remotely.
```

---

## Directory Structure

```
BBRender/
├── CMakeLists.txt              # Root CMake config
├── main.cpp                    # Qt app entry point (splash + main window)
│
├── shared/
│   └── types/
│       └── bb_types.h          # All shared types: JobInfo, WorkerInfo, etc.
│
├── backend/
│   ├── CMakeLists.txt
│   ├── server_main.cpp         # Headless farm server entry
│   ├── worker_main.cpp         # Worker agent entry point
│   ├── scheduler/
│   │   └── scheduler.h         # Core scheduling engine (priority queue)
│   ├── network/
│   │   └── network_server.h    # TCP server, binary protocol, JSON I/O
│   └── core/
│       └── worker_agent.h      # Worker: connects, receives tasks, launches renders
│
├── frontend/
│   ├── CMakeLists.txt
│   ├── ui/
│   │   ├── mainwindow.h/.cpp   # Main Qt window
│   ├── widgets/
│   │   ├── job_queue_widget.h      # Job list with status badges + progress
│   │   ├── worker_monitor_widget.h # Worker grid with CPU/RAM/GPU meters
│   │   ├── farm_stats_widget.h     # KPI tiles + sparkline charts
│   │   ├── log_widget.h            # Color-coded system log
│   │   └── job_detail_widget.h     # Right-panel task breakdown
│   └── dialogs/
│       └── job_submit_dialog.h     # 4-tab job submission dialog
│
└── scripts/
    ├── build_linux_mac.sh      # Linux / macOS build
    └── build_windows.ps1       # Windows PowerShell build
```

---

## Prerequisites

### All platforms
- **CMake** ≥ 3.20
- **C++20** compiler (GCC 10+, Clang 12+, MSVC 2022)
- **Qt6** ≥ 6.2 (Core, Widgets, Network, Charts, Concurrent)
- **OpenSSL** (optional, for TLS — `-DBBRENDER_USE_OPENSSL=OFF` to disable)
- **Ninja** (recommended) or Make/MSBuild

### Windows
```
winget install Kitware.CMake Ninja-build.Ninja
# Install Qt 6.x from https://www.qt.io/download-qt-installer
# Install Visual Studio 2022 with C++ workload
```

### Linux (Ubuntu/Debian)
```bash
sudo apt install cmake ninja-build gcc g++ \
    qt6-base-dev qt6-charts-dev libqt6concurrent6 \
    libssl-dev
```

### Linux (Fedora/RHEL)
```bash
sudo dnf install cmake ninja-build gcc-c++ \
    qt6-qtbase-devel qt6-qtcharts-devel \
    openssl-devel
```

### macOS
```bash
brew install cmake ninja qt6 openssl
export Qt6_DIR=$(brew --prefix qt6)/lib/cmake/Qt6
```

---

## Build

### Linux / macOS
```bash
chmod +x scripts/build_linux_mac.sh
./scripts/build_linux_mac.sh Release
```

### Windows (PowerShell)
```powershell
.\scripts\build_windows.ps1 -BuildType Release -QtPath "C:\Qt\6.7.0\msvc2019_64\lib\cmake\Qt6"
```

### Manual CMake
```bash
mkdir build && cd build
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR=/path/to/Qt6/cmake/Qt6
cmake --build . --parallel
```

---

## Deployment

### Mode 1 — All-in-one (GUI embeds server)
```bash
# Open the GUI — it contains the scheduler internally
./BBRenderFarm

# Click "▶ Start Server" in the toolbar to accept worker connections
# Workers connect to this machine's IP on port 9876
```

### Mode 2 — Dedicated server (headless)
```bash
# On your server machine:
./BBRenderServer --port 9876

# On each render node:
./BBRenderWorker --server 192.168.1.100 --port 9876

# On your workstation (GUI only — connects remotely):
./BBRenderFarm
# Farm > Connect to Farm > enter server IP
```

---

## Network Protocol

Binary framing over TCP:

```
┌────────┬────────┬────────┬──────────────────┐
│ MAGIC  │  TYPE  │  SEQ   │ PAYLOAD SIZE (4B) │ → 16-byte header
│ 4B     │  2B+2B │  4B    │                  │
└────────┴────────┴────────┴──────────────────┘
│                PAYLOAD (JSON)                │ → variable
└─────────────────────────────────────────────┘
```

Message types: `WorkerRegister`, `HelloAck`, `TaskAssign`, `TaskProgress`, `TaskComplete`, `TaskFail`, `WorkerHeartbeat`, `FarmStats`

---

## Performance Notes

- **Scheduler** runs on a dedicated thread pool (configurable, default = `hw_concurrency`)
- **Lock contention** minimized — scheduler uses a single mutex; network I/O is per-connection
- **Task chunking** — default 5 frames/task; reduce to 1 for heavy shots, increase for lightweight passes
- **Heartbeat** every 2 seconds per worker; dead workers are automatically detected and their tasks re-queued
- **TCP_NODELAY** + **SO_KEEPALIVE** enabled on all connections for low-latency dispatch
- The GUI refresh timer is 1 second; increase in Settings for very large farms

---

## Adding a Custom Engine

1. Add a new value to `RenderEngine` enum in `shared/types/bb_types.h`
2. Add a case to `buildRenderCommand()` in `backend/core/worker_agent.h`
3. Add the engine to the combo box in `frontend/dialogs/job_submit_dialog.h`
4. Recompile

---

## License

© 2025 BB Render. All rights reserved.  
Internal use only. Not for redistribution.
