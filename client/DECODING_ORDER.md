# Decoding Execution Order in rtl_433_client

## 🔄 **ANSWER TO THE QUESTION:**

In our `rtl_433_client`, **device decoding is called BEFORE `pulse_analyzer`**.

`pulse_analyzer` is used **only as a fallback** for debugging when no devices were decoded.

## 📊 **UNIFIED SIGNAL PROCESSING FLOW:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        INPUT SOURCES (Multiple Options)                     │
└─────────────────────────────────────────────────────────────────────────────┘

    ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
    │   SDR Device     │    │    File Input    │    │  RabbitMQ Input  │    │   ASN.1 Input    │
    │   (-d rtlsdr)    │    │ (-r file.cu8)    │    │(-r rabbitmq://...)│    │ (-r asn1://...)  │
    └─────────┬────────┘    └─────────┬────────┘    └─────────┬────────┘    └─────────┬────────┘
              │                       │                       │                       │
              └───────────┬───────────┘                       │                       │
                          │                                   │                       │
                          ▼                                   ▼                       ▼
                ┌─────────────────┐                 ┌─────────────────┐     ┌─────────────────┐
                │  AM/FM Convert  │                 │ Read JSON from  │     │Read ASN.1 Binary│
                │(Baseband conv.) │                 │ 'signals' Queue │     │from Queue & PER │
                └─────────┬───────┘                 └─────────┬───────┘     │    Decode       │
                          │                                   │             └─────────┬───────┘
                          ▼                                   ▼                       │
                ┌─────────────────┐                 ┌─────────────────┐               │
                │ Pulse Detection │                 │Signal Reconstru-│               │
                │(pulse_detect_*) │                 │ction from JSON │               │
                └─────────┬───────┘                 └─────────┬───────┘               │
                          │                                   │                       │
                          └───────────┬───────────────────────┼───────────────────────┘
                                      │                       │
                                      └───────────────────────┘
                                      │
                                ┌─────┴─────┐
                                │           │
                                ▼           ▼
                        ┌─────────┐ ┌─────────┐
                        │OOK Data │ │FSK Data │ (pulse_data_t structures)
                        └────┬────┘ └────┬────┘
                             │           │
                             └─────┬─────┘
                                   │
                                   ▼
                        ┌─────────────────┐
                        │ Device Decoding │ ← 🔍 run_ook_demods() / run_fsk_demods()
                        └─────────┬───────┘     (488 registered decoders)
                                  │
                            ┌─────┴─────┐
                            │           │
                            ▼           ▼
                        ┌─────────┐ ┌─────────┐
                        │SUCCESS  │ │ FAILED  │
                        │Decoded  │ │No Match │
                        └────┬────┘ └────┬────┘
                             │           │
                             ▼           ▼
                        ┌─────────────────┐ ┌─────────────────┐
                        │ OUTPUT ROUTING  │ │ pulse_analyzer  │ ← 🔬 Debug only
                        └─────────┬───────┘ └─────────────────┘   (When verbosity >= DEBUG)
                                  │
                    ┌─────────────┼─────────────┐
                    │             │             │
                    ▼             ▼             ▼

┌─────────────────────────────────────────────────────────────────────────────┐
│                     OUTPUT OPTIONS (Multiple Destinations)                  │
└─────────────────────────────────────────────────────────────────────────────┘

   📤 Console JSON    📤 RabbitMQ Output   📤 ASN.1 Output     📤 File Output    📤 MQTT Output
   (-F json)          (-F rabbitmq://...) (-F asn1://...)      (-F file://...)   (-F mqtt://...)
        │                     │                  │                    │               │
        ▼                     ▼                  ▼                    ▼               ▼
   ┌─────────┐        ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
   │ stdout  │        │ Raw Signals     │ │ Binary ASN.1    │ │ JSON to Files   │ │ MQTT Broker     │
   │ output  │        │ → 'signals'     │ │ → 'asn1_signals'│ │ (decoded data)  │ │ (external sys)  │
   └─────────┘        │ Detected Devs   │ │ → 'asn1_detected'│ └─────────────────┘ └─────────────────┘
                      │ → 'detected'    │ │ (PER Encoded)   │
                      └─────────────────┘ └─────────────────┘
                                    │
                              ┌─────┴─────┐                    ┌─────┴─────┐
                              │ Enhanced  │                    │ Compact   │
                              │JSON with  │                    │Binary PER │
                              │hex_string │                    │Encoding   │
                              │+ metadata │                    │~70% size  │
                              └───────────┘                    └───────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                        🔄 DISTRIBUTED PROCESSING                            │
└─────────────────────────────────────────────────────────────────────────────┘

CLIENT INSTANCE                 RabbitMQ                 SERVER INSTANCE(S)
     │                                                         │
     ▼                                                         ▼
File/SDR → Enhanced JSON  ────→ 'signals' Queue  ────→ JSON → Reconstruction
     │                                                         │
     │         ASN.1 Binary ────→ 'asn1_signals' ────→ PER → pulse_data_t
     │                                                         │
     ▼                                                         ▼
Local Decoding           ────→ 'detected' Queue ←──── Remote Decoding Results
     │                                                    (Scalable processing)
     │                    ────→ 'asn1_detected' ←──── Binary Encoded Results
     ▼                                                    (70% smaller messages)
📊 Statistics Display
```

## 📋 **DETAILED EXECUTION ORDER:**

### 1. **File Processing (`process_cu8_file`):**

```c
// Lines 138-153 in rtl_433_client_v2.c
// Step 4a: Always send pulse_data first (raw demodulated data)
client_pulse_handler_with_type(&pulse_data, PULSE_DATA_OOK);

// Step 4b: Try to decode devices 
int ook_events = run_ook_demods(&g_cfg->demod->r_devs, &pulse_data);

if (ook_events > 0) {
    // SUCCESS: Device decoded, client_data_acquired_handler() was called
    print_logf(LOG_INFO, "Client", "OOK package decoded %d device events", ook_events);
} else {
    // FAILED: No device match, use pulse_analyzer for debugging
    if (g_cfg->verbosity >= LOG_DEBUG) {
        r_device device = {.log_fn = log_device_handler, .output_ctx = g_cfg};
        pulse_analyzer(&pulse_data, package_type, &device);
    }
}
```

### 2. **SDR Processing (`client_sdr_callback`):**

```c
// Lines 417-424 in rtl_433_client_v2.c
// Try to decode devices (this will call client_data_acquired_handler if successful)
int ook_events = run_ook_demods(&cfg->demod->r_devs, &pulse_data);

if (ook_events > 0) {
    print_logf(LOG_INFO, "SDR", "OOK package decoded %d device events", ook_events);
} else {
    print_logf(LOG_DEBUG, "SDR", "OOK package: no devices decoded");
    // Note: pulse_analyzer NOT called in SDR mode for performance
}
```

## 🔍 **WHAT HAPPENS IN `run_ook_demods()`:**

1. **Iterate through all registered devices** by priority
2. **For each device:**
   ```c
   switch (r_dev->modulation) {
   case OOK_PULSE_PCM:
       p_events += pulse_slicer_pcm(pulse_data, r_dev);  // ← Decoding!
       break;
   case OOK_PULSE_PWM:
       p_events += pulse_slicer_pwm(pulse_data, r_dev);  // ← Decoding!
       break;
   // ... other modulation types
   }
   ```

3. **In each `pulse_slicer_*()` function:**
   - Analyze pulse timing characteristics
   - Convert to `bitbuffer_t` (bits)
   - Call `account_event()` → `device->decode_fn()` → `client_data_acquired_handler()`

## 🔬 **WHEN `pulse_analyzer` IS CALLED:**

```c
// ONLY if:
1. ook_events == 0 (no devices decoded)
2. g_cfg->verbosity >= LOG_DEBUG (debug enabled)
3. File processing mode (NOT real-time with SDR)
```

### **The `pulse_analyzer` function:**

1. **Statistical analysis** (pulse/gap histograms)
2. **Modulation type guessing** (PWM, PPM, FSK)
3. **Output parameters for flex decoder**
4. **Generate RfRaw for triq.org**
5. **Optional decoding attempt** with guessed parameters

## ⚡ **KEY TAKEAWAYS:**

### **🎯 UNIFIED ARCHITECTURE BENEFITS:**
- **Multiple Input Sources**: SDR devices, Files, RabbitMQ queues, ASN.1 binary streams - all use same processing pipeline
- **Multiple Output Options**: Console, RabbitMQ, Files, MQTT, ASN.1 binary - flexible data routing  
- **Performance**: Decoding goes first for fast recognition across all input types
- **Scalability**: Distributed processing via RabbitMQ for high-throughput scenarios
- **Efficiency**: ASN.1 binary encoding reduces message size by ~70% for high-volume scenarios

### **🔄 PROCESSING FLOW:**
1. **Input Normalization**: All sources converge to `pulse_data_t` structures
2. **Universal Decoding**: Same 488 decoders work on any input source
3. **Flexible Output**: Route results to any combination of destinations
4. **Debug Fallback**: `pulse_analyzer` available for unknown signals regardless of input

### **🎯 DISTRIBUTED PROCESSING:**
- **Horizontal Scaling**: Multiple SERVER instances can process from same RabbitMQ
- **Load Balancing**: Queue-based distribution automatically balances workload
- **Enhanced JSON**: Complete signal metadata enables perfect reconstruction
- **Statistics Tracking**: Real-time monitoring across distributed instances

### **📊 EXECUTION ORDER (All Modes):**
```
INPUT → pulse_data_t → DECODING (488 decoders) → OUTPUT ROUTING
                           │
                           └→ pulse_analyzer (debug fallback)
```

**Universal principle: DECODING FIRST → debug analysis only on failure** 🎯

## 📦 **ASN.1 BINARY PROCESSING:**

### **🔄 ASN.1 Input Flow:**
```c
// ASN.1 Input Processing (rtl433_input.c)
asn1://guest:guest@localhost:5672/asn1_signals
    │
    ▼
rtl433_input_init_asn1_from_url()  // Initialize ASN.1 input
    │
    ▼
internal_asn1_message_handler()    // Receive binary message
    │
    ▼
rtl433_input_parse_pulse_data_from_asn1()  // PER decode to pulse_data_t
    │
    ▼
rabbitmq_pulse_handler()           // Standard pulse processing
    │
    ▼
run_ook_demods() / run_fsk_demods()  // Same 488 decoders
```

### **📤 ASN.1 Output Flow:**
```c
// ASN.1 Output Processing (output_asn1.c)
pulse_data_t → rtl433_asn1_encode_pulse_data_to_signal()
    │                                    │
    ▼                                    ▼
SignalMessage (ASN.1)            DetectedMessage (ASN.1)
    │                                    │
    ▼                                    ▼
PER Binary Encoding              PER Binary Encoding
    │                                    │
    ▼                                    ▼
'asn1_signals' Queue            'asn1_detected' Queue
```

### **🎯 ASN.1 ADVANTAGES:**

#### **📊 Size Efficiency:**
- **JSON Message**: ~500-1000 bytes (text-based)
- **ASN.1 PER Message**: ~150-300 bytes (binary)
- **Compression Ratio**: ~70% size reduction

#### **⚡ Performance Benefits:**
- **Faster Parsing**: Binary format vs JSON parsing
- **Network Efficiency**: Reduced bandwidth usage
- **Memory Usage**: Smaller message footprint
- **Processing Speed**: Direct binary-to-struct conversion

#### **🔒 Data Integrity:**
- **Schema Validation**: ASN.1 enforces strict data types
- **Version Control**: Schema evolution support
- **Cross-Platform**: Standard binary encoding
- **Backwards Compatibility**: ASN.1 extensibility rules

### **🔧 ASN.1 TECHNICAL DETAILS:**

#### **Encoding Rules:**
- **PER (Packed Encoding Rules)**: Most compact binary format
- **Automatic Length Encoding**: Variable-length fields optimized
- **Bit-Level Packing**: Minimal overhead for boolean/enum fields

#### **Message Types:**
```asn1
SignalMessage ::= SEQUENCE {
    packageId       INTEGER,
    timestamp       GeneralizedTime,
    hexString       OCTET STRING OPTIONAL,
    pulsesData      PulsesData OPTIONAL,
    frequency       INTEGER,
    modulation      INTEGER,
    rssi           INTEGER OPTIONAL,
    snr            INTEGER OPTIONAL,
    noise          INTEGER OPTIONAL
}

DetectedMessage ::= SEQUENCE {
    packageId       INTEGER,
    timestamp       GeneralizedTime,
    model           UTF8String,
    deviceType      UTF8String OPTIONAL,
    deviceId        UTF8String OPTIONAL,
    protocol        UTF8String OPTIONAL,
    dataFields      SEQUENCE OF KeyValuePair OPTIONAL
}
```

#### **Integration Points:**
- **Input**: `rtl433_input_init_asn1_from_url()` - Seamless RabbitMQ integration
- **Output**: `data_output_asn1_create()` - Parallel to JSON/RabbitMQ outputs
- **Decoding**: `rtl433_asn1_decode_signal_to_pulse_data()` - Direct pulse_data_t reconstruction
- **Encoding**: `rtl433_asn1_encode_pulse_data_to_signal()` - Complete signal preservation

### **📋 ASN.1 USAGE EXAMPLES:**

#### **High-Volume Signal Collection:**
```bash
# Producer (high-throughput SDR)
rtl_433 -F asn1://guest:guest@localhost:5672/rtl_433

# Consumer (distributed processing)
rtl_433_client -r asn1://guest:guest@localhost:5672/asn1_signals
```

#### **Distributed Processing Pipeline:**
```bash
# Stage 1: Raw signal capture (binary efficiency)
rtl_433 -d 0 -F asn1://guest:guest@server1:5672/rtl_433

# Stage 2: Signal processing (multiple instances)
rtl_433_client -r asn1://guest:guest@server1:5672/asn1_signals -F json
rtl_433_client -r asn1://guest:guest@server1:5672/asn1_signals -F mqtt://server2

# Stage 3: Results aggregation
rtl_433_client -r asn1://guest:guest@server1:5672/asn1_detected -F file://results.json
```

**🎯 ASN.1 enables high-performance, distributed RF signal processing with minimal network overhead!**