#!/bin/bash

echo "=== Testing MQTT Transport for RTL_433 Client ==="
echo

# Configuration
MQTT_HOST="localhost"
MQTT_PORT="1883"
MQTT_USER="rtl433"
MQTT_PASS="rtl433"
MQTT_TOPIC="rtl_433/signals"

echo "MQTT Configuration:"
echo "  Host: $MQTT_HOST"
echo "  Port: $MQTT_PORT"
echo "  User: $MQTT_USER"
echo "  Password: $MQTT_PASS"
echo "  Topic: $MQTT_TOPIC"
echo

# Check if MQTT broker is running
echo "1. Checking MQTT broker connectivity..."
if command -v mosquitto_pub &> /dev/null; then
    echo "   Testing connection to MQTT broker..."
    timeout 5 mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS -t "test/connection" -m "test" &> /dev/null
    if [ $? -eq 0 ]; then
        echo "   ✓ MQTT broker is accessible"
    else
        echo "   ✗ Cannot connect to MQTT broker"
        echo "   Please check:"
        echo "     - MQTT broker is running on $MQTT_HOST:$MQTT_PORT"
        echo "     - Credentials are correct ($MQTT_USER/$MQTT_PASS)"
        exit 1
    fi
else
    echo "   mosquitto_pub not found, skipping connection test"
fi

# Start MQTT subscriber to capture messages
echo "2. Starting MQTT subscriber..."
if command -v mosquitto_sub &> /dev/null; then
    timeout 120 mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS -t "$MQTT_TOPIC" > mqtt_messages.log &
    SUB_PID=$!
    echo "   MQTT subscriber started with PID: $SUB_PID"
    sleep 2
else
    echo "   mosquitto_sub not found, cannot capture MQTT messages"
    echo "   You can manually subscribe to topic: $MQTT_TOPIC"
fi

# Check if client executable exists
echo "3. Checking rtl_433_client..."
if [ ! -f "./rtl_433_client" ]; then
    echo "   Error: rtl_433_client not found in current directory"
    if [ -n "$SUB_PID" ]; then
        kill $SUB_PID 2>/dev/null
    fi
    exit 1
fi

# Test with file input first (safer for testing)
echo "4. Testing with file input..."
if [ -f "../tests/signals/g001_433.92M_250k.cu8" ]; then
    echo "   Running client with file input and MQTT transport..."
    MQTT_URL="mqtt://$MQTT_USER:$MQTT_PASS@$MQTT_HOST:$MQTT_PORT/$MQTT_TOPIC"
    echo "   Command: ./rtl_433_client -R 0 -r ../tests/signals/g001_433.92M_250k.cu8 -v -T \"$MQTT_URL\""
    
    timeout 30 ./rtl_433_client -R 0 -r ../tests/signals/g001_433.92M_250k.cu8 -v -T "$MQTT_URL"
    echo "   File input test completed"
else
    echo "   Test file not found, skipping file input test"
fi

# Test with SDR device (if available)
echo "5. Testing with SDR device (30 seconds)..."
echo "   Running client with SDR device and MQTT transport..."
MQTT_URL="mqtt://$MQTT_USER:$MQTT_PASS@$MQTT_HOST:$MQTT_PORT/$MQTT_TOPIC"
echo "   Command: ./rtl_433_client -d 0 -f 433.92M -vv -T \"$MQTT_URL\""

timeout 30 ./rtl_433_client -d 0 -f 433.92M -vv -T "$MQTT_URL"

echo "6. Stopping MQTT subscriber..."
if [ -n "$SUB_PID" ]; then
    kill $SUB_PID 2>/dev/null
    wait $SUB_PID 2>/dev/null
fi

echo "7. Checking received MQTT messages..."
if [ -f "mqtt_messages.log" ]; then
    MSG_COUNT=$(wc -l < mqtt_messages.log 2>/dev/null || echo "0")
    echo "   Messages received: $MSG_COUNT"
    
    if [ "$MSG_COUNT" -gt 0 ]; then
        echo "   ✓ MQTT transport is working!"
        echo "   First few messages:"
        head -5 mqtt_messages.log | sed 's/^/     /'
        echo "   Full log saved to: mqtt_messages.log"
    else
        echo "   ✗ No MQTT messages received"
        echo "   Possible issues:"
        echo "     - Client not detecting signals"
        echo "     - MQTT transport not working"
        echo "     - Authentication issues"
    fi
else
    echo "   No message log found"
fi

echo
echo "=== MQTT Transport Test Complete ==="
