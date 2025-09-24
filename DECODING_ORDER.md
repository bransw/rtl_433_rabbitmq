# Decoding Execution Order in rtl_433_client

## 🔄 **ANSWER TO THE QUESTION:**

In our `rtl_433_client`, **device decoding is called BEFORE `pulse_analyzer`**.

`pulse_analyzer` is used **only as a fallback** for debugging when no devices were decoded.

## 📊 **UNIFIED SIGNAL PROCESSING FLOW:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        INPUT SOURCES (Multiple Options)                     │
└─────────────────────────────────────────────────────────────────────────────┘

    ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
    │   SDR Device     │    │    File Input    │    │  RabbitMQ Input  │
    │   (-d rtlsdr)    │    │ (-r file.cu8)    │    │(-r rabbitmq://...)│
    └─────────┬────────┘    └─────────┬────────┘    └─────────┬────────┘
              │                       │                       │
              └───────────┬───────────┘                       │
                          │                                   │
                          ▼                                   ▼
                ┌─────────────────┐                 ┌─────────────────┐
                │  AM/FM Convert  │                 │ Read JSON from  │
                │(Baseband conv.) │                 │ 'signals' Queue │
                └─────────┬───────┘                 └─────────┬───────┘
                          │                                   │
                          ▼                                   ▼
                ┌─────────────────┐                 ┌─────────────────┐
                │ Pulse Detection │                 │Signal Reconstru-│
                │(pulse_detect_*) │                 │ction from JSON │
                └─────────┬───────┘                 └─────────┬───────┘
                          │                                   │
                          └───────────┬───────────────────────┘
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

   📤 Console JSON         📤 RabbitMQ Output      📤 File Output        📤 MQTT Output
   (-F json)               (-F rabbitmq://...)     (-F file://...)       (-F mqtt://...)
        │                         │                      │                     │
        ▼                         ▼                      ▼                     ▼
   ┌─────────┐            ┌─────────────────┐    ┌─────────────────┐   ┌─────────────────┐
   │ stdout  │            │ Raw Signals     │    │ JSON to Files   │   │ MQTT Broker     │
   │ output  │            │ → 'signals'     │    │ (decoded data)  │   │ (external sys)  │
   └─────────┘            │ Detected Devs   │    └─────────────────┘   └─────────────────┘
                          │ → 'detected'    │
                          └─────────────────┘
                                    │
                              ┌─────┴─────┐
                              │ Enhanced  │
                              │JSON with  │
                              │hex_string │
                              │+ metadata │
                              └───────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                        🔄 DISTRIBUTED PROCESSING                            │
└─────────────────────────────────────────────────────────────────────────────┘

CLIENT INSTANCE                 RabbitMQ                 SERVER INSTANCE(S)
     │                                                         │
     ▼                                                         ▼
File/SDR → Enhanced JSON  ────→ 'signals' Queue  ────→ JSON → Reconstruction
     │                                                         │
     ▼                                                         ▼
Local Decoding           ────→ 'detected' Queue ←──── Remote Decoding Results
     │                                                    (Scalable processing)
     ▼
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
- **Multiple Input Sources**: SDR devices, Files, RabbitMQ queues - all use same processing pipeline
- **Multiple Output Options**: Console, RabbitMQ, Files, MQTT - flexible data routing  
- **Performance**: Decoding goes first for fast recognition across all input types
- **Scalability**: Distributed processing via RabbitMQ for high-throughput scenarios

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