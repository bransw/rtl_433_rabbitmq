/** @file
    Enhanced rtl_433 with dynamic device loading support.
    
    This is a demonstration of how to integrate dynamic device loading
    into the main rtl_433 application.
*/

#include "rtl_433.c"  // Include original implementation
#include "dynamic_device_loader.h"

// Global dynamic device manager
static dynamic_device_manager_t *g_dynamic_manager = NULL;

// Enhanced help function that includes dynamic device options
static void usage_dynamic(void)
{
    usage(); // Call original usage function
    
    printf("\nDynamic Device Options:\n");
    printf("  [-D <dir>]    Directory to scan for dynamic device definitions (default: ./devices)\n");
    printf("  [-L]          List available dynamic devices\n");
    printf("  [-E <name>]   Enable specific dynamic device\n");
    printf("  [-G <name>]   Disable specific dynamic device\n");
    printf("  [-H]          Disable auto-reload of device definitions\n");
    printf("  [-J <file>]   Load device definition from specific file\n");
    printf("\nDynamic device configuration files:\n");
    printf("  - JSON format: .json extension\n");
    printf("  - INI format: .ini or .conf extension\n");
    printf("  - Auto-detection based on content if no extension\n");
    printf("\nExample dynamic device JSON:\n");
    printf("{\n");
    printf("  \"name\": \"MyDevice\",\n");
    printf("  \"modulation\": \"OOK_PWM\",\n");
    printf("  \"description\": \"Custom device\",\n");
    printf("  \"flex_spec\": \"n=MyDevice,m=OOK_PWM,s=100,l=200,r=2000\",\n");
    printf("  \"enabled\": true,\n");
    printf("  \"priority\": 0\n");
    printf("}\n");
}

// Enhanced argument parsing
static int parse_dynamic_args(r_cfg_t *cfg, int argc, char **argv)
{
    int option_index = 0;
    int opt;
    
    // Extended getopt string with dynamic options
    const char *short_options = "hVvqQ:t:r:p:s:f:g:d:a:y:n:S:G:K:I:U:w:W:F:M:C:T:l:m:Y:z:b:A:c:L::R:X:x:D:LE:G:HJ:";
    
    while ((opt = getopt_long(argc, argv, short_options, NULL, &option_index)) != -1) {
        switch (opt) {
        case 'D':
            // Set dynamic devices directory
            if (g_dynamic_manager) {
                free(g_dynamic_manager->devices_dir);
                g_dynamic_manager->devices_dir = strdup(optarg);
                printf("Dynamic devices directory set to: %s\n", optarg);
            }
            break;
            
        case 'L':
            // List dynamic devices
            if (optarg) {
                // Load from specific directory
                if (g_dynamic_manager) {
                    load_device_definitions_from_dir(g_dynamic_manager, optarg);
                }
            } else if (g_dynamic_manager) {
                // Load from default directory
                load_device_definitions_from_dir(g_dynamic_manager, g_dynamic_manager->devices_dir);
            }
            
            if (g_dynamic_manager) {
                list_dynamic_devices(g_dynamic_manager);
            }
            exit(0);
            break;
            
        case 'E':
            // Enable specific dynamic device
            if (g_dynamic_manager && optarg) {
                if (enable_dynamic_device(g_dynamic_manager, optarg) == 0) {
                    printf("Enabled dynamic device: %s\n", optarg);
                } else {
                    fprintf(stderr, "Failed to enable device: %s\n", optarg);
                }
            }
            break;
            
        case 'G':
            // Disable specific dynamic device
            if (g_dynamic_manager && optarg) {
                if (disable_dynamic_device(g_dynamic_manager, optarg) == 0) {
                    printf("Disabled dynamic device: %s\n", optarg);
                } else {
                    fprintf(stderr, "Failed to disable device: %s\n", optarg);
                }
            }
            break;
            
        case 'H':
            // Disable auto-reload
            if (g_dynamic_manager) {
                g_dynamic_manager->auto_reload = 0;
                printf("Auto-reload of device definitions disabled\n");
            }
            break;
            
        case 'J':
            // Load device definition from specific file
            if (g_dynamic_manager && optarg) {
                if (load_device_definitions_from_file(g_dynamic_manager, optarg) == 0) {
                    printf("Loaded device definition from: %s\n", optarg);
                } else {
                    fprintf(stderr, "Failed to load device definition from: %s\n", optarg);
                }
            }
            break;
            
        default:
            // Let original argument parser handle other options
            continue;
        }
    }
    
    return optind;
}

int main(int argc, char **argv)
{
    // Initialize dynamic device manager
    g_dynamic_manager = dynamic_device_manager_create();
    if (!g_dynamic_manager) {
        fprintf(stderr, "Failed to initialize dynamic device manager\n");
        exit(1);
    }
    
    // Initialize rtl_433 configuration
    r_cfg_t *cfg = r_create_cfg();
    if (!cfg) {
        fprintf(stderr, "Failed to create r_cfg_t\n");
        dynamic_device_manager_destroy(g_dynamic_manager);
        exit(1);
    }
    
    // Parse dynamic arguments first
    int dynamic_optind = parse_dynamic_args(cfg, argc, argv);
    
    // Load dynamic devices from default directory
    printf("Loading dynamic device definitions...\n");
    int loaded_devices = load_device_definitions_from_dir(g_dynamic_manager, g_dynamic_manager->devices_dir);
    if (loaded_devices > 0) {
        printf("Found %d dynamic device definitions\n", loaded_devices);
    }
    
    // Call original main logic with remaining arguments
    // Reset optind for original argument parsing
    optind = 1;
    
    // Use original argument parsing but catch help to show enhanced version
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage_dynamic();
            r_free_cfg(cfg);
            dynamic_device_manager_destroy(g_dynamic_manager);
            exit(0);
        }
    }
    
    // Parse original rtl_433 arguments
    int result = parse_conf_args(cfg, argc, argv);
    if (result != 0) {
        r_free_cfg(cfg);
        dynamic_device_manager_destroy(g_dynamic_manager);
        exit(result);
    }
    
    // Register dynamic devices after standard protocols
    printf("Registering dynamic devices...\n");
    int registered_dynamic = register_all_dynamic_devices(cfg, g_dynamic_manager);
    if (registered_dynamic > 0) {
        printf("Registered %d dynamic devices\n", registered_dynamic);
    }
    
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#ifndef _WIN32
    signal(SIGQUIT, sighandler);
    signal(SIGPIPE, SIG_IGN);
#endif
    
    // Main processing loop
    printf("Starting rtl_433 with dynamic device support...\n");
    
    while (!do_exit) {
        // Check for device definition updates periodically
        check_for_device_updates(g_dynamic_manager);
        
        // Run original rtl_433 processing
        // This would need to be extracted from original main function
        // For now, we'll call a simplified version
        
        // Process SDR data...
        if (run_sdr(cfg) != 0) {
            break;
        }
        
        // Sleep briefly to allow for graceful shutdown
        usleep(100000); // 100ms
    }
    
    // Cleanup
    printf("Shutting down rtl_433 with dynamic devices...\n");
    r_free_cfg(cfg);
    dynamic_device_manager_destroy(g_dynamic_manager);
    
    return 0;
}

// Convenience function to add dynamic device at runtime
int rtl433_add_dynamic_device(const char *device_spec)
{
    if (!g_dynamic_manager) {
        return -1;
    }
    
    // Create temporary file with device specification
    char temp_filename[] = "/tmp/rtl433_dynamic_XXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd < 0) {
        return -1;
    }
    
    FILE *temp_file = fdopen(fd, "w");
    if (!temp_file) {
        close(fd);
        unlink(temp_filename);
        return -1;
    }
    
    fprintf(temp_file, "%s\n", device_spec);
    fclose(temp_file);
    
    // Load device definition
    int result = load_device_definitions_from_file(g_dynamic_manager, temp_filename);
    
    // Cleanup temporary file
    unlink(temp_filename);
    
    return result;
}

// API function to reload all dynamic devices
int rtl433_reload_dynamic_devices(void)
{
    if (!g_dynamic_manager) {
        return -1;
    }
    
    return reload_device_definitions(g_dynamic_manager);
}
