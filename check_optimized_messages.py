#!/usr/bin/env python3
"""
Check optimized message structure in RabbitMQ
"""
import pika
import json

def check_messages():
    try:
        connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
        channel = connection.channel()
        
        # Get one message from signals queue
        method_frame, header_frame, body = channel.basic_get('signals')
        if method_frame:
            message = json.loads(body)
            print('📦 OPTIMIZED MESSAGE STRUCTURE:')
            print('================================')
            for key, value in message.items():
                if key == 'hex_string':
                    print(f'{key}: {value[:50]}...' if len(str(value)) > 50 else f'{key}: {value}')
                else:
                    print(f'{key}: {value}')
            
            print(f'\n📏 MESSAGE SIZE: {len(body)} bytes')
            print(f'🔥 FIELDS COUNT: {len(message)} (vs ~15 in old format)')
            
            # Check if pulse data is missing (should be!)
            if 'pulses' not in message:
                print('✅ SUCCESS: pulse_data array REMOVED (redundant)')
            if 'count' not in message:
                print('✅ SUCCESS: count field REMOVED (redundant)')
            if 'rate_Hz' not in message:
                print('✅ SUCCESS: rate_Hz field REMOVED (redundant)')
                
            channel.basic_ack(method_frame.delivery_tag)
        else:
            print('❌ No messages in queue')
        
        connection.close()
        
    except Exception as e:
        print(f'❌ Error: {e}')

if __name__ == '__main__':
    check_messages()
