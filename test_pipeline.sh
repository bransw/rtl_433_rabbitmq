#!/bin/bash

echo "=== Testing RTL_433 Client-Server Pipeline ==="
echo

# Check if executables exist
if [ ! -f "./build/rtl_433_client" ]; then
    echo "Error: rtl_433_client not found"
    exit 1
fi

if [ ! -f "tests/signals/g001_433.92M_250k.cu8" ]; then
    echo "Error: Test signal file not found"
    exit 1
fi

echo "1. Starting netcat server on port 8080..."
nc -l 8080 > received_data.txt &
NC_PID=$!

echo "   Netcat server started with PID: $NC_PID"
sleep 2

echo "2. Running client with test signal file..."
echo "   Command: ./build/rtl_433_client -r tests/signals/g001_433.92M_250k.cu8 -T http://localhost:8080/api/signals -v"
echo

./build/rtl_433_client -r tests/signals/g001_433.92M_250k.cu8 -T http://localhost:8080/api/signals -v

echo
echo "3. Stopping netcat server..."
kill $NC_PID 2>/dev/null

echo "4. Checking received data..."
if [ -f "received_data.txt" ]; then
    echo "   Data received (first 10 lines):"
    head -10 received_data.txt
    echo "   Full data saved to: received_data.txt"
else
    echo "   No data received"
fi

echo
echo "=== Test Complete ==="
