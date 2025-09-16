# 🔌 RTL_433 Dynamic Device Loading

## Overview

This system allows adding new device decoders to RTL_433 without recompilation. Simply create configuration files and RTL_433 will automatically load them at runtime.

## Quick Start

### 1. Create Device Configuration

Create a JSON file describing your device:

```json
{
  "name": "MyTemperatureSensor",
  "description": "Custom temperature sensor",
  "modulation": "OOK_PWM", 
  "flex_spec": "n=MyTemp,m=OOK_PWM,s=150,l=300,r=2000",
  "enabled": true
}
```

### 2. Load Dynamically

```bash
# Place config in examples/dynamic_devices/
cp my_device.json examples/dynamic_devices/

# Run RTL_433 with dynamic loading
./rtl_433_dynamic -D examples/dynamic_devices/

# Or specify individual file
./rtl_433_dynamic -J my_device.json
```

## Supported Formats

- **JSON** (.json) - Structured format with metadata
- **INI** (.ini) - Simple key=value format  
- **CONF** (.conf) - Configuration file format

## Configuration Options

### Required Fields
- `name` - Device name
- `modulation` - Signal modulation type
- `flex_spec` - Flex decoder specification

### Optional Fields
- `description` - Human readable description
- `enabled` - Enable/disable device (default: true)
- `priority` - Loading priority (default: 0)
- `metadata` - Additional device information

## Command Line Options

```bash
# Scan directory for devices
./rtl_433_dynamic -D <directory>

# Load specific JSON file
./rtl_433_dynamic -J <file.json>

# Load specific INI file  
./rtl_433_dynamic -I <file.ini>

# Load specific CONF file
./rtl_433_dynamic -C <file.conf>

# Enable hot reloading
./rtl_433_dynamic -D <directory> --hot-reload

# List loaded devices
./rtl_433_dynamic -D <directory> --list-devices
```

## Examples

See `examples/dynamic_devices/` for:
- `temperature_sensor.json` - JSON format example
- `door_sensor.ini` - INI format example  
- `weather_station.conf` - CONF format example

## Building

```bash
# Build with dynamic loading support
make -f Makefile.dynamic

# Or use standard build with dynamic support
mkdir build && cd build
cmake -DENABLE_DYNAMIC_PLUGINS=ON ..
make
```

## Testing

```bash
# Run automated tests
python3 test_dynamic_loading.py

# Test specific configuration
./rtl_433_dynamic -J examples/dynamic_devices/temperature_sensor.json -t
```

## Hot Reloading

The system supports hot reloading - modify configuration files and RTL_433 will automatically reload them:

```bash
# Start with hot reload enabled
./rtl_433_dynamic -D examples/dynamic_devices/ --hot-reload

# In another terminal, modify a config file
echo '{"name":"UpdatedSensor","modulation":"FSK_PCM","flex_spec":"n=New,m=FSK_PCM,s=52,l=52,r=800","enabled":true}' > examples/dynamic_devices/new_device.json

# RTL_433 automatically detects and loads the new device
```

## Architecture

The dynamic loading system extends RTL_433's existing flex decoder mechanism:

1. **Configuration Parser** - Reads JSON/INI/CONF files
2. **Device Factory** - Creates r_device structs from config
3. **Hot Reload Manager** - Monitors file changes
4. **Plugin Registry** - Manages loaded devices

## Compatibility

- Supports all existing RTL_433 modulation types
- Compatible with flex decoder syntax
- Backwards compatible with static device registration
- Cross-platform (Linux, Windows, macOS)

## Limitations

Current implementation supports ~30% of RTL_433 decoders (simple flex-compatible devices). For complex decoders requiring:

- Custom bit manipulation
- CRC validation
- Conditional logic
- State machines

See the advanced plugin system documentation for extended capabilities.

## Troubleshooting

### Device Not Loading
- Check JSON syntax with `python -m json.tool device.json`
- Verify flex_spec format matches RTL_433 flex syntax
- Enable verbose logging: `./rtl_433_dynamic -D dir/ -v`

### Hot Reload Not Working
- Ensure filesystem supports file notifications
- Check file permissions
- Verify `-hot-reload` flag is set

### Performance Issues
- Limit directory scanning frequency
- Use specific file loading instead of directory scanning
- Disable hot reload for production use

## Contributing

1. Add new configuration format parsers in `src/dynamic_device_loader.c`
2. Extend device validation in `validate_device_config()`
3. Add examples to `examples/dynamic_devices/`
4. Update tests in `test_dynamic_loading.py`

## License

Same as RTL_433 main project.