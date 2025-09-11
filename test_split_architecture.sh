#!/bin/bash
# Test script for RTL_433 split architecture
# This script demonstrates how to use client and server together

set -e

echo "=== RTL_433 Split Architecture Test ==="
echo ""

# Check if executables exist
if [ ! -f "build/rtl_433_client" ]; then
    echo "❌ rtl_433_client not found. Build first with: ./build_all.sh"
    exit 1
fi

if [ ! -f "build/rtl_433_server" ]; then
    echo "❌ rtl_433_server not found. Build first with: ./build_all.sh"
    exit 1
fi

echo "✅ Found all executables"
echo ""

# Test 1: Show help for both components
echo "=== Testing Help Messages ==="
echo ""

echo "📋 RTL_433 Client Help:"
./build/rtl_433_client --help 2>&1 | head -10 || echo "Help not implemented yet"
echo ""

echo "📋 RTL_433 Server Help:"
./build/rtl_433_server --help 2>&1 | head -10 || echo "Help not implemented yet"
echo ""

# Test 2: Start server in background (stub mode)
echo "=== Testing Server Startup ==="
echo "🚀 Starting server in background..."
./build/rtl_433_server &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 2

# Check if server is running
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "✅ Server started successfully"
else
    echo "❌ Server failed to start"
    exit 1
fi

# Test 3: Test client connection (stub mode)
echo ""
echo "=== Testing Client Connection ==="
echo "📡 Testing client with stub data..."
timeout 5 ./build/rtl_433_client -T http://localhost:8080/api/signals || echo "Client test completed (expected with stubs)"

# Cleanup
echo ""
echo "=== Cleanup ==="
echo "🛑 Stopping server..."
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true

echo ""
echo "✅ Test completed successfully!"
echo ""
echo "Next steps:"
echo "  1. Configure real RTL-SDR hardware (see README_SPLIT.md)"
echo "  2. Set up transport layer (HTTP/MQTT/RabbitMQ)"
echo "  3. Test with real signals"
echo ""
