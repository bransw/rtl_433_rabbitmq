# triq.org Integration

## Overview

Data sent by `rtl_433_client` to RabbitMQ queues is **fully sufficient** for creating requests to [triq.org/pdv](https://triq.org/pdv) for visual signal analysis.

## RabbitMQ Data Structure

### Queues and Data

- **`ook_raw`** - raw OOK signals
- **`fsk_raw`** - raw FSK signals  
- **`unknown_raw`** - signals of unknown type
- **`detected`** - decoded devices

### JSON Format for pulse_data

```json
{
  "mod": "OOK",                    // Modulation type: OOK or FSK
  "count": 1,                      // Number of pulses
  "pulses": [2396, 23964],         // Array [pulse1, gap1, pulse2, gap2, ...]
  "freq1_Hz": 433919776,           // Frequency F1 (FSK)
  "freq2_Hz": 433921000,           // Frequency F2 (FSK only)
  "freq_Hz": 433920000,            // Center frequency
  "rate_Hz": 250000,               // Sample rate
  "depth_bits": 0,                 // Sample bit depth
  "range_dB": 84.2884,             // Dynamic range
  "rssi_dB": 5.10922,              // Signal strength
  "snr_dB": 1.23904,               // Signal-to-noise ratio
  "noise_dB": -1.23958             // Noise level
}
```

## Creating RfRaw Strings for triq.org

### Conversion Utility

To convert RabbitMQ messages to triq.org format, use the `rabbitmq_to_triq.py` utility:

```bash
python3 rabbitmq_to_triq.py '<json_message>'
```

### Usage Example

```bash
python3 rabbitmq_to_triq.py '{"mod":"OOK","count":1,"pulses":[2396,23964],"rate_Hz":250000}'
```

**Result:**
```
📡 Modulation: OOK
📊 Pulses: 1
🔄 Rate: 250000 Hz
📈 Timings: 2
✅ RfRaw: AAB1022570FFFF8155
🌐 Link: https://triq.org/pdv/#AAB1022570FFFF8155
```

## RfRaw B1 Format

### Structure

```
AAB1 060228FFFF007000000044001C8494A2B555
│││└─ Number of timings (6)
││└── B1 format (single packet)
│└─── Header AA
└──── Type B1

060228FFFF007000000044001C = Timing table (2 bytes per timing in μs)
8494A2B5 = Pulse data (encoded)
55 = End byte
```

### Pulse Encoding

Each pulse byte has format `0x8X`, where:
- `X` upper 4 bits = pulse index in timing table
- `X` lower 4 bits = gap index in timing table

Example: `0x84 = pulse[0], gap[4]`

## Analysis Process

### 1. Getting Data from RabbitMQ

```python
import json
import pika

# Connect to RabbitMQ
connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
channel = connection.channel()

def process_message(ch, method, properties, body):
    message = json.loads(body)
    rfraw = create_rfraw_from_message(message)
    print(f"Analysis: https://triq.org/pdv/#{rfraw}")

# Listen to queue
channel.basic_consume(queue='ook_raw', on_message_callback=process_message)
channel.start_consuming()
```

### 2. Automatic Analysis

Create a script for automatic monitoring:

```python
# monitor_signals.py
import json
import pika
import subprocess
import sys

def analyze_signal(message):
    """Automatically creates triq.org link from RabbitMQ message"""
    json_str = json.dumps(message)
    result = subprocess.run(['python3', 'rabbitmq_to_triq.py', json_str], 
                          capture_output=True, text=True)
    return result.stdout

# Real-time monitoring
```

## Limitations

- **Maximum 8 timings**: RfRaw B1 supports up to 8 different timing values
- **16-bit timings**: Maximum timing value 65535 μs
- **Simple signals**: Complex protocols may require RfRaw B0 format

## Useful Links

- [triq.org Pulse Data Viewer](https://triq.org/pdv)
- [RfRaw format](https://github.com/merbanan/rtl_433/blob/master/docs/ANALYZE.md)
- [rtl_433 documentation](https://github.com/merbanan/rtl_433)

## Conclusion

✅ **Data from RabbitMQ queues is fully sufficient for:**
- Creating RfRaw strings
- Visual analysis on triq.org
- Determining modulation parameters
- Creating flex decoders

Now you can easily analyze any signals captured by `rtl_433_client` using powerful triq.org visualization tools!