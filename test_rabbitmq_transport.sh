#!/bin/bash

echo "=== Testing RabbitMQ Transport for RTL_433 Client ==="
echo

# Configuration
RABBITMQ_HOST="localhost"
RABBITMQ_PORT="5672"
RABBITMQ_USER="guest"
RABBITMQ_PASS="guest"
RABBITMQ_EXCHANGE="rtl_433"
RABBITMQ_QUEUE="signals"

echo "RabbitMQ Configuration:"
echo "  Host: $RABBITMQ_HOST"
echo "  Port: $RABBITMQ_PORT"
echo "  User: $RABBITMQ_USER"
echo "  Password: $RABBITMQ_PASS"
echo "  Exchange: $RABBITMQ_EXCHANGE"
echo "  Queue: $RABBITMQ_QUEUE"
echo

# Check if RabbitMQ server is running
echo "1. Checking RabbitMQ server connectivity..."
if command -v curl &> /dev/null; then
    echo "   Testing connection to RabbitMQ management interface..."
    timeout 5 curl -s -u $RABBITMQ_USER:$RABBITMQ_PASS http://$RABBITMQ_HOST:15672/api/overview &> /dev/null
    if [ $? -eq 0 ]; then
        echo "   ✓ RabbitMQ server is accessible via management interface"
    else
        echo "   ⚠ Cannot connect to RabbitMQ management interface (may not be enabled)"
        echo "   Continuing with direct AMQP test..."
    fi
else
    echo "   curl not found, skipping management interface test"
fi

# Start RabbitMQ consumer to capture messages (if rabbitmqadmin is available)
CONSUMER_PID=""
if command -v python3 &> /dev/null; then
    echo "2. Starting simple message consumer..."
    
    # Create a simple Python consumer
    cat > rabbitmq_consumer.py << 'EOF'
#!/usr/bin/env python3
import pika
import sys
import signal
import json

def signal_handler(sig, frame):
    print('\nConsumer stopped')
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

try:
    # Connect to RabbitMQ
    credentials = pika.PlainCredentials('guest', 'guest')
    connection = pika.BlockingConnection(pika.ConnectionParameters('localhost', 5672, '/', credentials))
    channel = connection.channel()
    
    # Declare exchange and queue
    channel.exchange_declare(exchange='rtl_433', exchange_type='direct', durable=True)
    channel.queue_declare(queue='signals', durable=True)
    channel.queue_bind(exchange='rtl_433', queue='signals', routing_key='signals')
    
    print("Waiting for messages from rtl_433_client...")
    
    def callback(ch, method, properties, body):
        try:
            data = json.loads(body.decode('utf-8'))
            print(f"Received message: {len(body)} bytes, pulses: {data.get('count', 'unknown')}")
            with open('rabbitmq_messages.log', 'a') as f:
                f.write(body.decode('utf-8') + '\n')
            ch.basic_ack(delivery_tag=method.delivery_tag)
        except Exception as e:
            print(f"Error processing message: {e}")
    
    channel.basic_consume(queue='signals', on_message_callback=callback)
    channel.start_consuming()

except KeyboardInterrupt:
    print('\nConsumer stopped')
except Exception as e:
    print(f"Connection error: {e}")
    sys.exit(1)
EOF
    
    # Clear previous log
    rm -f rabbitmq_messages.log
    
    timeout 120 python3 rabbitmq_consumer.py &
    CONSUMER_PID=$!
    echo "   RabbitMQ consumer started with PID: $CONSUMER_PID"
    sleep 2
else
    echo "   Python3 not found, cannot start consumer"
    echo "   Messages will still be sent to RabbitMQ but not captured"
fi

# Check if client executable exists
echo "3. Checking rtl_433_client..."
if [ ! -f "./rtl_433_client" ]; then
    echo "   Error: rtl_433_client not found in current directory"
    if [ -n "$CONSUMER_PID" ]; then
        kill $CONSUMER_PID 2>/dev/null
    fi
    exit 1
fi

# Test with file input first (safer for testing)
echo "4. Testing with file input..."
if [ -f "../tests/signals/g001_433.92M_250k.cu8" ]; then
    echo "   Running client with file input and RabbitMQ transport..."
    AMQP_URL="amqp://$RABBITMQ_USER:$RABBITMQ_PASS@$RABBITMQ_HOST:$RABBITMQ_PORT/$RABBITMQ_EXCHANGE"
    echo "   Command: ./rtl_433_client -R 0 -r ../tests/signals/g001_433.92M_250k.cu8 -v -T \"$AMQP_URL\""
    
    timeout 30 ./rtl_433_client -R 0 -r ../tests/signals/g001_433.92M_250k.cu8 -v -T "$AMQP_URL"
    echo "   File input test completed"
else
    echo "   Test file not found, skipping file input test"
fi

# Test with SDR device (if available)
echo "5. Testing with SDR device (30 seconds)..."
echo "   Running client with SDR device and RabbitMQ transport..."
AMQP_URL="amqp://$RABBITMQ_USER:$RABBITMQ_PASS@$RABBITMQ_HOST:$RABBITMQ_PORT/$RABBITMQ_EXCHANGE"
echo "   Command: ./rtl_433_client -d 0 -f 433.92M -vv -T \"$AMQP_URL\""

timeout 30 ./rtl_433_client -d 0 -f 433.92M -vv -T "$AMQP_URL"

echo "6. Stopping RabbitMQ consumer..."
if [ -n "$CONSUMER_PID" ]; then
    kill $CONSUMER_PID 2>/dev/null
    wait $CONSUMER_PID 2>/dev/null
fi

echo "7. Checking received RabbitMQ messages..."
if [ -f "rabbitmq_messages.log" ]; then
    MSG_COUNT=$(wc -l < rabbitmq_messages.log 2>/dev/null || echo "0")
    echo "   Messages received: $MSG_COUNT"
    
    if [ "$MSG_COUNT" -gt 0 ]; then
        echo "   ✓ RabbitMQ transport is working!"
        echo "   First few messages:"
        head -3 rabbitmq_messages.log | sed 's/^/     /'
        echo "   Full log saved to: rabbitmq_messages.log"
    else
        echo "   ✗ No RabbitMQ messages received"
        echo "   Possible issues:"
        echo "     - Client not detecting signals"
        echo "     - RabbitMQ transport not working"
        echo "     - Authentication issues"
        echo "     - RabbitMQ server not running"
    fi
else
    echo "   No message log found"
    echo "   This may be normal if Python consumer wasn't started"
fi

# Cleanup
rm -f rabbitmq_consumer.py

echo
echo "=== RabbitMQ Transport Test Complete ==="
