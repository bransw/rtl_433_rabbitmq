# RTL_433 Split Architecture Demo

This directory contains examples demonstrating the split architecture.

## Quick Start

### 1. Build the project
```bash
./build_all.sh
```

### 2. Test the split architecture
```bash
./test_split_architecture.sh
```

### 3. Manual testing

#### Start Server
```bash
# Terminal 1: Start server
cd build
./rtl_433_server -H localhost:8081 -v

# Server will listen on:
# - HTTP: http://localhost:8081 (for receiving signals from client)
# - Management: Web interface for monitoring
```

#### Start Client
```bash
# Terminal 2: Start client (stub mode - no real SDR)
cd build
./rtl_433_client -T http://localhost:8081/api/signals -v

# Client will:
# - Try to connect to RTL-SDR device (will fail with stubs)
# - Send any detected signals to server
```

## Real Hardware Setup

### 1. Install RTL-SDR support
```bash
# Install real RTL-SDR libraries
sudo apt install librtlsdr0 librtlsdr-dev

# For WSL: Setup USB forwarding
# See README_SPLIT.md for detailed instructions
```

### 2. Enable real SDR in client
Edit `CMakeLists.txt` and uncomment RTL-SDR linking:
```cmake
# Uncomment these lines for real SDR:
if(LIBRTLSDR_FOUND)
    target_include_directories(rtl_433_client PRIVATE ${LIBRTLSDR_INCLUDE_DIRS})
    target_link_libraries(rtl_433_client ${LIBRTLSDR_LIBRARIES})
    target_compile_options(rtl_433_client PRIVATE ${LIBRTLSDR_CFLAGS_OTHER})
    target_compile_definitions(rtl_433_client PRIVATE RTLSDR)
endif()
```

### 3. Rebuild and test
```bash
./build_all.sh
./rtl_433_client -d 0 -f 433.92M -T http://localhost:8081/api/signals
```

## Transport Options

### HTTP (Default)
```bash
# Server listens on HTTP
./rtl_433_server -H localhost:8081

# Client sends to HTTP endpoint
./rtl_433_client -T http://localhost:8081/api/signals
```

### MQTT (if enabled)
```bash
# Server connects to MQTT broker
./rtl_433_server -M mqtt://localhost:1883

# Client publishes to MQTT
./rtl_433_client -M mqtt://localhost:1883 -t rtl433/signals
```

## Data Flow

```
RTL-SDR Device → Client (Demodulation) → Transport → Server (Decoding) → Queues
                                                                      ├─ Ready Queue (Known devices)
                                                                      └─ Unknown Queue (Unknown signals)
```

## Monitoring

The server provides:
- Statistics at `/stats` endpoint
- Queue status monitoring
- Device identification results
- Unknown signal analysis

## Troubleshooting

### Client issues:
- "No RTL-SDR devices found" → Check USB setup in WSL
- "Connection refused" → Ensure server is running
- "Permission denied" → Check udev rules for RTL-SDR

### Server issues:
- "Address already in use" → Change port with `-H localhost:8082`
- High memory usage → Check queue sizes in configuration
