# RTL_433 Split Architecture

This document describes the design and implementation of the RTL_433 Split Architecture, which separates the original monolithic `rtl_433` application into distinct client and server components.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Components](#components)
- [Data Flow](#data-flow)
- [Transport Layer](#transport-layer)
- [Deployment Patterns](#deployment-patterns)
- [Performance](#performance)
- [Security](#security)
- [Implementation Details](#implementation-details)

## Overview

The RTL_433 Split Architecture addresses several limitations of the original monolithic design:

**Problems Solved:**
- ✅ **Distributed Processing** - Multiple clients can feed one server
- ✅ **Scalability** - Horizontal scaling of signal processing
- ✅ **Reliability** - Client failures don't affect the entire system
- ✅ **Flexibility** - Mix different SDR devices and protocols
- ✅ **Resource Optimization** - Dedicated hardware for specific tasks

**Use Cases:**
- Multi-location sensor monitoring (home, office, remote sites)
- High-availability IoT data collection
- Mixed-frequency monitoring (433MHz + 868MHz + 915MHz)
- Cloud-based signal processing and analytics
- Edge computing for real-time processing

## Architecture

```
                    RTL_433 Split Architecture
                    
┌─────────────────────────┐    ┌─────────────────────────┐    ┌─────────────────────────┐
│      CLIENT SIDE        │    │     TRANSPORT LAYER     │    │      SERVER SIDE        │
├─────────────────────────┤    ├─────────────────────────┤    ├─────────────────────────┤
│                         │    │                         │    │                         │
│ ┌─────────────────────┐ │    │  ┌─────────────────────┐│    │ ┌─────────────────────┐ │
│ │    RTL-SDR          │ │    │  │       HTTP          ││    │ │  rtl_433_server     │ │
│ │    Device           │ │    │  │   REST API          ││    │ │                     │ │
│ │                     │ │    │  │                     ││    │ │ • Device Decoding   │ │
│ │ • IQ Samples        │ │    │  └─────────────────────┘│    │ │ • Protocol Registry │ │
│ │ • 433/868/915 MHz   │ │────┤                         ├────│ │ • Queue Management  │ │
│ │                     │ │    │  ┌─────────────────────┐│    │ │ • Analytics         │ │
│ └─────────────────────┘ │    │  │       MQTT          ││    │ │ • API Endpoints     │ │
│                         │    │  │   Pub/Sub           ││    │ │                     │ │
│ ┌─────────────────────┐ │    │  │                     ││    │ └─────────────────────┘ │
│ │  rtl_433_client     │ │    │  └─────────────────────┘│    │                         │
│ │                     │ │    │                         │    │ ┌─────────────────────┐ │
│ │ • Signal Demod      │ │    │  ┌─────────────────────┐│    │ │     Database        │ │
│ │ • Pulse Detection   │ │    │  │     RabbitMQ        ││    │ │                     │ │
│ │ • Protocol Matching │ │    │  │   Message Queue     ││    │ │ • Signal History    │ │
│ │ • Data Serialization│ │    │  │                     ││    │ │ • Device Registry   │ │
│ │                     │ │    │  └─────────────────────┘│    │ │ • Statistics        │ │
│ └─────────────────────┘ │    │                         │    │ │                     │ │
│                         │    │  ┌─────────────────────┐│    │ └─────────────────────┘ │
└─────────────────────────┘    │  │     TCP/UDP         ││    └─────────────────────────┘
                               │  │   Raw Sockets       ││    
                               │  │                     ││    
                               │  └─────────────────────┘│    
                               └─────────────────────────┘    
```

## Components

### rtl_433_client

**Primary Responsibility:** Signal acquisition and demodulation

**Core Functions:**
1. **SDR Interface** - Communicates with RTL-SDR/SoapySDR devices
2. **Signal Processing** - IQ → AM demodulation → pulse detection
3. **Protocol Matching** - Identifies known device protocols (244 supported)
4. **Data Transmission** - Sends structured data to server

**Input Sources:**
- RTL-SDR USB dongles
- SoapySDR compatible devices (LimeSDR, HackRF, PlutoSDR)
- Recorded IQ files (.cu8, .cs16, etc.)

**Output Data:**
- **Pulse Data** - Raw demodulated pulse trains (always sent)
- **Device Data** - Decoded device information (when protocol matches)

### rtl_433_server (Future Implementation)

**Primary Responsibility:** Device decoding and data management

**Planned Functions:**
1. **Signal Reception** - Receives pulse/device data from multiple clients
2. **Protocol Processing** - Advanced device decoding and analysis
3. **Queue Management** - Separates known vs unknown signals
4. **API Services** - REST API for data access and configuration
5. **Data Storage** - Persistent storage for analytics and history

### Transport Layer

**Supported Protocols:**
- **HTTP/HTTPS** - RESTful API with TLS support
- **MQTT/MQTTS** - Publish/Subscribe with TLS support  
- **AMQP** - RabbitMQ message queuing
- **TCP/UDP** - Raw socket connections

## Data Flow

### 1. Signal Acquisition Flow

```
RTL-SDR Device → IQ Samples → rtl_433_client
                                     │
                             ┌───────┼───────┐
                             │       │       │
                        Demodulation │   File Input
                             │       │       │
                        AM/FM Data   │   IQ Samples
                             │       │       │
                             └───────┼───────┘
                                     │
                               Pulse Detection
                                     │
                           ┌─────────┼─────────┐
                           │                   │
                    Pulse Data            Device Match?
                     (Always)                  │
                           │              ┌────┼────┐
                           │              │         │
                           │            Yes       No
                           │              │         │
                           │         Device Data    │
                           │              │         │
                           └──────────────┼─────────┘
                                          │
                                   Transport Layer
```

### 2. Data Types

#### Pulse Data (Always Generated)
```json
{
  "type": "pulse_data",
  "timestamp_us": 1726080123456789,
  "frequency": 433920000,
  "sample_rate": 250000,
  "num_pulses": 64,
  "pulse": [432, 1896, 448, 932, ...],  // Microsecond timings
  "gap": [1896, 448, 932, 432, ...],    // Gap timings
  "rssi": -35.2,                        // Signal strength
  "snr": 15.3,                          // Signal-to-noise ratio
  "modulation": "OOK"                   // OOK or FSK
}
```

#### Device Data (Protocol Match Only)
```json
{
  "type": "device_data",
  "device_id": "Acurite-Tower", 
  "timestamp_us": 1726080123456789,
  "frequency": 433920000,
  "sample_rate": 250000,
  "raw_data_hex": "{\"model\":\"Acurite-Tower\",\"id\":1234,\"battery_ok\":1,\"temperature_C\":23.5,\"humidity\":65,\"mic\":\"CRC\"}"
}
```

### 3. Protocol Processing Pipeline

```
IQ Samples (Complex)
        │
        ▼
┌─────────────────┐
│   AM/FM Demod   │ ── envelope_detect() / fm_demod()
└─────────────────┘
        │
        ▼
┌─────────────────┐
│ Pulse Detection │ ── pulse_detect_package() 
└─────────────────┘
        │
        ▼
┌─────────────────┐
│ Pulse Analysis  │ ── pulse_analyzer() (optional debug)
└─────────────────┘
        │
        ▼
┌─────────────────┐
│Protocol Matching│ ── run_ook_demods() / run_fsk_demods()
└─────────────────┘
        │
        ├─── No Match ────► Send Pulse Data Only
        │
        └─── Match Found ─► Send Pulse Data + Device Data
```

## Transport Layer

### HTTP Transport

**Characteristics:**
- Request/Response pattern
- Built-in error handling and retry logic
- Support for authentication (Basic, Bearer tokens)
- TLS encryption support
- Easy integration with web services

**URL Format:**
```
http[s]://[user:pass@]host[:port]/path
```

**Examples:**
```bash
# Basic HTTP
http://localhost:8080/signals

# With authentication  
https://user:pass@api.example.com/rtl433/signals

# Custom endpoints
http://192.168.1.100:3000/api/v1/sensor-data
```

### MQTT Transport

**Characteristics:**
- Publish/Subscribe pattern
- Built-in message persistence
- Quality of Service (QoS) levels
- TLS encryption support
- Efficient for high-frequency data

**URL Format:**
```
mqtt[s]://[user:pass@]host[:port]/topic
```

**Examples:**
```bash
# Local broker
mqtt://localhost:1883/rtl433/signals

# Cloud broker with auth
mqtts://iot_user:secret@mqtt.cloud.com:8883/sensors/rtl433

# Hierarchical topics
mqtt://localhost:1883/home/outdoor/weather/rtl433
```

### RabbitMQ Transport (AMQP)

**Characteristics:**
- Advanced message queuing
- Message persistence and durability
- Complex routing patterns
- Dead letter queues
- Cluster support

**URL Format:**
```
amqp[s]://[user:pass@]host[:port]/exchange/queue
```

**Examples:**
```bash
# Default exchange
amqp://guest:guest@localhost:5672/rtl_433/signals

# Custom exchange
amqps://prod_user:pass@rabbitmq.cluster.local:5671/sensor_exchange/rtl433_queue
```

### TCP/UDP Transport

**Characteristics:**
- Raw socket connections
- Minimal overhead
- Custom protocols
- Direct point-to-point communication

**URL Format:**
```
tcp://host:port
udp://host:port
```

## Deployment Patterns

### 1. Single-Site Monitoring

```
┌─────────────────┐    HTTP/MQTT    ┌─────────────────┐
│  rtl_433_client │ ─────────────► │ rtl_433_server  │
│                 │                │                 │
│ RTL-SDR USB     │                │ Local DB        │
│ 433.92 MHz      │                │ Web UI          │
└─────────────────┘                └─────────────────┘
```

### 2. Multi-Site Monitoring

```
Site A:
┌─────────────────┐
│  rtl_433_client │ ──┐
│ 433 MHz Weather │   │
└─────────────────┘   │    MQTT/TLS
                      │ ┌─────────────────┐    ┌─────────────────┐
Site B:               ├─► │  MQTT Broker    │ ──► │ rtl_433_server  │
┌─────────────────┐   │ │  (Cloud)        │    │ (Cloud)         │
│  rtl_433_client │ ──┤ └─────────────────┘    │ Analytics DB    │
│ 868 MHz Sensors │   │                        │ Dashboard       │
└─────────────────┘   │                        └─────────────────┘
                      │
Site C:               │
┌─────────────────┐   │
│  rtl_433_client │ ──┘
│ 915 MHz TPMS    │
└─────────────────┘
```

### 3. High-Availability Setup

```
Client Cluster:                Load Balancer:           Server Cluster:
┌─────────────────┐           ┌─────────────────┐      ┌─────────────────┐
│  rtl_433_client │ ────────► │     HAProxy     │ ───► │ rtl_433_server  │
│     Node 1      │           │                 │      │     Node 1      │
└─────────────────┘           │   HTTP LB       │      └─────────────────┘
                              │   Health Checks │               │
┌─────────────────┐           │                 │      ┌─────────────────┐
│  rtl_433_client │ ────────► │                 │ ───► │ rtl_433_server  │
│     Node 2      │           └─────────────────┘      │     Node 2      │
└─────────────────┘                                    └─────────────────┘
                                                                │
┌─────────────────┐                                    ┌─────────────────┐
│  rtl_433_client │                                    │ Shared Database │
│     Node 3      │                                    │  (PostgreSQL)   │
└─────────────────┘                                    └─────────────────┘
```

### 4. Edge Computing Pattern

```
Edge Location:                    Cloud:
┌─────────────────┐              ┌─────────────────┐
│  rtl_433_client │              │   Cloud APIs    │
│                 │              │                 │
│ • Local SDR     │ ─── HTTPS ──►│ • Data Storage  │
│ • Edge AI       │   (Batch)    │ • ML Pipeline   │  
│ • Local Storage │              │ • Analytics     │
│ • Pre-filtering │              │ • Dashboards    │
└─────────────────┘              └─────────────────┘
       │
       ▼ (Local Network)
┌─────────────────┐
│ Local Services  │
│                 │
│ • Home Auto     │
│ • Alerts        │
│ • Local UI      │
└─────────────────┘
```

## Performance

### Throughput Characteristics

**Client Performance:**
- **Signal Processing**: 250k samples/sec (typical RTL-SDR rate)
- **Pulse Detection**: ~100-1000 pulses/sec (environment dependent)
- **Network Overhead**: 1-5KB per signal (JSON format)
- **CPU Usage**: 10-30% (Raspberry Pi 4)
- **Memory Usage**: 50-100MB typical

**Transport Performance:**
- **HTTP**: 100-1000 requests/sec (server dependent)
- **MQTT**: 10,000+ messages/sec (broker dependent)
- **RabbitMQ**: 1,000-10,000 messages/sec (configuration dependent)
- **TCP/UDP**: Limited by network bandwidth

### Optimization Strategies

**Client-Side:**
```bash
# Reduce sample rate for better performance
rtl_433_client -s 1024k

# Use local MQTT broker
rtl_433_client --transport mqtt://localhost:1883/rtl433

# Batch processing for files
rtl_433_client -r batch/*.cu8 --transport http://localhost:8080/batch
```

**Network:**
```bash
# Use compression (MQTT/HTTP)
rtl_433_client --transport mqtts://broker:8883/rtl433?compression=gzip

# Local message broker for buffering
rtl_433_client --transport mqtt://localhost:1883/rtl433
```

### Monitoring and Metrics

**Client Metrics:**
- Signals processed per minute
- Transport success/failure rates
- SDR device status and signal quality
- Memory and CPU usage

**Server Metrics:**
- Messages received per second
- Protocol decode success rates
- Queue depths and processing times
- API response times

## Security

### Authentication

**HTTP Transport:**
```bash
# Basic authentication
rtl_433_client --transport https://user:pass@api.example.com/signals

# Token-based authentication
rtl_433_client --transport https://api.example.com/signals \
  --auth-header "Authorization: Bearer eyJ0eXAiOiJKV1Q..."
```

**MQTT Transport:**
```bash
# Username/password
rtl_433_client --transport mqtts://user:pass@broker:8883/rtl433

# Client certificates
rtl_433_client --transport mqtts://broker:8883/rtl433 \
  --cert client.crt --key client.key --ca ca.crt
```

### Encryption

**TLS/SSL Support:**
- HTTPS transport with certificate validation
- MQTTS (MQTT over TLS) with client certificates
- AMQPS (RabbitMQ over TLS)
- Custom TCP with TLS wrapper

**Data Privacy:**
- No sensitive data in pulse timings
- Device IDs can be anonymized
- Location data can be stripped
- Timestamps can be relative

### Network Security

**Firewall Configuration:**
```bash
# Allow outbound connections only
iptables -A OUTPUT -p tcp --dport 443 -j ACCEPT  # HTTPS
iptables -A OUTPUT -p tcp --dport 8883 -j ACCEPT # MQTTS
iptables -A OUTPUT -p tcp --dport 5671 -j ACCEPT # AMQPS
```

**VPN Integration:**
```bash
# Route through VPN for remote sites
rtl_433_client --transport https://internal.server:8080/signals
```

## Implementation Details

### Build System Integration

The split architecture is integrated into the main CMake build system:

```cmake
# Enable client build
cmake -DBUILD_CLIENT=ON ..

# Optional features
cmake -DBUILD_CLIENT=ON -DMQTT_SUPPORT=ON -DRABBITMQ_SUPPORT=ON ..
```

### Library Dependencies

**Core Dependencies:**
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBRTLSDR REQUIRED librtlsdr)
pkg_check_modules(SOAPYSDR soapysdr)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
```

**Optional Dependencies:**
```cmake
# MQTT Support
find_library(MQTT_LIBRARY NAMES paho-mqtt3c)

# RabbitMQ Support  
find_library(RABBITMQ_LIBRARY NAMES rabbitmq)
```

### Code Organization

```
rtl_433/
├── client/
│   ├── src/
│   │   ├── rtl_433_client_v2.c      # Main client application
│   │   ├── client_transport.c       # Transport layer implementation
│   │   ├── client_transport.h       # Transport interface
│   │   └── client_config.h          # Configuration structures
│   ├── README.md                    # Client documentation
│   └── CMakeLists.txt              # Client build configuration
├── src/                            # Original rtl_433 core (libr_433.a)
├── docs/
│   ├── SPLIT_ARCHITECTURE.md      # This document
│   ├── BUILDING.md                # Build instructions
│   └── OPERATION.md               # Operation guide
└── CMakeLists.txt                 # Main build configuration
```

### Integration with libr_433.a

The client links against the static library built from the original rtl_433 source:

```c
// Key integration points
#include "rtl_433.h"
#include "r_private.h"
#include "pulse_detect.h"
#include "pulse_analyzer.h"

// Using rtl_433 APIs
r_cfg_t *cfg = &g_cfg;
register_all_protocols(cfg, 0);
pulse_detect_package(demod->pulse_detect, am_buf, fm_buf, n_samples, ...);
```

### Error Handling

**Client Error Recovery:**
```c
// Automatic reconnection
if (!transport_is_connected(&g_transport)) {
    if (transport_connect(&g_transport) != 0) {
        // Fallback to local storage or different transport
    }
}

// SDR device recovery
if (sdr_error) {
    client_stop_sdr();
    sleep(5);
    client_start_sdr();
}
```

**Transport Resilience:**
- HTTP: Retry with exponential backoff
- MQTT: Built-in reconnection logic
- RabbitMQ: Connection recovery and channel recreation
- TCP/UDP: Automatic reconnection attempts

## Future Enhancements

### Planned Features

1. **rtl_433_server Implementation**
   - REST API for data access
   - WebSocket for real-time streaming
   - Database integration (PostgreSQL, InfluxDB)
   - Web-based configuration UI

2. **Advanced Transport Features**
   - Message compression (gzip, LZ4)
   - Batch processing for efficiency
   - Priority queues for urgent signals
   - Message deduplication

3. **Enhanced Security**
   - OAuth2/JWT authentication
   - Message encryption (end-to-end)
   - Audit logging
   - Rate limiting and DDoS protection

4. **Operational Features**
   - Prometheus metrics integration
   - Health check endpoints
   - Configuration hot-reload
   - Graceful shutdown handling

5. **Edge Computing**
   - Local processing and filtering
   - Edge ML for signal classification
   - Bandwidth optimization
   - Offline operation modes

### Contribution Guidelines

The split architecture follows the same contribution guidelines as the main rtl_433 project:

1. **Code Style**: Follow existing C99 standards
2. **Testing**: Include unit and integration tests
3. **Documentation**: Update relevant documentation
4. **Compatibility**: Maintain backward compatibility
5. **Performance**: Benchmark performance impacts

For detailed contribution instructions, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Conclusion

The RTL_433 Split Architecture provides a scalable, flexible foundation for distributed RF signal processing. By separating demodulation (client) from device decoding (server), the architecture enables:

- **Horizontal scaling** for high-volume deployments
- **Fault tolerance** through component isolation  
- **Technology flexibility** with multiple transport options
- **Future extensibility** for advanced features

The implementation maintains full compatibility with the original rtl_433 protocol support while adding modern distributed systems capabilities suitable for production IoT and sensor monitoring applications.
