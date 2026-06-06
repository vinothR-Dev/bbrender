# BB Render Farm — Windows Build Script (PowerShell)
# Usage: .\build_windows.ps1 [Release|Debug]

param(
    [string]$BuildType = "Release",
    [string]$QtPath    = ""
)

$RootDir  = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RootDir "build_win"

Write-Host ""
Write-Host "╔═══════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   BB RENDER FARM — Windows Build v1.0     ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Root:       $RootDir"
Write-Host "  Build:      $BuildDir"
Write-Host "  Type:       $BuildType"
Write-Host ""

# Check cmake
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found. Install from https://cmake.org"
    exit 1
}

# Auto-detect Qt if not provided
if (-not $QtPath) {
    $candidates = @(
        "C:\Qt\6.7.0\msvc2019_64\lib\cmake\Qt6",
        "C:\Qt\6.6.0\msvc2019_64\lib\cmake\Qt6",
        "C:\Qt\6.5.0\msvc2019_64\lib\cmake\Qt6",
        "$env:USERPROFILE\Qt\6.7.0\msvc2019_64\lib\cmake\Qt6"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $QtPath = $c; break }
    }
}

if (-not $QtPath) {
    Write-Warning "Qt6_DIR not found. Set -QtPath to your Qt6 cmake dir."
    Write-Warning "Example: -QtPath C:\Qt\6.7.0\msvc2019_64\lib\cmake\Qt6"
}

# Create build dir
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Set-Location $BuildDir

# Configure
Write-Host "[1/3] Configuring..." -ForegroundColor Yellow
$cmakeArgs = @(
    $RootDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DBBRENDER_BUILD_FRONTEND=ON",
    "-DBBRENDER_BUILD_BACKEND=ON"
)
if ($QtPath) { $cmakeArgs += "-DQt6_DIR=$QtPath" }

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed"; exit 1 }

# Build
Write-Host ""
Write-Host "[2/3] Building..." -ForegroundColor Yellow
$cores = (Get-WmiObject Win32_Processor).NumberOfLogicalProcessors
& cmake --build . --config $BuildType --parallel $cores
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

Write-Host ""
Write-Host "[3/3] Done!" -ForegroundColor Green
Write-Host ""
Write-Host "  Binaries in: $BuildDir\bin\$BuildType\"
Write-Host "    BBRenderFarm.exe   — Qt GUI"
Write-Host "    BBRenderServer.exe — Headless server"
Write-Host "    BBRenderWorker.exe — Worker agent"
Write-Host ""
Write-Host "  Quick start:"
Write-Host "    BBRenderServer.exe --port 9876"
Write-Host "    BBRenderWorker.exe --server <IP> --port 9876"
Write-Host "    BBRenderFarm.exe"
Write-Host ""
