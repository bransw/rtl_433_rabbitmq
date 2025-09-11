#!/bin/bash
# Build script for rtl_433 with split architecture
# This script builds original rtl_433, client and server

set -e

# Check if we're in WSL
if grep -q Microsoft /proc/version 2>/dev/null; then
    echo "WSL environment detected"
    WSL_DETECTED=true
else
    WSL_DETECTED=false
fi

# Build directory
BUILD_DIR="build"

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=ON \
    -DBUILD_SERVER=ON \
    -DENABLE_MQTT=ON \
    -DENABLE_RABBITMQ=OFF

# Check configuration
echo ""
echo "Configuration complete. Build targets:"
echo "  - rtl_433 (original)"
echo "  - rtl_433_client"
echo "  - rtl_433_server"
echo ""

# Build
echo "Building..."
make -j$(nproc)

# Show build results
echo ""
echo "Build complete!"
echo ""
echo "Built executables:"
ls -la src/rtl_433* 2>/dev/null || true

# WSL-specific instructions
if [ "$WSL_DETECTED" = true ]; then
    echo ""
    echo "WSL Setup Notes:"
    echo "  1. For RTL-SDR support, install usbipd on Windows:"
    echo "     winget install usbipd"
    echo "  2. Attach your RTL-SDR device:"
    echo "     usbipd wsl list"
    echo "     usbipd wsl attach --busid <your-device-busid>"
    echo "  3. Test RTL-SDR:"
    echo "     rtl_test"
fi

echo ""
echo "To test the split architecture:"
echo "  1. Start server: ./src/rtl_433_server"
echo "  2. Start client: ./src/rtl_433_client"
echo ""
echo "For detailed usage, see README_SPLIT.md and USAGE_EXAMPLES.md"
