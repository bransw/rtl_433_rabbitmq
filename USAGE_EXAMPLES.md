# RTL_433 Split Architecture Usage Examples

## Quick Start

### 1. Simple HTTP Configuration

**Start server:**
```bash
# Create directories
sudo mkdir -p /var/log/rtl433 /var/lib/rtl433

# Start server on port 8080
rtl_433_server \
    --http 0.0.0.0:8080 \
    --ready-file /var/log/rtl433/devices.jsonl \
    --unknown-db /var/lib/rtl433/unknown.db \
    --verbosity 2
```

**Start client:**
```bash
# Start client with RTL-SDR device
rtl_433_client \
    --device 0 \
    --frequency 433920000 \
    --transport http://localhost:8080/api/signals \
    --verbosity 2
```

### 2. MQTT Configuration

**Server:**
```bash
rtl_433_server \
    --mqtt mqtt://localhost:1883 \
    --mqtt-topic rtl433/signals \
    --ready-mqtt mqtt://localhost:1883/devices/ready \
    --unknown-mqtt mqtt://localhost:1883/devices/unknown \
    --verbosity 2
```

**Client:**
```bash
rtl_433_client \
    --device 0 \
    --frequency 433920000 \
    --transport mqtt://localhost:1883 \
    --topic rtl433/signals \
    --verbosity 2
```

## Advanced Scenarios

### 3. Multi-frequency Client

```bash
# Client switching between frequencies
rtl_433_client \
    --device 0 \
    --frequency 433920000 \
    --frequency 868300000 \
    --frequency 915000000 \
    --hop-time 60 \
    --transport http://server:8080/api/signals \
    --batch-mode \
    --batch-size 5
```

### 4. High-performance Server

```bash
rtl_433_server \
    --config /etc/rtl433/server.conf \
    --worker-threads 8 \
    --io-threads 4 \
    --max-concurrent 200 \
    --daemon \
    --pid-file /var/run/rtl433_server.pid
```

### 5. Unknown Protocol Analysis

**Analysis mode server:**
```bash
rtl_433_server \
    --http 0.0.0.0:8080 \
    --disable-all-decoders \
    --unknown-file /var/log/rtl433/analysis.jsonl \
    --web-interface 0.0.0.0:8081 \
    --stats-interval 30
```

**High sensitivity client:**
```bash
rtl_433_client \
    --device 0 \
    --frequency 433920000 \
    --level-limit -30 \
    --min-level -35 \
    --analyze-pulses \
    --transport http://server:8080/api/signals \
    --raw-mode
```

### 6. Distributed Sensor Network

**Central server:**
```bash
rtl_433_server \
    --http 0.0.0.0:8080 \
    --mqtt mqtt://broker:1883 \
    --ready-topic sensors/rtl433/devices \
    --unknown-db /var/lib/rtl433/unknown.db \
    --web-interface 0.0.0.0:8081 \
    --stats-enabled \
    --daemon
```

**Remote clients:**
```bash
# Location 1
rtl_433_client \
    --device 0 \
    --frequency 433920000 \
    --transport http://central-server:8080/api/signals \
    --device-id location_1 \
    --duration 0

# Location 2  
rtl_433_client \
    --device 1 \
    --frequency 868300000 \
    --transport http://central-server:8080/api/signals \
    --device-id location_2 \
    --duration 0
```

### 7. Home Assistant Integration

**Server for Home Assistant:**
```bash
rtl_433_server \
    --http 0.0.0.0:8080 \
    --ready-mqtt mqtt://homeassistant:1883/homeassistant/sensor/rtl433 \
    --mqtt-username hass_user \
    --mqtt-password hass_pass \
    --enable-decoder 40,12,18,78 \
    --verbosity 1
```

**Home Assistant configuration.yaml:**
```yaml
mqtt:
  sensor:
    - name: "RTL433 Devices"
      state_topic: "homeassistant/sensor/rtl433"
      value_template: "{{ value_json.device.data.model }}"
      json_attributes_topic: "homeassistant/sensor/rtl433"
      json_attributes_template: "{{ value_json.device.data | tojson }}"
```

### 8. File Analysis

**Analyze recorded files:**
```bash
# Client reads from file
rtl_433_client \
    --input-file /path/to/recording.cu8 \
    --sample-rate 250000 \
    --frequency 433920000 \
    --transport http://localhost:8080/api/signals \
    --analyze-pulses

# Server with detailed logging
rtl_433_server \
    --http 0.0.0.0:8080 \
    --ready-file /tmp/decoded.jsonl \
    --unknown-file /tmp/unknown.jsonl \
    --verbosity 3
```

### 9. Monitoring and Debugging

**Server with full monitoring:**
```bash
rtl_433_server \
    --config /etc/rtl433/server.conf \
    --web-interface 0.0.0.0:8081 \
    --stats-enabled \
    --stats-interval 60 \
    --stats-file /var/log/rtl433/stats.json \
    --log-file /var/log/rtl433/server.log \
    --log-rotation \
    --verbosity 2
```

**Client with debug output:**
```bash
rtl_433_client \
    --device 0 \
    --frequency 433920000 \
    --transport http://server:8080/api/signals \
    --log-file /var/log/rtl433/client.log \
    --verbosity 3 \
    --analyze-pulses
```

### 10. Docker Deployment

**docker-compose.yml:**
```yaml
version: '3.8'

services:
  rtl433-server:
    image: rtl433/server:latest
    container_name: rtl433-server
    ports:
      - "8080:8080"
      - "8081:8081"
    volumes:
      - ./server.conf:/etc/rtl433/server.conf:ro
      - rtl433-data:/var/lib/rtl433
      - rtl433-logs:/var/log/rtl433
    environment:
      - RTL433_VERBOSITY=2
    restart: unless-stopped

  rtl433-client:
    image: rtl433/client:latest  
    container_name: rtl433-client
    privileged: true
    devices:
      - "/dev/bus/usb:/dev/bus/usb"
    volumes:
      - ./client.conf:/etc/rtl433/client.conf:ro
    environment:
      - RTL433_SERVER_URL=http://rtl433-server:8080/api/signals
      - RTL433_DEVICE=0
      - RTL433_FREQUENCY=433920000
    depends_on:
      - rtl433-server
    restart: unless-stopped

  mqtt-broker:
    image: eclipse-mosquitto:latest
    container_name: mqtt-broker
    ports:
      - "1883:1883"
      - "9001:9001"
    volumes:
      - ./mosquitto.conf:/mosquitto/config/mosquitto.conf:ro
    restart: unless-stopped

volumes:
  rtl433-data:
  rtl433-logs:
```

**Launch:**
```bash
docker-compose up -d
```

## Configuration Files

### Client (client.conf)
```ini
[sdr]
device = 0
frequency = 433920000
sample_rate = 250000
gain = auto

[transport]
type = http
host = localhost
port = 8080
topic_queue = /api/signals

[operation]
batch_mode = true
batch_size = 10
duration = 0

[logging]
verbosity = 2
```

### Server (server.conf)
```ini
[input_transports.http]
enabled = true
host = 0.0.0.0
port = 8080

[output_queues.ready]
type = file
file_path = /var/log/rtl433/devices.jsonl

[output_queues.unknown]
type = database
db_path = /var/lib/rtl433/unknown.db

[decoders]
default_enabled = true
disabled_decoders = [101, 106, 151]

[processing]
worker_threads = 4
max_concurrent_signals = 100

[logging]
verbosity = 2
log_file = /var/log/rtl433/server.log
```

## System Services

### Systemd service for server
```ini
# /etc/systemd/system/rtl433-server.service
[Unit]
Description=RTL433 Signal Decoding Server
After=network.target
Wants=network.target

[Service]
Type=forking
User=rtl433
Group=rtl433
ExecStart=/usr/local/bin/rtl_433_server --config /etc/rtl433/server.conf --daemon
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/var/run/rtl433_server.pid
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

### Systemd service for client
```ini
# /etc/systemd/system/rtl433-client.service  
[Unit]
Description=RTL433 Signal Collection Client
After=network.target rtl433-server.service
Wants=network.target
Requires=rtl433-server.service

[Service]
Type=simple
User=rtl433
Group=rtl433
ExecStart=/usr/local/bin/rtl_433_client --config /etc/rtl433/client.conf
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**Install and start:**
```bash
sudo systemctl enable rtl433-server rtl433-client
sudo systemctl start rtl433-server rtl433-client
sudo systemctl status rtl433-server rtl433-client
```

## Monitoring and Logs

### View logs
```bash
# Server logs
tail -f /var/log/rtl433/server.log

# Client logs
tail -f /var/log/rtl433/client.log

# Systemd logs
journalctl -u rtl433-server -f
journalctl -u rtl433-client -f
```

### Web interface
- Open http://server:8081 for monitoring
- Real-time statistics
- Active clients list
- Unknown signals viewer

### REST API
```bash
# Server statistics
curl http://server:8081/api/stats

# Active clients list
curl http://server:8081/api/clients

# Recent decoded devices
curl http://server:8081/api/devices/recent

# Unknown signals
curl http://server:8081/api/unknown/recent
```

## Troubleshooting

### Connection issues
```bash
# Check server availability
curl -v http://server:8080/health

# Check ports
netstat -an | grep 8080

# Check DNS
nslookup server
```

### RTL-SDR issues
```bash
# Check device
rtl_test -t

# List devices
rtl_433_client --device help

# Test with minimal settings
rtl_433_client --device 0 --frequency 433920000 --transport http://localhost:8080/test
```

### Performance monitoring
```bash
# Resource monitoring
top -p $(pgrep rtl_433)
htop -p $(pgrep rtl_433)

# Network statistics
iftop -i eth0
nethogs

# Disk activity
iotop -p $(pgrep rtl_433)
```

## WSL Ubuntu 22.04 Specific

### USB device access
```bash
# Install usbipd on Windows (run in PowerShell as Administrator)
winget install usbipd

# List USB devices (Windows PowerShell)
usbipd wsl list

# Attach RTL-SDR device (Windows PowerShell)
usbipd wsl attach --busid <busid>

# Verify in WSL
lsusb | grep RTL
```

### Performance optimization
```bash
# Increase USB buffer size
echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="2838", TAG+="uaccess", RUN+="/bin/sh -c '\''echo 1024 > /sys/bus/usb/devices/$kernel/bConfigurationValue'\''"' | sudo tee /etc/udev/rules.d/20-rtlsdr.rules

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### WSL configuration optimization
```bash
# Edit .wslconfig in Windows user home directory
# C:\Users\<username>\.wslconfig
[wsl2]
memory=4GB
processors=4
swap=2GB
```