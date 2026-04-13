# ---------------------------------------------------------
# Daily Focus Pro - Build Script
# ---------------------------------------------------------
# HOW TO COMPILE MANUALLY:
# g++ main.cpp win32_helper.cpp -o TimerApp.exe -I. -IC:\raylib\raylib\src -LC:\raylib\raylib\src -lraylib -lgdi32 -lwinmm -lshell32 -mwindows -O2
# ---------------------------------------------------------

$W64DEVKIT_BIN = "C:\raylib\w64devkit\bin"

# Ensure the toolchain is in the PATH for this session
$env:PATH = "$W64DEVKIT_BIN;" + $env:PATH

$RAYLIB_DIR = "C:\raylib\raylib\src"
$OUTPUT = "TimerApp.exe"
$SRCS = "main.cpp", "platform_helper.cpp"

Write-Host "Compiling Daily Focus Pro (Modernized UI)..." -ForegroundColor Cyan

# Compile with optimization and windows mode (no console)
& "$W64DEVKIT_BIN\g++.exe" $SRCS -o $OUTPUT `
    -I "." `
    -I "$RAYLIB_DIR" `
    -L "$RAYLIB_DIR" `
    -lraylib -lgdi32 -lwinmm -lshell32 `
    -O2 -Wall `
    -mwindows

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Successful! Run .\$OUTPUT to start." -ForegroundColor Green
} else {
    Write-Host "Build Failed! Check for errors above." -ForegroundColor Red
}
