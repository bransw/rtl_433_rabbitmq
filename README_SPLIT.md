# RTL_433 Split Architecture

This is a client-server split architecture version of rtl_433 that separates signal demodulation and device decoding processes.

## Architecture

### rtl_433_client
The client is responsible for:
- Receiving data from RTL-SDR device or file
- Signal demodulation (OOK/FSK)
- Pulse detection
- Sending demodulated data to server via various transports

### rtl_433_server
The server is responsible for:
- Receiving demodulated signals from clients
- Device decoding using all available decoders
- Placing results in queues:
  - `ready` - for recognized devices
  - `unknown` - for unknown signals

## Installation

### Dependencies

#### For client:
- librtlsdr (for RTL-SDR operation)
- libcurl (for HTTP transport)
- json-c (for data serialization)
- paho-mqtt3c (optional, for MQTT)
- librabbitmq (optional, for RabbitMQ)

#### For server:
- json-c (for data parsing)
- sqlite3 (for unknown signals storage)
- paho-mqtt3c (optional, for MQTT)
- librabbitmq (optional, for RabbitMQ)
- pthread (for multithreading)

### Building on WSL Ubuntu 22.04

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake pkg-config git \
    librtlsdr-dev libcurl4-openssl-dev libjson-c-dev \
    libsqlite3-dev libpaho-mqtt-dev

# Optional: RabbitMQ support
sudo apt install -y librabbitmq-dev

# Build split architecture (integrated into main CMakeLists.txt)
mkdir build
cd build

# Configure build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_CLIENT=ON \
      -DBUILD_SERVER=ON \
      -DENABLE_MQTT=ON \
      -DENABLE_RABBITMQ=OFF \
      ..

# Build
make -j$(nproc)

# Or use the convenience script
../build_all.sh

# Install
sudo make install
```

### Build Options

- `BUILD_CLIENT=ON/OFF` - build client
- `BUILD_SERVER=ON/OFF` - build server  
- `BUILD_ORIGINAL=ON/OFF` - build original rtl_433
- `ENABLE_MQTT=ON/OFF` - MQTT transport support
- `ENABLE_RABBITMQ=ON/OFF` - RabbitMQ transport support

## Usage

### Quick Start

1. **Start server:**
```bash
# HTTP server on port 8080
rtl_433_server -H 0.0.0.0:8080 -R ready_queue.json -U unknown_queue.json

# MQTT server
rtl_433_server -M mqtt://localhost:1883 -t rtl433/ready -u rtl433/unknown
```

2. **Start client:**
```bash
# HTTP client
rtl_433_client -d 0 -f 433.92M -T http://server:8080/signals

# MQTT client  
rtl_433_client -d 0 -f 433.92M -T mqtt://server:1883 -t rtl433/signals
```

### Client Configuration

#### Command Line
```bash
rtl_433_client [options]

SDR Options:
  -d <device>     RTL-SDR device (default: 0)
  -f <freq>       Frequency in Hz (default: 433920000)
  -s <rate>       Sample rate (default: 250000)
  -g <gain>       Gain (default: auto)
  -p <ppm>        Frequency correction in ppm (default: 0)

Demodulation Options:
  -Y <mode>       FSK detector mode (auto/classic/minmax)
  -A              Enable pulse analyzer
  -l <level>      Detection level (-1.0 to -30.0 dB)
  
Transport Options:
  -T <url>        Server URL (http://host:port/path)
  -M <url>        MQTT broker (mqtt://host:port)
  -R <url>        RabbitMQ server (amqp://host:port)
  -t <topic>      MQTT topic or RabbitMQ queue
  -u <user>       Username
  -P <pass>       Password

Mode Options:
  -r <file>       Read from file instead of SDR
  -T <sec>        Run time in seconds
  -v              Increase verbosity
  -B <size>       Signal batch size
  -b <timeout>    Batch timeout in ms
```

#### Configuration File
```ini
# client.conf
[sdr]
device = 0
frequency = 433920000
sample_rate = 250000
gain = auto
ppm_error = 0

[demodulation]
ook_mode = auto
fsk_mode = auto
level_limit = -12.0
min_level = -20.0
analyze_pulses = false

[transport]
type = http
host = localhost
port = 8080
path = /api/signals
username = 
password = 
ssl = false

[logging]
verbosity = 1
log_file = client.log

[operation]
duration = 0
batch_mode = true
batch_size = 10
batch_timeout_ms = 1000
```

### Server Configuration

#### Command Line
```bash
rtl_433_server [options]

Input Transports:
  -H <bind>       HTTP server (host:port)
  -M <url>        MQTT broker (mqtt://host:port)
  -R <url>        RabbitMQ server (amqp://host:port)
  -t <topic>      MQTT topic or RabbitMQ queue for input data

Output Queues:
  -r <config>     Ready devices queue configuration
  -u <config>     Unknown signals queue configuration
  
Decoders:
  -D <file>       Decoder configuration file
  -E <id>         Enable decoder by ID
  -X <id>         Exclude decoder by ID

Performance Options:
  -j <threads>    Number of worker threads
  -q <size>       Queue buffer sizes
  -b <size>       Batch size for processing

Misc:
  -c <file>       Configuration file
  -v              Increase verbosity
  -d              Daemon mode
  -p <file>       PID file
```

#### Server Configuration File
```ini
# server.conf
[input_transports]
http_enabled = true
http_host = 0.0.0.0
http_port = 8080
http_max_connections = 100

mqtt_enabled = false
mqtt_host = localhost
mqtt_port = 1883
mqtt_topic = rtl433/signals
mqtt_username = 
mqtt_password = 

[output_queues]
[output_queues.ready]
type = file
path = /var/log/rtl433/ready.jsonl
batch_size = 100
batch_timeout_ms = 5000

[output_queues.unknown]
type = database
db_path = /var/lib/rtl433/unknown_signals.db
retention_days = 30
max_size_mb = 1024

[decoders]
# Enable all decoders by default
default_enabled = true

# Exclusions
disabled_decoders = [101, 106, 151]

# Special settings for specific decoders
[decoders.176]  # BlueLine
enabled = true
parameters = "auto"

[processing]
max_concurrent_signals = 50
signal_timeout_ms = 1000
worker_threads = 4
io_threads = 2

[logging]
verbosity = 2
log_file = /var/log/rtl433/server.log
log_rotation = true
log_max_size_mb = 100
log_max_files = 5

[monitoring]
stats_enabled = true
stats_interval_sec = 60
web_enabled = true
web_port = 8081

[daemon]
daemon_mode = false
pid_file = /var/run/rtl433_server.pid
user = rtl433
group = rtl433
```

## Transports

### HTTP
- **Client**: Sends POST requests with JSON data
- **Server**: HTTP API for receiving signals
- **Format**: JSON with demodulated data

### MQTT
- **Client**: Publishes messages to specified topic
- **Server**: Subscribes to topic for receiving signals  
- **Format**: JSON messages with QoS 1

### RabbitMQ (optional)
- **Client**: Sends messages to queue
- **Server**: Consumes messages from queue
- **Format**: JSON messages with delivery confirmation

### TCP/UDP (planned)
- Direct network connection
- Binary or JSON protocol

## Data Formats

### Demodulated Signal (client → server)
```json
{
  "device_id": "rtl_433_client_001",
  "timestamp_us": 1640995200000000,
  "frequency": 433920000,
  "sample_rate": 250000,
  "rssi_db": -15.2,
  "snr_db": 12.8,
  "noise_db": -28.0,
  "ook_detected": true,
  "fsk_detected": false,
  "pulse_data": {
    "num_pulses": 64,
    "pulses": [500, 1000, 500, 2000, ...],
    "gaps": [500, 500, 1500, 500, ...]
  },
  "raw_data_hex": "a5a5a5..."
}
```

### Decoded Device (server → ready queue)
```json
{
  "source_id": "rtl_433_client_001", 
  "timestamp_us": 1640995200000000,
  "device": {
    "name": "Acurite 5n1 Weather Station",
    "id": 40,
    "confidence": 0.95,
    "data": {
      "model": "Acurite-5n1",
      "id": 3216,
      "channel": "A",
      "battery_ok": 1,
      "temperature_C": 23.4,
      "humidity": 45,
      "wind_avg_m_s": 2.1,
      "wind_dir_deg": 180,
      "rain_mm": 0.0
    }
  },
  "signal_info": {
    "frequency": 433920000,
    "rssi_db": -15.2,
    "snr_db": 12.8
  }
}
```

### Unknown Signal (server → unknown queue)  
```json
{
  "source_id": "rtl_433_client_001",
  "timestamp_us": 1640995200000000,
  "signal_strength": -18.5,
  "analysis": {
    "pulse_count": 32,
    "estimated_bitrate": 1000,
    "modulation": "OOK",
    "pattern_analysis": "repeating_pattern_detected"
  },
  "raw_data": {
    "pulse_data": {...},
    "hex": "deadbeef..."
  }
}
```

## Monitoring and Debugging

### Logs
- Client: detailed demodulation and transmission logs
- Server: decoding and queue statistics logs

### Web Interface
- Real-time statistics
- Active clients list  
- Unknown signals viewer
- Decoder configuration

### Metrics
- Number of processed signals
- Successful decoding percentage
- Queue performance
- Connection status

## Deployment

### WSL Ubuntu 22.04 Setup
```bash
# Install WSL Ubuntu 22.04
wsl --install -d Ubuntu-22.04

# Update system
sudo apt update && sudo apt upgrade -y

# Install build dependencies
sudo apt install -y build-essential cmake pkg-config git \
    librtlsdr-dev libcurl4-openssl-dev libjson-c-dev \
    libsqlite3-dev libpaho-mqtt-dev

# Install USB support for RTL-SDR
sudo apt install -y usbutils

# Add user to dialout group for USB access
sudo usermod -a -G dialout $USER

# Logout and login again to apply group changes
```

### Docker
```yaml
# docker-compose.yml
version: '3.8'
services:
  rtl433-server:
    image: rtl433/server:latest
    ports:
      - "8080:8080"
      - "8081:8081"
    volumes:
      - ./server.conf:/etc/rtl433/server.conf
      - rtl433-data:/var/lib/rtl433
    environment:
      - RTL433_CONFIG=/etc/rtl433/server.conf

  rtl433-client:
    image: rtl433/client:latest
    privileged: true
    volumes:
      - /dev/bus/usb:/dev/bus/usb
      - ./client.conf:/etc/rtl433/client.conf
    environment:
      - RTL433_SERVER=http://rtl433-server:8080
    depends_on:
      - rtl433-server

volumes:
  rtl433-data:
```

### Systemd Services
```ini
# /etc/systemd/system/rtl433-server.service
[Unit]
Description=RTL433 Server
After=network.target

[Service]
Type=forking
User=rtl433
Group=rtl433
ExecStart=/usr/bin/rtl_433_server -c /etc/rtl433/server.conf -d
PIDFile=/var/run/rtl433_server.pid
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

## Usage Examples

### Distributed Sensor Network
```bash
# Central server
rtl_433_server -H 0.0.0.0:8080 -r mqtt://broker:1883/ready -u file://unknown.jsonl

# Clients in different locations
rtl_433_client -d 0 -f 433.92M -T http://central-server:8080 -i location_1
rtl_433_client -d 1 -f 868.3M -T http://central-server:8080 -i location_2
```

### Unknown Protocol Analysis
```bash
# Server with detailed analysis
rtl_433_server -A -u database:///var/lib/rtl433/analysis.db

# Client with maximum sensitivity  
rtl_433_client -l -30 -Y minmax -A -T http://server:8080
```

### Home Assistant Integration
```bash
# Server sends to MQTT for Home Assistant
rtl_433_server -r mqtt://ha-broker:1883/homeassistant/sensor/rtl433

# Client collects data from multiple frequencies
rtl_433_client -f 433.92M -f 868.3M -f 915M -H 60 -T http://server:8080
```

## Troubleshooting

### Common Issues

1. **Client cannot connect to server**
   - Check network connectivity
   - Verify server is running
   - Check URL and ports

2. **Server not decoding signals**
   - Verify needed decoders are enabled
   - Check client demodulation is working
   - Verify signal level and quality

3. **High server load**
   - Increase worker threads
   - Configure batch processing
   - Optimize active decoder list

### Debugging

```bash
# Verbose client logs
rtl_433_client -vvv -T http://server:8080

# Verbose server logs  
rtl_433_server -vvv -H 0.0.0.0:8080

# Network analysis
tcpdump -i any -s 0 -w rtl433.pcap port 8080
```

## License

This project inherits the license from original rtl_433 - GNU General Public License v2.0.

## Contributing

Pull requests and issues are welcome! Particularly needed:
- New transports (WebSocket, gRPC)
- Performance improvements
- Platform testing
- Documentation and examples