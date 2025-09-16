/** @file
    Dynamic device loader for rtl_433 - allows runtime loading of device decoders.
    
    This system extends the existing flex decoder mechanism to support:
    1. Loading device definitions from configuration files
    2. Hot-reloading without restart
    3. Plugin-based architecture
    
    Copyright (C) 2025 RTL_433 Dynamic Loading Extension
*/

#ifndef INCLUDE_DYNAMIC_DEVICE_LOADER_H_
#define INCLUDE_DYNAMIC_DEVICE_LOADER_H_

#include "r_device.h"
#include "list.h"

// Plugin interface version
#define DEVICE_PLUGIN_VERSION 1

// Maximum number of dynamic devices
#define MAX_DYNAMIC_DEVICES 64

// Device definition structure for configuration files
typedef struct {
    char *name;
    char *modulation;
    char *description;
    char *flex_spec;        // Flex decoder specification string
    char *config_file;      // Source configuration file
    int enabled;
    int priority;
} device_definition_t;

// Dynamic device manager
typedef struct {
    list_t device_definitions;  // List of device_definition_t
    list_t loaded_devices;     // List of loaded r_device*
    char *devices_dir;         // Directory to scan for device files
    int auto_reload;           // Auto-reload on file changes
    time_t last_scan;          // Last directory scan time
} dynamic_device_manager_t;

// Plugin interface (for future shared library support)
typedef struct {
    int version;
    char *name;
    char *description;
    r_device *(*create_device)(char *config);
    void (*destroy_device)(r_device *device);
    int (*validate_config)(char *config);
} device_plugin_interface_t;

// Public API functions
dynamic_device_manager_t *dynamic_device_manager_create(void);
void dynamic_device_manager_destroy(dynamic_device_manager_t *manager);

// Device configuration loading
int load_device_definitions_from_file(dynamic_device_manager_t *manager, const char *filename);
int load_device_definitions_from_dir(dynamic_device_manager_t *manager, const char *dirname);
int reload_device_definitions(dynamic_device_manager_t *manager);

// Device registration
int register_dynamic_device(r_cfg_t *cfg, dynamic_device_manager_t *manager, const char *device_name);
int register_all_dynamic_devices(r_cfg_t *cfg, dynamic_device_manager_t *manager);
int unregister_dynamic_device(r_cfg_t *cfg, const char *device_name);

// Device management
device_definition_t *find_device_definition(dynamic_device_manager_t *manager, const char *name);
void list_dynamic_devices(dynamic_device_manager_t *manager);
int enable_dynamic_device(dynamic_device_manager_t *manager, const char *name);
int disable_dynamic_device(dynamic_device_manager_t *manager, const char *name);

// Hot reload support
int check_for_device_updates(dynamic_device_manager_t *manager);
void setup_file_monitoring(dynamic_device_manager_t *manager);

// Configuration file format support
int parse_device_config_json(const char *json_data, device_definition_t *device);
int parse_device_config_ini(const char *ini_data, device_definition_t *device);
char *serialize_device_config_json(device_definition_t *device);

// Plugin support (for future extension)
int load_device_plugin(dynamic_device_manager_t *manager, const char *plugin_path);
int unload_device_plugin(dynamic_device_manager_t *manager, const char *plugin_name);

#endif /* INCLUDE_DYNAMIC_DEVICE_LOADER_H_ */
