#!/usr/bin/env bash
# ─── BB Render Farm — Linux/macOS Build Script ──────────────────────────────
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BUILD_TYPE="${1:-Release}"

echo ""
echo "╔═══════════════════════════════════════════╗"
echo "║   BB RENDER FARM — Build Script v1.0      ║"
echo "╚═══════════════════════════════════════════╝"
echo ""
echo "  Root:       $ROOT_DIR"
echo "  Build:      $BUILD_DIR"
echo "  Type:       $BUILD_TYPE"
echo "  Platform:   $(uname -s)"
echo ""

# Check dependencies
check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "[ERROR] '$1' not found. Please install it."
        exit 1
    fi
}
check_dep cmake
check_dep ninja || check_dep make

# Find Qt
if [ -z "$Qt6_DIR" ]; then
    # Common install locations
    for p in \
        "$HOME/Qt/6.*/gcc_64/lib/cmake/Qt6" \
        "$HOME/Qt/6.*/macos/lib/cmake/Qt6" \
        "/usr/lib/x86_64-linux-gnu/cmake/Qt6" \
        "/opt/homebrew/opt/qt6/lib/cmake/Qt6" \
        "/usr/local/lib/cmake/Qt6"
    do
        match=$(echo $p 2>/dev/null | head -1)
        if [ -d "$match" ]; then
            export Qt6_DIR="$match"
            break
        fi
    done
fi

if [ -z "$Qt6_DIR" ]; then
    echo "[WARN] Qt6_DIR not set. CMake will try to auto-detect Qt6."
    echo "       If build fails, set: export Qt6_DIR=/path/to/Qt/6.x.x/gcc_64/lib/cmake/Qt6"
fi

# Create build dir
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Detect generator
if command -v ninja &>/dev/null; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

echo "[1/3] Configuring with CMake..."
cmake "$ROOT_DIR" \
    -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBBRENDER_BUILD_FRONTEND=ON \
    -DBBRENDER_BUILD_BACKEND=ON \
    ${Qt6_DIR:+-DQt6_DIR="$Qt6_DIR"} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo ""
echo "[2/3] Building..."
cmake --build . --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "[3/3] Done!"
echo ""
echo "  Binaries in: $BUILD_DIR/bin/"
echo "    BBRenderFarm   — Qt GUI application"
echo "    BBRenderServer — Headless farm server"
echo "    BBRenderWorker — Render node worker agent"
echo ""
echo "  Quick start:"
echo "    # Start the server:"
echo "    $BUILD_DIR/bin/BBRenderServer --port 9876"
echo ""
echo "    # Start worker agents on each render node:"
echo "    $BUILD_DIR/bin/BBRenderWorker --server <SERVER_IP> --port 9876"
echo ""
echo "    # Open the GUI:"
echo "    $BUILD_DIR/bin/BBRenderFarm"
echo ""
