# Package ID Linking in rtl_433_client

## 🔗 **ANSWER TO THE QUESTION:**

Now you can link demodulated packages (FSK/OOK) with decoded information through **`package_id`**!

## 📦 **HOW IT WORKS:**

### 1. **Each package gets a unique ID:**
```c
// Unique ID generation across restarts
static uint64_t g_start_time_us = 0;
static uint32_t g_sequence_in_second = 0;
static uint32_t g_last_second = 0;

static unsigned long generate_unique_package_id(void) {
    // Create ID: timestamp(32-bit) + sequence(16-bit) + startup_random(16-bit)
    uint64_t unique_id = ((uint64_t)current_second << 32) | 
                        ((g_sequence_in_second & 0xFFFF) << 16) | 
                        ((g_start_time_us & 0xFFFF));
    return (unsigned long)unique_id;
}

// For each detected package:
g_current_package_id = generate_unique_package_id();
```

**🔄 Uniqueness Algorithm:**
- **32 bits**: Unix timestamp (seconds) 
- **16 bits**: Sequence within second (up to 65535 packages/sec)
- **16 bits**: Random startup time part (uniqueness between restarts)

**✅ Guarantees:**
- Uniqueness between application restarts
- Support up to 65535 packages per second  
- Same data size (64-bit unsigned long)
- Chronological order (timestamp in higher bits)

### 2. **Raw data includes package_id:**
**Queues: `ook_raw`, `fsk_raw`**
```json
{
  "package_id": 1726141486000001,
  "time": "2025-09-12 12:34:56",
  "mod": "FSK", 
  "freq1": 433920000,
  "freq2": 433920000,
  "rssi": -12.1,
  "snr": 15.2,
  "noise": -27.3,
  "pulses": [0, 48, 48, 96, 96, 48, ...],
  "gap_limit": 1000,
  "short_width": 48,
  "long_width": 96,
  "reset_limit": 200
}
```

### 3. **Decoded data includes the same package_id:**
**Queue: `detected`**
```json
{
  "package_id": 1726141486000001,
  "device_id": "Toyota TPMS",
  "timestamp_us": 1726141486000000,
  "frequency": 433920000,
  "sample_rate": 250000,
  "rssi_db": -12.1,
  "snr_db": 15.2,
  "noise_db": -27.3,
  "ook_detected": 0,
  "fsk_detected": 1,
  "pulse_count": 59,
  "raw_data_hex": "{\"model\":\"Toyota\",\"type\":\"TPMS\",\"id\":\"12ab34cd\",\"pressure_kPa\":220,\"temperature_C\":23,\"mic\":\"CRC\"}"
}
```

## 🔄 **LINKING ALGORITHM:**

### Server-side:

```python
def link_packages(raw_messages, decoded_messages):
    """Link raw and decoded packages by package_id"""
    linked_data = {}
    
    # Index raw data by package_id
    for raw_msg in raw_messages:
        package_id = raw_msg['package_id']
        linked_data[package_id] = {
            'raw': raw_msg,
            'decoded': None
        }
    
    # Add decoded data
    for decoded_msg in decoded_messages:
        package_id = decoded_msg['package_id']
        if package_id in linked_data:
            linked_data[package_id]['decoded'] = decoded_msg
    
    return linked_data

# Usage:
linked = link_packages(ook_raw + fsk_raw, detected)
for package_id, data in linked.items():
    print(f"Package {package_id}:")
    print(f"  Raw: {data['raw']['mod']} with {len(data['raw']['pulses'])} pulses")
    if data['decoded']:
        print(f"  Decoded: {data['decoded']['device_id']}")
    else:
        print(f"  Decoded: No device matched")
```

## 📊 **PRACTICAL EXAMPLE:**

From your test:
```
Client: Signals sent: 10
Found 8 packages
FSK package #2 decoded 1 device events  
FSK package #3 decoded 1 device events
```

**RabbitMQ contains:**
- **8 raw messages** (ook_raw + fsk_raw) with package_id: 1, 2, 3, 4, 5, 6, 7, 8
- **2 decoded messages** (detected) with package_id: 2, 3

**Linked data:**
- **Package 1**: OOK raw → not decoded
- **Package 2**: FSK raw → Toyota TPMS ✅ 
- **Package 3**: FSK raw → Toyota TPMS ✅
- **Package 4-8**: FSK raw → not decoded

## 🗄️ **DATA STRUCTURES:**

### **In code:**
```c
// Raw pulse data gets package_id
transport_send_pulse_data_with_id(&g_transport, pulse_data, queue_name, package_id);

// Decoded data gets same package_id  
demod_data.package_id = g_current_package_id;
transport_send_demod_data_to_queue(&g_transport, &demod_data, "detected");
```

### **In JSON:**
- **Raw data**: Includes `package_id` as first field
- **Decoded data**: Includes `package_id` in demod_data_t structure

## 🎯 **APPLICATIONS:**

1. **Unknown signal analysis**: If raw data exists without decoded — signal not recognized
2. **Decoder debugging**: Compare raw pulse characteristics with decoding results  
3. **Statistics**: Calculate success rate of decoding by device types
4. **Replay analysis**: Re-decode raw data with new parameters

**Now you have complete traceability from raw pulses to decoded devices!** 🎉
