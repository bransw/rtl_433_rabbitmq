#!/usr/bin/env python3
"""
Script to send realistic pulse data to RabbitMQ for testing rtl_433_server
Based on real device signal patterns
"""

import pika
import json
import sys
import time

def create_realistic_pulse_data():
    """Create realistic pulse data patterns for different device types"""
    
    # Pattern 1: Temperature sensor (Oregon Scientific style)
    temp_sensor = {
        "package_id": 987654,
        "mod": "OOK",
        "count": 64,
        "pulses": [
            # Preamble
            500, 1000, 500, 1000, 500, 1000, 500, 1000,
            # Sync
            500, 4000,
            # Data bits (Manchester encoded temperature data)
            500, 1000, 1000, 500, 500, 1000, 1000, 500,  # Device ID
            500, 1000, 500, 1000, 1000, 500, 500, 1000,  # Channel
            1000, 500, 500, 1000, 500, 1000, 1000, 500,  # Temperature high
            500, 1000, 1000, 500, 1000, 500, 500, 1000,  # Temperature low
            1000, 500, 1000, 500, 500, 1000, 500, 1000,  # Humidity
            500, 1000, 1000, 500, 500, 1000, 1000, 500,  # Checksum
            # End
            500, 10000
        ],
        "freq_Hz": 433920000,
        "rate_Hz": 250000,
        "rssi_dB": -15.2,
        "snr_dB": 12.8,
        "noise_dB": -28.0,
        "timestamp": "2025-09-13T15:30:00"
    }
    
    # Pattern 2: TPMS sensor (Toyota style)
    tpms_sensor = {
        "package_id": 456789,
        "mod": "FSK",
        "count": 48,
        "pulses": [
            # FSK preamble
            52, 52, 52, 52, 52, 52, 52, 52,
            # Sync word
            104, 52, 52, 104, 104, 52,
            # Sensor ID
            52, 104, 52, 52, 104, 104, 52, 52,
            52, 52, 104, 52, 104, 52, 52, 104,
            # Pressure data
            104, 52, 52, 104, 52, 104, 104, 52,
            # Temperature data  
            52, 52, 104, 104, 52, 52, 104, 52,
            # Status flags
            52, 104, 52, 52,
            # Checksum
            104, 52, 104, 104
        ],
        "freq_Hz": 433920000,
        "rate_Hz": 250000,
        "rssi_dB": -18.5,
        "snr_dB": 10.2,
        "noise_dB": -28.7,
        "timestamp": "2025-09-13T15:30:05"
    }
    
    # Pattern 3: Remote control (simple OOK)
    remote_control = {
        "package_id": 123789,
        "mod": "OOK", 
        "count": 25,
        "pulses": [
            # Short preamble
            300, 300, 300, 300,
            # Sync
            300, 9000,
            # Command data (PWM encoded)
            300, 600, 300, 1200, 600, 300, 300, 600,
            1200, 300, 300, 600, 600, 300, 300, 1200,
            300, 600, 600, 300, 300, 600, 300, 1200,
            # End gap
            300
        ],
        "freq_Hz": 433920000,
        "rate_Hz": 250000,
        "rssi_dB": -10.1,
        "snr_dB": 15.5,
        "noise_dB": -25.6,
        "timestamp": "2025-09-13T15:30:10"
    }
    
    return [temp_sensor, tpms_sensor, remote_control]

def send_pulse_data_to_rabbitmq(pulse_data_list):
    """Send pulse data to RabbitMQ"""
    
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
        
        print("🚀 Sending realistic pulse data to RabbitMQ...")
        print("=" * 60)
        
        for i, pulse_data in enumerate(pulse_data_list, 1):
            message = json.dumps(pulse_data)
            
            channel.basic_publish(
                exchange='rtl_433',
                routing_key='signals',
                body=message,
                properties=pika.BasicProperties(
                    delivery_mode=2,  # Make message persistent
                    content_type='application/json'
                )
            )
            
            print(f"📤 Sent signal {i}/3:")
            print(f"   Type: {pulse_data['mod']}")
            print(f"   Pulses: {pulse_data['count']}")
            print(f"   Frequency: {pulse_data['freq_Hz']} Hz")
            print(f"   Sample Rate: {pulse_data['rate_Hz']} Hz")
            print(f"   RSSI: {pulse_data['rssi_dB']} dB")
            print(f"   Package ID: {pulse_data['package_id']}")
            print(f"   Message size: {len(message)} bytes")
            print()
            
            # Small delay between messages
            time.sleep(0.5)
        
        connection.close()
        
        print("✅ All realistic pulse data sent successfully!")
        print("💡 Now run the server to see device decoding results:")
        print("   ./build/rtl_433_server -v")
        
        return True
        
    except Exception as e:
        print(f"❌ Error sending pulse data: {e}")
        return False

def main():
    """Main function"""
    
    print("🧪 RTL_433 REALISTIC PULSE DATA SENDER")
    print("=" * 60)
    print("📡 Creating realistic device signal patterns...")
    
    # Create realistic pulse data
    pulse_data_list = create_realistic_pulse_data()
    
    print(f"✅ Created {len(pulse_data_list)} realistic signal patterns:")
    print("   1. Temperature/Humidity Sensor (OOK)")
    print("   2. TPMS Tire Pressure Sensor (FSK)")  
    print("   3. Remote Control (OOK)")
    print()
    
    # Send to RabbitMQ
    success = send_pulse_data_to_rabbitmq(pulse_data_list)
    
    if success:
        print("\n🎯 READY FOR SERVER TEST!")
        print("Run: timeout 10s ./build/rtl_433_server -v")
    
    return success

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
