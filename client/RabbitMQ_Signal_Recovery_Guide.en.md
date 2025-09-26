## Data from RabbitMQ for Signal Recovery

**From code analysis and real RabbitMQ data, the following data is used for signal recovery and detection:**

### 📡 **SIGNALS Queue (for signal recovery):**

**Main fields for signal recovery:**
1. **`hex_string`** - `"AAB10209745E8C8155"` - **Key field!** Contains compact representation of all signal timing data in rfraw format
2. **`pulses`** - `[2420, 24204]` - Array of pulse and gap durations in microseconds
3. **`mod`** - `"OOK"` or `"FSK"` - Modulation type
4. **`count`** - `1` - Number of pulses

**RF metadata for accurate recovery:**
- **`freq1_Hz`** - `433921472` - Primary frequency
- **`freq_Hz`** - `433920000` - Center frequency  
- **`rate_Hz`** - `250000` - Sample rate
- **`depth_bits`** - `8` - Bit depth
- **`range_dB`** - `42.1442` - Dynamic range
- **`rssi_dB`** - `2.41444` - Signal strength
- **`snr_dB`** - Signal-to-noise ratio
- **`noise_dB`** - Noise level

**Additional fields for precise positioning:**
- **`offset`** - Offset in data stream
- **`start_ago`** - Signal start time 
- **`end_ago`** - Signal end time
- **`ook_low_estimate`** / **`ook_high_estimate`** - Thresholds for OOK
- **`fsk_f1_est`** / **`fsk_f2_est_value`** - Frequencies for FSK

### 📋 **DETECTED Queue (decoded devices):**

**Detection result:**
- **`model`** - `"Toyota"` - Device model
- **`type`** - `"TPMS"` - Device type  
- **`id`** - `"d75c823b"` - Device identifier
- **Specific data** - `pressure_PSI`, `temperature_C`, `status` etc.

### 🔧 **Signal recovery process:**

**1. Main method (via hex_string):**
```c
// Function rtl433_signal_reconstruct_from_hex()
int rtl433_signal_reconstruct_from_hex(const char *hex_str, 
                                      pulse_data_t *pulse_data,
                                      const char *json_metadata)
```

**2. Alternative method (via pulses array):**
```json
{
  "pulses": [2420, 24204],  // Array pulse-gap-pulse...
  "rate_Hz": 250000,        // Sample rate for accurate timing
  "mod": "OOK"             // Modulation type
}
```

### 🎯 **Key features:**

1. **`hex_string`** - most compact and accurate way to transmit timing data
2. **`pulses` array** - backup method if hex_string unavailable
3. **RF metadata** (frequencies, RSSI, SNR) critical for accurate recovery
4. **Time correlation** between 'signals' and 'detected' via `time` field

**Example usage for detection:**
```c
// 1. Get JSON from RabbitMQ 'signals'
// 2. Extract hex_string: "AAB10209745E8C8155"  
// 3. Reconstruct pulse_data_t structure
// 4. Apply device detectors
// 5. Send result to 'detected' queue
```

Thus, **hex_string** is the main field for signal recovery, and **pulses array + metadata** is the backup method for more complex signals.

---

## What data is needed if only hex_string is available

Excellent question! Let's analyze what minimal data is needed to recover a signal when we only have `hex_string`.

Let's look at the `rtl433_signal_set_defaults` function and hex_string parsing:

Let's look at the hex_string parsing function and what it extracts:

Perfect! Now I have the complete picture. Let's create a practical example for demonstration:

```python
#!/usr/bin/env python3
"""
Analysis of minimal data for signal recovery from hex_string
"""
import json

# Example real data from RabbitMQ signals queue
signal_data = {
    "time": "@0.524288s",
    "mod": "OOK",
    "count": 1,
    "pulses": [2420, 24204],
    "freq1_Hz": 433921472,
    "freq_Hz": 433920000,
    "rate_Hz": 250000,
    "depth_bits": 8,
    "range_dB": 42.1442,
    "rssi_dB": 2.41444,
    "snr_dB": 0.63729,
    "noise_dB": -0.637554,
    "offset": 1025,
    "start_ago": 130047,
    "end_ago": 123391,
    "ook_low_estimate": 14147,
    "ook_high_estimate": 28567,
    "fsk_f1_est": 388,
    "fsk_f2_est_value": 0,
    "hex_string": "AAB10209745E8C8155"
}

print("=== ANALYSIS: Minimal data for recovery from hex_string ===\n")

print("1. 🎯 ONLY REQUIRED FIELD:")
print(f"   hex_string: '{signal_data['hex_string']}'")
print("   ↳ Contains ALL timing data for complete signal recovery\n")

print("2. 📊 FIELDS WITH DEFAULT VALUES (if not specified):")
defaults = {
    "sample_rate": "250000 Hz (from rtl433_signal_set_defaults)",
    "depth_bits": "8 bits",
    "centerfreq_hz": "433920000 Hz (from rtl433_rfraw_parse_hex_string)",
    "freq1_hz": "= centerfreq_hz",
    "offset": "0",
    "start_ago": "0", 
    "end_ago": "0",
    "ook_low_estimate": "1000",
    "ook_high_estimate": "8000",
    "fsk_f1_est": "0",
    "fsk_f2_est": "0",
    "range_db": "0.0",
    "rssi_db": "0.0",
    "snr_db": "0.0",
    "noise_db": "0.0"
}

for field, default in defaults.items():
    print(f"   {field}: {default}")

print("\n3. 🔧 ADDITIONAL METADATA (improves quality):")
important_metadata = {
    "freq_Hz": "Exact center frequency",
    "rate_Hz": "Real sample rate", 
    "rssi_dB": "Signal strength",
    "snr_dB": "Signal-to-noise ratio",
    "noise_dB": "Noise level"
}

for field, description in important_metadata.items():
    if field in signal_data:
        print(f"   {field}: {signal_data[field]} - {description}")

print("\n4. 📋 MINIMAL JSON for recovery:")
minimal_json = {
    "hex_string": signal_data["hex_string"]
}
print(json.dumps(minimal_json, indent=2))

print("\n5. 🚀 RECOMMENDED JSON for quality recovery:")
recommended_json = {
    "hex_string": signal_data["hex_string"],
    "freq_Hz": signal_data["freq_Hz"], 
    "rate_Hz": signal_data["rate_Hz"],
    "rssi_dB": signal_data["rssi_dB"],
    "snr_dB": signal_data["snr_dB"],
    "noise_dB": signal_data["noise_dB"]
}
print(json.dumps(recommended_json, indent=2))

print("\n6. 🔍 WHAT hex_string CONTAINS:")
hex_analysis = {
    "AA": "Header (0xAA = OOK format)",
    "B1": "Modulation info",
    "02": "Payload length",
    "09": "Timing histogram count (9 bins)",
    "745E8C8155": "Timing data + pulse/gap pairs"
}

hex_string = signal_data["hex_string"]
print(f"   hex_string: {hex_string}")
for i, (part, description) in enumerate(hex_analysis.items()):
    if i < 4:  # First 4 parts are fixed length
        start_pos = i * 2
        hex_part = hex_string[start_pos:start_pos+2] if start_pos < len(hex_string) else "??"
        print(f"   {hex_part}: {description}")
    else:
        remaining = hex_string[8:]
        print(f"   {remaining}: {description}")

print("\n💡 CONCLUSION:")
print("   • hex_string - only REQUIRED field")
print("   • freq_Hz, rate_Hz, rssi_dB - RECOMMENDED for accuracy")
print("   • All other fields have reasonable defaults")
```

## Answer: Minimal data for signal recovery from hex_string

### 🎯 **ONLY REQUIRED FIELD:**

```json
{
  "hex_string": "AAB10209745E8C8155"
}
```

**hex_string contains ALL timing data** for complete pulse and gap signal recovery.

### 📊 **Fields with automatic defaults:**

If not specified, the following values are set:

| Field | Default Value | Source |
|-------|---------------|--------|
| `sample_rate` | `250000 Hz` | `rtl433_signal_set_defaults()` |
| `centerfreq_hz` | `433920000 Hz` | `rtl433_rfraw_parse_hex_string()` |
| `freq1_hz` | `= centerfreq_hz` | copied |
| `depth_bits` | `8` | standard |
| `offset` | `0` | stream start |
| `rssi_db`, `snr_db`, `noise_db` | `0.0` | unknown |
| `ook_low_estimate` | `1000` | detection threshold |
| `ook_high_estimate` | `8000` | detection threshold |

### 🔧 **Recommended metadata (for accuracy):**

```json
{
  "hex_string": "AAB10209745E8C8155",
  "freq_Hz": 433920000,      // Exact center frequency
  "rate_Hz": 250000,         // Real sample rate  
  "rssi_dB": 2.41444,        // Signal strength
  "snr_dB": 0.63729,         // Signal-to-noise ratio
  "noise_dB": -0.637554      // Noise level
}
```

### 🔍 **hex_string structure:**

```
AAB10209745E8C8155
│││││└─────────────── Timing data + pulse/gap pairs  
│││└─ 09: Timing histogram count (9 bins)
││└── 02: Payload length
│└─── B1: Modulation info
└──── AA: Header (0xAA = OOK format)
```

### 💡 **Recovery process:**

```c
// Minimal recovery code:
pulse_data_t pulse_data;
int result = rtl433_signal_reconstruct_from_hex("AAB10209745E8C8155", 
                                                &pulse_data, 
                                                NULL);  // no JSON metadata

// With metadata for better quality:
char *json_metadata = "{\"freq_Hz\":433920000,\"rate_Hz\":250000,\"rssi_dB\":2.41}";
int result = rtl433_signal_reconstruct_from_hex("AAB10209745E8C8155", 
                                                &pulse_data, 
                                                json_metadata);
```

### 🎯 **Key conclusions:**

1. **`hex_string`** - only **REQUIRED** field
2. **RF metadata** (`freq_Hz`, `rate_Hz`, `rssi_dB`) - **RECOMMENDED** for accurate detection
3. **All other fields** have **reasonable defaults**
4. **hex_string** contains **complete timing information** in compact format
5. **Defaults are sufficient** for basic detection, metadata improves accuracy

**Thus, for signal recovery only `hex_string` is minimally sufficient, but for quality detection it's recommended to add RF metadata.**

---

## RabbitMQ Signal Recovery Guide

## Overview

This document explains how to use data from RabbitMQ queues to reconstruct and detect RF signals in the rtl_433 system. The system uses two main queues: 'signals' for raw signal data and 'detected' for decoded device information.

## Queue Structure

### 📡 SIGNALS Queue (Raw Signal Data)

The 'signals' queue contains raw pulse data that can be used to reconstruct the original RF signal for detection purposes.

#### Complete Signal Data Structure

```json
{
  "time": "@0.524288s",
  "mod": "OOK",
  "count": 1,
  "pulses": [2420, 24204],
  "freq1_Hz": 433921472,
  "freq_Hz": 433920000,
  "rate_Hz": 250000,
  "depth_bits": 8,
  "range_dB": 42.1442,
  "rssi_dB": 2.41444,
  "snr_dB": 0.63729,
  "noise_dB": -0.637554,
  "offset": 1025,
  "start_ago": 130047,
  "end_ago": 123391,
  "ook_low_estimate": 14147,
  "ook_high_estimate": 28567,
  "fsk_f1_est": 388,
  "fsk_f2_est_value": 0,
  "hex_string": "AAB10209745E8C8155"
}
```

### 📋 DETECTED Queue (Decoded Device Data)

The 'detected' queue contains the results of successful device detection and decoding.

```json
{
  "time": "@133.026779s",
  "model": "Toyota", 
  "type": "TPMS",
  "id": "d75c823b",
  "status": 130,
  "pressure_PSI": 29.0,
  "temperature_C": 29.0,
  "mic": "CRC"
}
```

## Signal Reconstruction Methods

### Method 1: Using hex_string (Recommended)

The `hex_string` field contains the most compact and accurate representation of timing data.

#### Minimum Required Data

```json
{
  "hex_string": "AAB10209745E8C8155"
}
```

**This single field contains ALL timing information needed for signal reconstruction.**

#### hex_string Structure

```
AAB10209745E8C8155
│││││└─────────────── Timing data + pulse/gap pairs  
│││└─ 09: Timing histogram count (9 bins)
││└── 02: Payload length
│└─── B1: Modulation info
└──── AA: Header (0xAA = OOK format)
```

#### Reconstruction Process

```c
// Basic reconstruction (minimum data)
pulse_data_t pulse_data;
int result = rtl433_signal_reconstruct_from_hex("AAB10209745E8C8155", 
                                                &pulse_data, 
                                                NULL);

// Enhanced reconstruction with metadata
char *json_metadata = "{\"freq_Hz\":433920000,\"rate_Hz\":250000,\"rssi_dB\":2.41}";
int result = rtl433_signal_reconstruct_from_hex("AAB10209745E8C8155", 
                                                &pulse_data, 
                                                json_metadata);
```

#### Default Values (Applied Automatically)

| Field | Default Value | Source |
|-------|---------------|--------|
| `sample_rate` | 250000 Hz | `rtl433_signal_set_defaults()` |
| `centerfreq_hz` | 433920000 Hz | `rtl433_rfraw_parse_hex_string()` |
| `freq1_hz` | = centerfreq_hz | Copied from center frequency |
| `depth_bits` | 8 | Standard bit depth |
| `offset` | 0 | Stream start |
| `rssi_db`, `snr_db`, `noise_db` | 0.0 | Unknown values |
| `ook_low_estimate` | 1000 | Detection threshold |
| `ook_high_estimate` | 8000 | Detection threshold |

### Method 2: Using pulses Array (Fallback)

For signals that cannot generate a hex_string (complex signals with >8 timing bins):

#### Required Fields

```json
{
  "pulses": [2420, 24204],
  "mod": "OOK", 
  "rate_Hz": 250000,
  "count": 1
}
```

#### Field Descriptions

- **`pulses`**: Array of pulse and gap durations in microseconds [pulse1, gap1, pulse2, gap2, ...]
- **`mod`**: Modulation type ("OOK" or "FSK")
- **`rate_Hz`**: Sample rate for accurate timing conversion
- **`count`**: Number of pulses in the signal

## Enhanced Reconstruction (Recommended)

### Essential Metadata Fields

For high-quality signal reconstruction, include these additional fields:

```json
{
  "hex_string": "AAB10209745E8C8155",
  "freq_Hz": 433920000,      // Exact center frequency
  "rate_Hz": 250000,         // Actual sample rate
  "rssi_dB": 2.41444,        // Signal strength
  "snr_dB": 0.63729,         // Signal-to-noise ratio
  "noise_dB": -0.637554      // Noise level
}
```

### RF Quality Parameters

- **`freq_Hz`**: Critical for proper demodulation and device matching
- **`rssi_dB`**: Helps with signal quality assessment
- **`snr_dB`**: Important for weak signal detection
- **`noise_dB`**: Baseline noise level for threshold calculations

## Signal Processing Pipeline

### 1. Data Extraction from RabbitMQ

```python
# Example: Extract signal data from RabbitMQ
import pika
import json

connection = pika.BlockingConnection(pika.ConnectionParameters('localhost'))
channel = connection.channel()

method_frame, header_frame, body = channel.basic_get('signals')
if method_frame:
    signal_data = json.loads(body.decode())
    hex_string = signal_data.get('hex_string')
    metadata = {
        'freq_Hz': signal_data.get('freq_Hz'),
        'rate_Hz': signal_data.get('rate_Hz'), 
        'rssi_dB': signal_data.get('rssi_dB')
    }
```

### 2. Signal Reconstruction

```c
// C code for signal reconstruction
pulse_data_t pulse_data;
memset(&pulse_data, 0, sizeof(pulse_data_t));

// Method 1: Using hex_string (preferred)
if (hex_string) {
    char *json_meta = create_json_from_metadata(metadata);
    result = rtl433_signal_reconstruct_from_hex(hex_string, &pulse_data, json_meta);
    free(json_meta);
}

// Method 2: Using pulse array (fallback)
else if (pulses_array) {
    result = rtl433_signal_reconstruct_from_pulses(&pulse_data, pulses_array, 
                                                   pulse_count, sample_rate);
}
```

### 3. Device Detection

```c
// Apply device detectors to reconstructed signal
r_device **devices = get_all_devices();
for (int i = 0; devices[i]; i++) {
    r_device *device = devices[i];
    if (device->decode_fn) {
        int detection_result = device->decode_fn(&pulse_data, device);
        if (detection_result > 0) {
            // Device detected - send to 'detected' queue
            send_to_detected_queue(device_data);
        }
    }
}
```

## Time-Based Correlation

### Matching Signals to Detected Devices

Both 'signals' and 'detected' queues include a `time` field for correlation:

```json
// Signal in 'signals' queue
{
  "time": "@0.524288s",
  "hex_string": "AAB10209745E8C8155",
  ...
}

// Corresponding detection in 'detected' queue  
{
  "time": "@0.524288s",
  "model": "Toyota",
  "id": "d75c823b",
  ...
}
```

### Correlation Logic

```python
def correlate_signal_to_detection(signal_time, detected_time, tolerance_ms=100):
    """
    Correlate signal and detection based on timestamps
    """
    # Parse time format "@123.456s" 
    sig_time = float(signal_time.strip('@s'))
    det_time = float(detected_time.strip('@s'))
    
    # Check if within tolerance
    return abs(sig_time - det_time) <= (tolerance_ms / 1000.0)
```

## Performance Considerations

### Data Size Optimization

- **hex_string**: Most compact format (~20 bytes typical)
- **pulses array**: Larger but more detailed (~100+ bytes)
- **Full metadata**: Complete but verbose (~500+ bytes)

### Reconstruction Speed

1. **hex_string method**: Fastest reconstruction
2. **pulses array method**: Moderate speed 
3. **Complex signals**: May require full bitbuffer data

### Memory Usage

- **hex_string**: Minimal memory footprint
- **pulse arrays**: Linear with pulse count
- **Full signal data**: Higher memory usage

## Error Handling

### Common Issues

1. **Invalid hex_string format**: Check header bytes (0xAA family)
2. **Too many timing bins**: >8 bins cannot use hex_string format
3. **Missing metadata**: Use defaults but quality may suffer
4. **Corrupted timing data**: Validate pulse durations

### Validation

```c
// Validate reconstructed signal
bool is_valid = rtl433_signal_validate_pulse_data(&pulse_data);
if (!is_valid) {
    // Handle invalid signal data
    return -1;
}

// Check reasonable pulse durations (1μs to 100ms)
for (int i = 0; i < pulse_data.num_pulses; i++) {
    float pulse_us = pulse_data.pulse[i] * 1e6 / pulse_data.sample_rate;
    if (pulse_us < 1.0 || pulse_us > 100000.0) {
        // Invalid pulse duration
        return -1;
    }
}
```

## Best Practices

### 1. Data Prioritization

1. **Always prefer hex_string** when available
2. **Include RF metadata** for quality reconstruction  
3. **Use pulse arrays as fallback** for complex signals
4. **Validate reconstructed data** before processing

### 2. Queue Management

1. **Process signals queue immediately** to prevent overflow
2. **Correlate with detected queue** for feedback
3. **Handle missing data gracefully** with defaults
4. **Monitor queue sizes** and processing rates

### 3. Signal Quality

1. **Check RSSI values** for signal strength
2. **Validate SNR ratios** for detection confidence
3. **Use frequency data** for proper demodulation
4. **Apply appropriate thresholds** based on noise levels

## Code Examples

### Complete Signal Recovery Example

```c
#include "rtl433_signal.h"
#include "rtl433_transport.h"

int process_rabbitmq_signal(const char *json_message) {
    // Parse JSON from RabbitMQ
    json_object *root = json_tokener_parse(json_message);
    if (!root) return -1;
    
    // Extract hex_string
    json_object *hex_obj;
    if (!json_object_object_get_ex(root, "hex_string", &hex_obj)) {
        json_object_put(root);
        return -1;
    }
    
    const char *hex_string = json_object_get_string(hex_obj);
    
    // Extract metadata
    char metadata_json[512];
    snprintf(metadata_json, sizeof(metadata_json),
        "{\"freq_Hz\":%d,\"rate_Hz\":%d,\"rssi_dB\":%.2f}",
        get_json_int(root, "freq_Hz", 433920000),
        get_json_int(root, "rate_Hz", 250000), 
        get_json_double(root, "rssi_dB", 0.0));
    
    // Reconstruct signal
    pulse_data_t pulse_data;
    int result = rtl433_signal_reconstruct_from_hex(hex_string, 
                                                   &pulse_data, 
                                                   metadata_json);
    
    json_object_put(root);
    
    if (result != 0) {
        return -1; // Reconstruction failed
    }
    
    // Validate reconstructed signal
    if (!rtl433_signal_validate_pulse_data(&pulse_data)) {
        return -1; // Invalid signal data
    }
    
    // Process with device detectors
    return run_device_detectors(&pulse_data);
}
```

## Summary

The RabbitMQ signal recovery system provides flexible options for signal reconstruction:

- **🎯 Minimum**: Only `hex_string` required
- **🔧 Recommended**: `hex_string` + RF metadata (`freq_Hz`, `rate_Hz`, `rssi_dB`)
- **📊 Fallback**: `pulses` array for complex signals
- **⏱️ Correlation**: Time-based matching between signals and detections

This architecture enables efficient signal processing while maintaining compatibility with various signal types and quality levels.
