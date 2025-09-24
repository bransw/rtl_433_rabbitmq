#!/usr/bin/env python3
"""
Test script to send pulse data directly to RabbitMQ for server testing
"""

import pika
import json
import sys

def send_test_message():
    # Test pulse data with correct sample_rate
    test_pulse_data = {
        "package_id": 123456,
        "mod": "FSK", 
        "count": 10,
        "pulses": [100, 200, 100, 200, 100, 200, 100, 200, 100, 200, 100, 200, 100, 200, 100, 200, 100, 200, 100, 200],
        "freq_Hz": 433920000,
        "rate_Hz": 250000,  # CRITICAL: Set sample rate correctly
        "rssi_dB": -12.1,
        "snr_dB": 13.9,
        "noise_dB": -26.0,
        "timestamp": "2025-09-13T12:00:00"
    }
    
    try:
        # Connect to RabbitMQ
        connection = pika.BlockingConnection(
            pika.ConnectionParameters(host='localhost', 
                                    credentials=pika.PlainCredentials('guest', 'guest'))
        )
        channel = connection.channel()
        
        # Declare exchange and queue
        channel.exchange_declare(exchange='rtl_433', exchange_type='direct', durable=True)
        channel.queue_declare(queue='signals', durable=True)
        channel.queue_bind(exchange='rtl_433', queue='signals', routing_key='signals')
        
        # Send message
        message = json.dumps(test_pulse_data)
        channel.basic_publish(
            exchange='rtl_433',
            routing_key='signals', 
            body=message,
            properties=pika.BasicProperties(
                delivery_mode=2,  # Make message persistent
                content_type='application/json'
            )
        )
        
        print(f"✅ Sent test message to RabbitMQ:")
        print(f"   Queue: signals")
        print(f"   Pulses: {test_pulse_data['count']}")
        print(f"   Sample Rate: {test_pulse_data['rate_Hz']} Hz")
        print(f"   Frequency: {test_pulse_data['freq_Hz']} Hz")
        print(f"   Message size: {len(message)} bytes")
        
        connection.close()
        return True
        
    except Exception as e:
        print(f"❌ Error sending message: {e}")
        return False

if __name__ == "__main__":
    print("🚀 Testing RabbitMQ message sending...")
    success = send_test_message()
    sys.exit(0 if success else 1)
