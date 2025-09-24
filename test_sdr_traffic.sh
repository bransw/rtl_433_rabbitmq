#!/bin/bash

echo "=== Testing SDR Client Traffic ==="
echo

# Start netcat server in background
echo "1. Starting netcat server on port 8080..."
nc -l 8080 > sdr_traffic.log &
NC_PID=$!
echo "   Netcat server started with PID: $NC_PID"

# Wait for server to start
sleep 2

# Check if executable exists
if [ ! -f "./rtl_433_client" ]; then
    echo "Error: rtl_433_client not found in current directory"
    kill $NC_PID 2>/dev/null
    exit 1
fi

echo "2. Starting rtl_433_client with SDR device..."
echo "   Command: ./rtl_433_client -d 0 -f 433.92M -vvv -T http://localhost:8080/api/signals"
echo "   (Will run for 30 seconds to capture any signals)"
echo

# Run client for 30 seconds
timeout 30 ./rtl_433_client -d 0 -f 433.92M -vvv -T http://localhost:8080/api/signals

echo
echo "3. Stopping netcat server..."
kill $NC_PID 2>/dev/null

echo "4. Checking received data..."
if [ -f "sdr_traffic.log" ]; then
    echo "   Traffic log size: $(wc -c < sdr_traffic.log) bytes"
    if [ -s "sdr_traffic.log" ]; then
        echo "   Data received! First 500 characters:"
        head -c 500 sdr_traffic.log
        echo ""
        echo "   Full log saved to: sdr_traffic.log"
    else
        echo "   No data received from client"
    fi
else
    echo "   No traffic log created"
fi

echo
echo "=== Test Complete ==="
