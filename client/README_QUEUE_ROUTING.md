# RTL_433 Client - Queue/Topic Routing

## 🎯 Overview

RTL_433 Client now supports automatic message routing to different queues/topics based on data type.

## 📊 Data Type Routing

### **RabbitMQ Routing:**
- **OOK packets** → `ook_raw` queue (routing_key: "ook_raw")
- **FSK packets** → `fsk_raw` queue (routing_key: "fsk_raw")  
- **Decoded devices** → `detected` queue (routing_key: "detected")

### **MQTT Routing:**
- **OOK packets** → `ook_raw` topic
- **FSK packets** → `fsk_raw` topic
- **Decoded devices** → `detected` topic

## 🔧 Usage

### RabbitMQ Example:
```bash
./rtl_433_client -T amqp://guest:guest@localhost:5672/rtl_433/signals -r test.cu8
```

### MQTT Example:
```bash
./rtl_433_client -T mqtt://localhost:1883/rtl_433/signals -r test.cu8
```

## 📋 Message Flow

```
┌─────────────┐
│ rtl_433_    │
│ client      │
└──────┬──────┘
       │
       ▼
┌─────────────┐    ┌──────────────┐
│ Signal      │    │ Raw Pulse    │ 
│ Detection   │───▶│ Data         │ ────┐
└─────────────┘    └──────────────┘     │
       │                               │
       ▼                               │
┌─────────────┐    ┌──────────────┐     │    ┌─────────────┐
│ Device      │    │ Decoded      │     │    │             │
│ Decoding    │───▶│ Device Data  │─────┼───▶│ Transport   │
└─────────────┘    └──────────────┘     │    │ Layer       │
                                        │    │             │
                                        │    └─────┬───────┘
                                        │          │
                                        │          ▼
                   ┌─────────────────────┼────────────────────┐
                   │                     │                    │
                   ▼                     ▼                    ▼
            ┌─────────────┐      ┌─────────────┐      ┌─────────────┐
            │ ook_raw     │      │ fsk_raw     │      │ detected    │
            │ queue/topic │      │ queue/topic │      │ queue/topic │
            └─────────────┘      └─────────────┘      └─────────────┘
```

## 🔍 Message Types

### 1. Raw Pulse Data (ook_raw/fsk_raw)

Sent for every detected signal before device decoding attempts:

```json
{
  "type": "pulse_data",
  "mod": "OOK",
  "count": 3,
  "pulses": [104, 248, 100, 136, 112],
  "freq_Hz": 433920000,
  "rate_Hz": 250000,
  "rssi_dB": 1.2,
  "snr_dB": 15.3,
  "noise_dB": -15.3
}
```

### 2. Decoded Device Data (detected)

Sent when a signal is successfully decoded by a device protocol:

```json
{
  "type": "device_data", 
  "device": "Toyota TPMS",
  "time": "2025-09-12 14:30:15",
  "model": "Toyota",
  "type": "TPMS",
  "id": "d4c2b1", 
  "pressure_kPa": 241.3,
  "temperature_C": 21,
  "mic": "CRC"
}
```

## ⚡ Benefits

### **Separation of Concerns**
- **Raw data processing** - Workers subscribe to `ook_raw`/`fsk_raw` for signal analysis
- **Device data processing** - Workers subscribe to `detected` for device-specific logic
- **Signal analysis** - Pulse data can be sent to triq.org for visualization

### **Scalability**
- Different worker types can scale independently
- Raw signal analysis doesn't affect device processing performance
- Easy to add specialized workers for specific device types

### **Monitoring and Analytics**
- Track signal detection rates by modulation type
- Monitor device decoding success rates  
- Separate statistics for OOK vs FSK signals
- Easy debugging with isolated message streams

### **Flexible Processing Pipeline**
- Process raw signals for unknown device analysis
- Re-decode signals with updated protocols
- Send raw data to external analysis tools
- Build custom device decoders from pulse patterns

## 🛠️ Implementation Details

### Transport Layer Integration

The routing is implemented at the transport layer level:

```c
// Raw pulse data routing
client_pulse_handler_with_type(&pulse_data, PULSE_DATA_OOK);
// -> Sends to 'ook_raw' queue

client_pulse_handler_with_type(&pulse_data, PULSE_DATA_FSK);  
// -> Sends to 'fsk_raw' queue

// Decoded device data routing
client_data_acquired_handler(r_dev, data);
// -> Sends to 'detected' queue
```

### Queue/Topic Naming

- **RabbitMQ**: Uses routing_key for message routing
- **MQTT**: Uses topic hierarchy
- **HTTP**: Uses URL path parameters
- **TCP/UDP**: Uses message headers

## 🔧 Configuration

### RabbitMQ Setup

```bash
# Create exchange
rabbitmqctl eval 'rabbit_exchange:declare({resource, <<"/">>, exchange, <<"rtl_433">>}, topic, true, false, false, []).'

# Create queues  
rabbitmqctl eval 'rabbit_amqqueue:declare({resource, <<"/">>, queue, <<"ook_raw">>}, true, false, [], none).'
rabbitmqctl eval 'rabbit_amqqueue:declare({resource, <<"/">>, queue, <<"fsk_raw">>}, true, false, [], none).'
rabbitmqctl eval 'rabbit_amqqueue:declare({resource, <<"/">>, queue, <<"detected">>}, true, false, [], none).'

# Bind queues to exchange
rabbitmqctl eval 'rabbit_binding:add({binding, {resource, <<"/">>, exchange, <<"rtl_433">>}, <<"ook_raw">>, {resource, <<"/">>, queue, <<"ook_raw">>}, []}).'
rabbitmqctl eval 'rabbit_binding:add({binding, {resource, <<"/">>, exchange, <<"rtl_433">>}, <<"fsk_raw">>, {resource, <<"/">>, queue, <<"fsk_raw">>}, []}).'
rabbitmqctl eval 'rabbit_binding:add({binding, {resource, <<"/">>, exchange, <<"rtl_433">>}, <<"detected">>, {resource, <<"/">>, queue, <<"detected">>}, []}).'
```

### Consumer Examples

#### Python RabbitMQ Consumer
```python
import pika
import json

def process_ook_signals(ch, method, properties, body):
    data = json.loads(body)
    print(f"OOK Signal: {data['pulses']}")
    # Send to triq.org for analysis
    
def process_device_data(ch, method, properties, body):
    data = json.loads(body)
    print(f"Device: {data['device']} - {data}")
    # Store in database

connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
channel = connection.channel()

channel.basic_consume(queue='ook_raw', on_message_callback=process_ook_signals)
channel.basic_consume(queue='detected', on_message_callback=process_device_data)

channel.start_consuming()
```

#### MQTT Consumer  
```python
import paho.mqtt.client as mqtt
import json

def on_message(client, userdata, msg):
    topic = msg.topic
    data = json.loads(msg.payload.decode())
    
    if 'ook_raw' in topic:
        print(f"OOK: {data['pulses']}")
    elif 'detected' in topic:
        print(f"Device: {data['device']}")

client = mqtt.Client()
client.on_message = on_message
client.connect("localhost", 1883, 60)

client.subscribe("rtl_433/ook_raw")
client.subscribe("rtl_433/fsk_raw") 
client.subscribe("rtl_433/detected")

client.loop_forever()
```

## 🎯 Use Cases

### 1. Signal Analysis Laboratory
- Subscribe to `ook_raw`/`fsk_raw` for unknown signal analysis
- Send pulse data to triq.org for visualization
- Develop new device protocols from raw patterns

### 2. IoT Device Monitoring
- Subscribe to `detected` for device status updates
- Store device data in time-series database
- Set up alerts for specific device conditions

### 3. RF Security Monitoring  
- Monitor `ook_raw`/`fsk_raw` for unauthorized transmissions
- Detect new/unknown device signatures
- Analyze signal patterns for security threats

### 4. Research and Development
- Collect raw signal samples for machine learning
- Test new demodulation algorithms
- Build device signature databases

This routing system provides a powerful foundation for building scalable RF signal processing pipelines! 🚀