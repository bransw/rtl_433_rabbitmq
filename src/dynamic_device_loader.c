/** @file
    Dynamic device loader implementation for rtl_433.
    
    This implementation extends the existing flex decoder mechanism to support
    runtime loading of device decoders from configuration files.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#else
#include <windows.h>
#endif

#include "dynamic_device_loader.h"
#include "r_api.h"
#include "flex.h"
#include "fatal.h"
#include "logger.h"

// JSON parsing (simple implementation)
#include "jsmn.c"

dynamic_device_manager_t *dynamic_device_manager_create(void)
{
    dynamic_device_manager_t *manager = calloc(1, sizeof(dynamic_device_manager_t));
    if (!manager) {
        return NULL;
    }
    
    list_ensure_size(&manager->device_definitions, MAX_DYNAMIC_DEVICES);
    list_ensure_size(&manager->loaded_devices, MAX_DYNAMIC_DEVICES);
    
    manager->devices_dir = strdup("devices");
    manager->auto_reload = 1;
    time(&manager->last_scan);
    
    return manager;
}

void device_definition_free(device_definition_t *def)
{
    if (!def) return;
    
    free(def->name);
    free(def->modulation);
    free(def->description);
    free(def->flex_spec);
    free(def->config_file);
    free(def);
}

void dynamic_device_manager_destroy(dynamic_device_manager_t *manager)
{
    if (!manager) return;
    
    // Free device definitions
    for (size_t i = 0; i < manager->device_definitions.len; i++) {
        device_definition_free((device_definition_t*)manager->device_definitions.elems[i]);
    }
    list_clear(&manager->device_definitions, NULL);
    
    // Note: loaded_devices are managed by rtl_433 config, not freed here
    list_clear(&manager->loaded_devices, NULL);
    
    free(manager->devices_dir);
    free(manager);
}

// Simple JSON parser for device configuration
static int parse_json_string(const char *json, jsmntok_t *tokens, int token_index, char **output)
{
    jsmntok_t *tok = &tokens[token_index];
    if (tok->type != JSMN_STRING) {
        return -1;
    }
    
    int len = tok->end - tok->start;
    *output = malloc(len + 1);
    if (!*output) return -1;
    
    strncpy(*output, json + tok->start, len);
    (*output)[len] = '\0';
    
    return 0;
}

static int find_json_key(const char *json, jsmntok_t *tokens, int num_tokens, const char *key)
{
    for (int i = 0; i < num_tokens - 1; i++) {
        if (tokens[i].type == JSMN_STRING) {
            int len = tokens[i].end - tokens[i].start;
            if (strncmp(json + tokens[i].start, key, len) == 0 && strlen(key) == len) {
                return i + 1; // Return value token index
            }
        }
    }
    return -1;
}

int parse_device_config_json(const char *json_data, device_definition_t *device)
{
    jsmn_parser parser;
    jsmntok_t tokens[128];
    
    jsmn_init(&parser);
    int num_tokens = jsmn_parse(&parser, json_data, strlen(json_data), tokens, 128);
    
    if (num_tokens < 0) {
        fprintf(stderr, "Failed to parse JSON: %d\n", num_tokens);
        return -1;
    }
    
    // Parse required fields
    int idx;
    
    // Parse name
    idx = find_json_key(json_data, tokens, num_tokens, "name");
    if (idx > 0) {
        parse_json_string(json_data, tokens, idx, &device->name);
    }
    
    // Parse modulation
    idx = find_json_key(json_data, tokens, num_tokens, "modulation");
    if (idx > 0) {
        parse_json_string(json_data, tokens, idx, &device->modulation);
    }
    
    // Parse description
    idx = find_json_key(json_data, tokens, num_tokens, "description");
    if (idx > 0) {
        parse_json_string(json_data, tokens, idx, &device->description);
    }
    
    // Parse flex_spec
    idx = find_json_key(json_data, tokens, num_tokens, "flex_spec");
    if (idx > 0) {
        parse_json_string(json_data, tokens, idx, &device->flex_spec);
    }
    
    // Parse enabled (optional, default true)
    device->enabled = 1;
    idx = find_json_key(json_data, tokens, num_tokens, "enabled");
    if (idx > 0 && tokens[idx].type == JSMN_PRIMITIVE) {
        char enabled_str[8];
        int len = tokens[idx].end - tokens[idx].start;
        strncpy(enabled_str, json_data + tokens[idx].start, len);
        enabled_str[len] = '\0';
        device->enabled = (strcmp(enabled_str, "true") == 0) ? 1 : 0;
    }
    
    // Parse priority (optional, default 0)
    device->priority = 0;
    idx = find_json_key(json_data, tokens, num_tokens, "priority");
    if (idx > 0 && tokens[idx].type == JSMN_PRIMITIVE) {
        char priority_str[16];
        int len = tokens[idx].end - tokens[idx].start;
        strncpy(priority_str, json_data + tokens[idx].start, len);
        priority_str[len] = '\0';
        device->priority = atoi(priority_str);
    }
    
    return 0;
}

// Simple INI parser for device configuration
int parse_device_config_ini(const char *ini_data, device_definition_t *device)
{
    char *data_copy = strdup(ini_data);
    char *line = strtok(data_copy, "\n\r");
    
    while (line) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            line = strtok(NULL, "\n\r");
            continue;
        }
        
        // Parse key=value pairs
        char *equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            char *key = line;
            char *value = equals + 1;
            
            // Trim whitespace
            while (*key == ' ' || *key == '\t') key++;
            while (*value == ' ' || *value == '\t') value++;
            
            // Remove trailing whitespace
            char *end = key + strlen(key) - 1;
            while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';
            end = value + strlen(value) - 1;
            while (end > value && (*end == ' ' || *end == '\t')) *end-- = '\0';
            
            // Parse known keys
            if (strcmp(key, "name") == 0) {
                device->name = strdup(value);
            } else if (strcmp(key, "modulation") == 0) {
                device->modulation = strdup(value);
            } else if (strcmp(key, "description") == 0) {
                device->description = strdup(value);
            } else if (strcmp(key, "flex_spec") == 0) {
                device->flex_spec = strdup(value);
            } else if (strcmp(key, "enabled") == 0) {
                device->enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) ? 1 : 0;
            } else if (strcmp(key, "priority") == 0) {
                device->priority = atoi(value);
            }
        }
        
        line = strtok(NULL, "\n\r");
    }
    
    free(data_copy);
    return 0;
}

static char *read_file_content(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    
    fclose(file);
    return content;
}

int load_device_definitions_from_file(dynamic_device_manager_t *manager, const char *filename)
{
    char *content = read_file_content(filename);
    if (!content) {
        fprintf(stderr, "Failed to read device configuration file: %s\n", filename);
        return -1;
    }
    
    device_definition_t *device = calloc(1, sizeof(device_definition_t));
    if (!device) {
        free(content);
        return -1;
    }
    
    device->config_file = strdup(filename);
    device->enabled = 1; // Default enabled
    
    // Determine file format by extension
    const char *ext = strrchr(filename, '.');
    int parse_result = -1;
    
    if (ext && strcmp(ext, ".json") == 0) {
        parse_result = parse_device_config_json(content, device);
    } else if (ext && (strcmp(ext, ".ini") == 0 || strcmp(ext, ".conf") == 0)) {
        parse_result = parse_device_config_ini(content, device);
    } else {
        // Try to auto-detect format
        if (content[0] == '{') {
            parse_result = parse_device_config_json(content, device);
        } else {
            parse_result = parse_device_config_ini(content, device);
        }
    }
    
    free(content);
    
    if (parse_result != 0 || !device->name || !device->flex_spec) {
        fprintf(stderr, "Failed to parse device configuration: %s\n", filename);
        device_definition_free(device);
        return -1;
    }
    
    // Add to manager
    list_push(&manager->device_definitions, device);
    
    printf("Loaded device definition: %s from %s\n", device->name, filename);
    return 0;
}

int load_device_definitions_from_dir(dynamic_device_manager_t *manager, const char *dirname)
{
    int loaded_count = 0;
    
#ifndef _WIN32
    DIR *dir = opendir(dirname);
    if (!dir) {
        fprintf(stderr, "Failed to open devices directory: %s\n", dirname);
        return -1;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && (strcmp(ext, ".json") == 0 || strcmp(ext, ".ini") == 0 || strcmp(ext, ".conf") == 0)) {
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", dirname, entry->d_name);
                
                if (load_device_definitions_from_file(manager, filepath) == 0) {
                    loaded_count++;
                }
            }
        }
    }
    
    closedir(dir);
#else
    // Windows implementation
    char search_path[512];
    snprintf(search_path, sizeof(search_path), "%s\\*.*", dirname);
    
    WIN32_FIND_DATA find_data;
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                const char *ext = strrchr(find_data.cFileName, '.');
                if (ext && (strcmp(ext, ".json") == 0 || strcmp(ext, ".ini") == 0 || strcmp(ext, ".conf") == 0)) {
                    char filepath[512];
                    snprintf(filepath, sizeof(filepath), "%s\\%s", dirname, find_data.cFileName);
                    
                    if (load_device_definitions_from_file(manager, filepath) == 0) {
                        loaded_count++;
                    }
                }
            }
        } while (FindNextFile(hFind, &find_data));
        
        FindClose(hFind);
    }
#endif
    
    time(&manager->last_scan);
    printf("Loaded %d dynamic device definitions from %s\n", loaded_count, dirname);
    return loaded_count;
}

device_definition_t *find_device_definition(dynamic_device_manager_t *manager, const char *name)
{
    for (size_t i = 0; i < manager->device_definitions.len; i++) {
        device_definition_t *def = (device_definition_t*)manager->device_definitions.elems[i];
        if (def && def->name && strcmp(def->name, name) == 0) {
            return def;
        }
    }
    return NULL;
}

int register_dynamic_device(r_cfg_t *cfg, dynamic_device_manager_t *manager, const char *device_name)
{
    device_definition_t *def = find_device_definition(manager, device_name);
    if (!def) {
        fprintf(stderr, "Device definition not found: %s\n", device_name);
        return -1;
    }
    
    if (!def->enabled) {
        fprintf(stderr, "Device is disabled: %s\n", device_name);
        return -1;
    }
    
    // Create flex device using existing rtl_433 infrastructure
    r_device *flex_device = flex_create_device(def->flex_spec);
    if (!flex_device) {
        fprintf(stderr, "Failed to create flex device for: %s\n", device_name);
        return -1;
    }
    
    // Register the device
    register_protocol(cfg, flex_device, "");
    
    // Track the loaded device
    list_push(&manager->loaded_devices, flex_device);
    
    printf("Registered dynamic device: %s\n", device_name);
    return 0;
}

int register_all_dynamic_devices(r_cfg_t *cfg, dynamic_device_manager_t *manager)
{
    int registered_count = 0;
    
    for (size_t i = 0; i < manager->device_definitions.len; i++) {
        device_definition_t *def = (device_definition_t*)manager->device_definitions.elems[i];
        if (def && def->enabled) {
            if (register_dynamic_device(cfg, manager, def->name) == 0) {
                registered_count++;
            }
        }
    }
    
    printf("Registered %d dynamic devices\n", registered_count);
    return registered_count;
}

void list_dynamic_devices(dynamic_device_manager_t *manager)
{
    printf("\nDynamic Device Definitions:\n");
    printf("%-20s %-10s %-15s %s\n", "Name", "Enabled", "Modulation", "Description");
    printf("%-20s %-10s %-15s %s\n", "----", "-------", "----------", "-----------");
    
    for (size_t i = 0; i < manager->device_definitions.len; i++) {
        device_definition_t *def = (device_definition_t*)manager->device_definitions.elems[i];
        if (def) {
            printf("%-20s %-10s %-15s %s\n", 
                   def->name ? def->name : "?",
                   def->enabled ? "Yes" : "No",
                   def->modulation ? def->modulation : "?",
                   def->description ? def->description : "");
        }
    }
    printf("\n");
}

int enable_dynamic_device(dynamic_device_manager_t *manager, const char *name)
{
    device_definition_t *def = find_device_definition(manager, name);
    if (def) {
        def->enabled = 1;
        return 0;
    }
    return -1;
}

int disable_dynamic_device(dynamic_device_manager_t *manager, const char *name)
{
    device_definition_t *def = find_device_definition(manager, name);
    if (def) {
        def->enabled = 0;
        return 0;
    }
    return -1;
}

int check_for_device_updates(dynamic_device_manager_t *manager)
{
    if (!manager->auto_reload) {
        return 0;
    }
    
    struct stat dir_stat;
    if (stat(manager->devices_dir, &dir_stat) == 0) {
        if (dir_stat.st_mtime > manager->last_scan) {
            printf("Device directory updated, reloading...\n");
            return reload_device_definitions(manager);
        }
    }
    
    return 0;
}

int reload_device_definitions(dynamic_device_manager_t *manager)
{
    // Clear existing definitions
    for (size_t i = 0; i < manager->device_definitions.len; i++) {
        device_definition_free((device_definition_t*)manager->device_definitions.elems[i]);
    }
    list_clear(&manager->device_definitions, NULL);
    
    // Reload from directory
    return load_device_definitions_from_dir(manager, manager->devices_dir);
}

char *serialize_device_config_json(device_definition_t *device)
{
    if (!device) return NULL;
    
    char *json = malloc(1024);
    if (!json) return NULL;
    
    snprintf(json, 1024,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"modulation\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"flex_spec\": \"%s\",\n"
        "  \"enabled\": %s,\n"
        "  \"priority\": %d\n"
        "}",
        device->name ? device->name : "",
        device->modulation ? device->modulation : "",
        device->description ? device->description : "",
        device->flex_spec ? device->flex_spec : "",
        device->enabled ? "true" : "false",
        device->priority
    );
    
    return json;
}
