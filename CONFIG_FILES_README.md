# RTL_433 Configuration Files Guide

This directory contains pre-configured configuration files for RTL_433 with all available protocols enabled.

## 📋 **Available Configuration Files**

### 🌟 **rtl_433_all_protocols.conf**
**Complete configuration with ALL 281+ device protocols enabled**
- Includes every available protocol from `src/devices/`
- Comprehensive coverage for maximum device compatibility
- May have higher false positive rates due to protocol conflicts
- Best for: Testing, development, maximum coverage scenarios

**Usage:**
```bash
rtl_433 -c rtl_433_all_protocols.conf
```

### 🎯 **rtl_433_main_protocols.conf** 
**Curated selection of most common and stable protocols**
- Focused on weather stations, TPMS, security systems, remotes
- Reduced false positives while covering majority of use cases
- Organized by device categories for easy customization
- Best for: Production use, stable operation

**Usage:**
```bash
rtl_433 -c rtl_433_main_protocols.conf
```

### 🖥️ **src/rtl_433_client_all_protocols.conf**
**Client-specific configuration for split architecture**
- Designed for `rtl_433_client` in client-server mode
- Includes transport configuration (RabbitMQ, HTTP, MQTT, TCP, UDP)
- Signal processing focused (device detection on server)
- Best for: Split architecture deployments

**Usage:**
```bash
./rtl_433_client -c src/rtl_433_client_all_protocols.conf
```

## 🚀 **Quick Start**

### For Standard RTL_433:
```bash
# Use all protocols (comprehensive)
rtl_433 -c rtl_433_all_protocols.conf

# Use main protocols only (recommended for production)
rtl_433 -c rtl_433_main_protocols.conf

# Use existing example config
rtl_433 -c conf/rtl_433.example.conf
```

### For Split Architecture:
```bash
# Start client
./rtl_433_client -c src/rtl_433_client_all_protocols.conf

# Start server (auto-enables all protocols)
./rtl_433_server
```

## 📊 **Protocol Categories**

### **Weather Stations (Major Brands)**
- Oregon Scientific (protocol 12, 50, 54)
- Acurite (protocols 10, 11, 40, 41, 55, 74, 163, 197)
- Bresser (protocols 52, 119, 172, 173, 247, 249, 259, 268)
- Fine Offset/Ecowitt (protocols 18, 32, 69, 78, 79, 88, 142, 152, 190, 200, 213, 219, 221, 244, 251, 262, 263)
- LaCrosse (protocols 8, 34, 73, 75, 76, 83, 124, 166, 170, 171, 175, 184, 206, 216, 240)

### **TPMS (Tire Pressure Monitoring)**
- **European**: Citroen (82), Renault (90, 212), BMW (252, 257), Porsche (203)
- **Asian**: Toyota (88), Nissan (248), Hyundai (140, 186), Kia (226)
- **American**: Ford (89), GM/Chevrolet (275)
- **Universal**: Schrader (60, 95, 168), Jansite (123, 180), AVE (208), TyreGuard (225)

### **Security Systems**
- SimpliSafe (102, 209)
- DSC Security (23, 148)
- Visonic (151)
- Honeywell (70, 115, 116, 185)
- Interlogix (100)

### **Remote Controls**
- Generic (30, 87)
- Silvercrest (1)
- Nexa/KlikAanKlikUit (15, 51, 96)
- Somfy (167, 189)
- LightwaveRF (61)

### **Energy Monitors**
- CurrentCost (44)
- IKEA Sparsnas (130)
- ESA Energy (117)
- BlueLine PowerCost (176)

## ⚙️ **Customization**

### **Enable Specific Categories Only:**
```bash
# Copy a base config
cp rtl_433_main_protocols.conf my_custom.conf

# Edit to enable only weather stations
# Comment out other sections with #
```

### **Add Custom Protocols:**
```bash
# Add at end of config file
protocol 999  # My custom device
```

### **Disable Problematic Protocols:**
```bash
# Comment out conflicting protocols
# protocol 123  # Causes false positives
```

## 🔧 **Transport Configuration**

### **RabbitMQ (Split Architecture)**
```ini
output rabbitmq://localhost:5672/signals,user=rtl433,pass=rtl433
```

### **MQTT**
```ini
output mqtt://localhost:1883/rtl433/signals
```

### **HTTP**
```ini
output http://localhost:8080/api/signals
```

### **File Output**
```ini
output json:/tmp/rtl433.json
output csv:/tmp/rtl433.csv
```

## 📈 **Performance Tuning**

### **High Traffic Scenarios:**
```ini
# Increase buffer sizes
out_block_size 262144
buffer_size 1048576

# Batch processing
batch_mode true
batch_size 20
batch_timeout_ms 500
```

### **Low Resource Systems:**
```ini
# Reduce protocols to main ones only
# Use rtl_433_main_protocols.conf

# Lower sample rate
sample_rate 200k

# Increase detection thresholds
min_snr 9.0
level_limit -10.0
```

## 🐛 **Debugging**

### **Enable Verbose Logging:**
```ini
verbosity 3
pulse_detect verbose
analyze
```

### **Signal Analysis:**
```ini
# Save raw signals for analysis
output raw:/tmp/signals.raw

# Statistics reporting
stats 60
report_meta time:iso
report_meta level
report_meta stats
```

## 📁 **File Structure**

```
rtl_433-1/
├── rtl_433_all_protocols.conf      # All 281+ protocols
├── rtl_433_main_protocols.conf     # Curated main protocols  
├── CONFIG_FILES_README.md          # This file
├── conf/
│   ├── rtl_433.example.conf        # Official example
│   └── *.conf                      # Device-specific configs
├── client/config/
│   └── client.conf.example         # Client example
├── server/config/  
│   └── server.conf.example         # Server example
└── src/
    └── rtl_433_client_all_protocols.conf  # Client-specific
```

## 💡 **Best Practices**

### **Production Deployment:**
1. Start with `rtl_433_main_protocols.conf`
2. Monitor for missed devices
3. Add specific protocols as needed
4. Avoid enabling all protocols unless necessary

### **Development/Testing:**
1. Use `rtl_433_all_protocols.conf` for comprehensive coverage
2. Enable verbose logging and analysis
3. Save raw signals for replay testing

### **Split Architecture:**
1. Client: Use `src/rtl_433_client_all_protocols.conf`
2. Server: Auto-enables all protocols by default
3. Configure appropriate transport (RabbitMQ recommended)

## 🚨 **Troubleshooting**

### **High False Positives:**
- Switch from all protocols to main protocols config
- Increase detection thresholds (`min_snr`, `level_limit`)
- Disable specific conflicting protocols

### **Missing Devices:**
- Enable more protocols from all protocols config
- Check frequency settings match device
- Verify gain and sensitivity settings

### **Performance Issues:**
- Reduce number of enabled protocols
- Increase buffer sizes
- Lower sample rate if acceptable

## 📞 **Support**

For issues with specific protocols or configuration:
1. Check official RTL_433 documentation
2. Review protocol-specific `.conf` files in `conf/` directory
3. Test with minimal configuration first
4. Enable debugging and analyze signal patterns

---

**Generated for RTL_433 Split Architecture Project**  
**All protocols based on rtl_433_devices.h DEVICES macro**  
**Total Protocols: 281+ (as of latest build)**
