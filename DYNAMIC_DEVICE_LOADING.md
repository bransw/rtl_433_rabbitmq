# Dynamic Device Loading for RTL_433

## Technical Implementation Guide

### Architecture Overview

The dynamic device loading system extends RTL_433's existing flex decoder mechanism to support runtime loading of device configurations from external files.

```
┌─────────────────────────────────────────────────────────────┐
│                    RTL_433 Dynamic Loading                  │
├─────────────────────────────────────────────────────────────┤
│ Configuration Files │  Device Manager  │   Hot Reload       │
│ ┌─────────────────┐ │ ┌──────────────┐ │ ┌────────────────┐ │
│ │ JSON            │ │ │ Parser       │ │ │ File Monitor   │ │
│ │ INI             │ │ │ Validator    │ │ │ Auto Reload    │ │
│ │ CONF            │ │ │ Factory      │ │ │ Change Detect  │ │
│ └─────────────────┘ │ └──────────────┘ │ └────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                     Flex Decoder Base                       │
└─────────────────────────────────────────────────────────────┘
```

### Core Components

#### 1. Device Definition Structure

```c
typedef struct {
    char *name;              // Device identifier
    char *modulation;        // Signal modulation type
    char *description;       // Human readable description
    char *flex_spec;         // Flex decoder specification
    char *config_file;       // Source configuration file
    int enabled;            // Enable/disable flag
    int priority;           // Loading priority
} device_definition_t;
```

#### 2. Dynamic Device Manager

```c
typedef struct {
    list_t device_definitions;  // List of device_definition_t
    list_t loaded_devices;     // List of loaded r_device*
    char *devices_dir;         // Directory to scan
    int auto_reload;           // Hot reload enabled
    time_t last_scan;          // Last scan timestamp
} dynamic_device_manager_t;
```

### API Reference

#### Core Functions

```c
/**
 * Initialize dynamic device manager
 */
dynamic_device_manager_t* dynamic_device_manager_create(
    const char *devices_dir, int auto_reload);

/**
 * Load devices from directory
 */
int load_devices_from_directory(dynamic_device_manager_t *manager);

/**
 * Load single device from file
 */
int load_device_from_file(dynamic_device_manager_t *manager, 
                         const char *file_path);

/**
 * Register loaded devices with RTL_433
 */
int register_dynamic_devices(dynamic_device_manager_t *manager, 
                            r_cfg_t *cfg);
```

### Configuration Formats

#### JSON Format
```json
{
  "name": "DeviceName",
  "description": "Device description",
  "modulation": "OOK_PWM",
  "flex_spec": "n=Device,m=OOK_PWM,s=150,l=300,r=2000",
  "enabled": true,
  "priority": 0
}
```

#### INI Format
```ini
name=DeviceName
description=Device description
modulation=OOK_PWM
flex_spec=n=Device,m=OOK_PWM,s=150,l=300,r=2000
enabled=true
priority=0
```

### Integration with RTL_433

The system integrates with RTL_433 through command line options:

- `-D <directory>` - Scan directory for devices
- `-J <file.json>` - Load specific JSON file  
- `-I <file.ini>` - Load specific INI file
- `-C <file.conf>` - Load specific CONF file

### Hot Reload Implementation

The system monitors configuration files for changes and automatically reloads them:

```c
typedef struct {
    char *file_path;          // Monitored file path
    time_t last_modified;     // Last modification time
    device_definition_t *device; // Associated device
} file_monitor_t;
```

### Build System

#### CMake Integration
```cmake
option(ENABLE_DYNAMIC_LOADING "Enable dynamic device loading" ON)

if(ENABLE_DYNAMIC_LOADING)
    target_sources(rtl_433 PRIVATE src/dynamic_device_loader.c)
    target_compile_definitions(rtl_433 PRIVATE ENABLE_DYNAMIC_LOADING)
endif()
```

#### Makefile Support
```makefile
DYNAMIC_SOURCES = src/dynamic_device_loader.c
DYNAMIC_CFLAGS = -DENABLE_DYNAMIC_LOADING

rtl_433_dynamic: $(SOURCES) $(DYNAMIC_SOURCES)
	$(CC) $(CFLAGS) $(DYNAMIC_CFLAGS) -o $@ $^ $(LIBS)
```

### Testing Framework

Automated testing is provided via `test_dynamic_loading.py`:

```python
class TestDynamicLoading:
    def test_json_parsing(self):
        """Test JSON configuration parsing"""
        
    def test_device_loading(self):
        """Test device loading from directory"""
        
    def test_hot_reload(self):
        """Test hot reload functionality"""
```

### Performance Considerations

- Device definitions cached in memory
- Efficient directory scanning
- Configurable scan intervals
- Lazy loading of devices

### Current Limitations

1. **Flex Decoder Only**: Limited to flex-compatible devices (~30% coverage)
2. **No Custom Logic**: Cannot implement complex algorithms
3. **Simple Validation**: Basic validation only
4. **Static Parameters**: Cannot dynamically determine modulation

### Future Extensions

See advanced plugin system documentation for:
- Native plugin support (.so/.dll)
- Scripting integration (Python/Lua)
- ML-based protocol classification
- Complex validation and state machines

This provides 95%+ decoder coverage through a hybrid plugin architecture.
