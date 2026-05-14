@echo off
REM Download VMA (Vulkan Memory Allocator) header file
REM This script downloads vk_mem_alloc.h from GitHub

setlocal enabledelayedexpansion

echo Downloading Vulkan Memory Allocator (VMA) header...

set VMA_URL=https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h
set VMA_DIR=%~dp0third_party\VulkanMemoryAllocator
set VMA_FILE=!VMA_DIR!\vk_mem_alloc.h

REM Create directory if it doesn't exist
if not exist "!VMA_DIR!" (
    echo Creating directory: !VMA_DIR!
    mkdir "!VMA_DIR!"
)

REM Download using PowerShell (built-in on Windows)
powershell -NoProfile -Command "Invoke-WebRequest -Uri '%VMA_URL%' -OutFile '%VMA_FILE%' -ErrorAction Stop; exit 0" || goto error

echo.
echo ✓ VMA header file installed at: !VMA_FILE!
echo.
echo Next steps:
echo 1. cd build
echo 2. cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
echo 3. cmake --build .
exit /b 0

:error
echo.
echo ✗ Failed to download VMA header
echo.
echo Manual fallback:
echo 1. Download from: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases
echo 2. Extract vk_mem_alloc.h to: !VMA_DIR!
echo.
exit /b 1
