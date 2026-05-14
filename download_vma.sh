#!/bin/bash
# Download VMA (Vulkan Memory Allocator) header file
# This script downloads vk_mem_alloc.h from GitHub

echo "Downloading Vulkan Memory Allocator (VMA) header..."

VMA_URL="https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/master/include/vk_mem_alloc.h"
VMA_DIR="$(cd "$(dirname "$0")" && pwd)/third_party/VulkanMemoryAllocator"
VMA_FILE="$VMA_DIR/vk_mem_alloc.h"

# Create directory if it doesn't exist
if [ ! -d "$VMA_DIR" ]; then
    echo "Creating directory: $VMA_DIR"
    mkdir -p "$VMA_DIR"
fi

# Download using curl or wget
if command -v curl &> /dev/null; then
    echo "Using curl to download..."
    curl -L "$VMA_URL" -o "$VMA_FILE"
    RESULT=$?
elif command -v wget &> /dev/null; then
    echo "Using wget to download..."
    wget "$VMA_URL" -O "$VMA_FILE"
    RESULT=$?
else
    echo "✗ Error: Neither curl nor wget found. Please install one of them."
    exit 1
fi

if [ $RESULT -eq 0 ]; then
    echo ""
    echo "✓ VMA header file installed at: $VMA_FILE"
    echo ""
    echo "Next steps:"
    echo "1. cd build"
    echo "2. cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "3. cmake --build ."
    exit 0
else
    echo ""
    echo "✗ Failed to download VMA header"
    echo ""
    echo "Manual fallback:"
    echo "1. Download from: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases"
    echo "2. Extract vk_mem_alloc.h to: $VMA_DIR"
    echo ""
    exit 1
fi
