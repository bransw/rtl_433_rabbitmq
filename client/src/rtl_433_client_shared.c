/** @file
 * RTL_433 Client - Unified version using shared library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "rtl_433.h" 
#include "r_api.h"
#include "sdr.h"
#include "confparse.h"
#include "rtl433_transport.h"
#include "rtl433_signal.h"
#include "rtl433_config.h"

// Global variables
static r_cfg_t *g_cfg = NULL;
static rtl433_transport_connection_t g_transport;
static rtl433_config_t g_client_config;
static volatile sig_atomic_t exit_flag = 0;

// Temporary storage for files parsed before g_cfg is ready
static char* temp_files[10] = {0};
static int temp_file_count = 0;

// Statistics
typedef struct {
    unsigned long signals_sent;
    unsigned long send_errors;
    time_t start_time;
} client_stats_t;

static client_stats_t g_stats = {0};

// Signal handler
static void signal_handler(int sig)
{
    (void)sig;
    exit_flag = 1;
}

// Simplified pulse handler that uses shared library
static void shared_pulse_handler(pulse_data_t *pulse_data)
{
    if (!pulse_data || pulse_data->num_pulses == 0) return;
    
    // Create message using shared library
    rtl433_message_t *message = rtl433_message_create_from_pulse_data(
        pulse_data, "FSK", rtl433_generate_package_id());
    
    if (message) {
        int result = rtl433_transport_send_message(&g_transport, message);
        if (result == 0) {
            g_stats.signals_sent++;
            printf("Sent signal: %u pulses\n", pulse_data->num_pulses);
        } else {
            g_stats.send_errors++;
            printf("Failed to send signal\n");
        }
        rtl433_message_free(message);
    }
}

int main(int argc, char **argv)
{
    printf("RTL_433 Client (Unified with Shared Library)\n");
    
    // Initialize shared config
    if (rtl433_config_init(&g_client_config, RTL433_APP_CLIENT) != 0) {
        fprintf(stderr, "Failed to initialize client config\n");
        return 1;
    }
    
    // Set default transport URL
    rtl433_config_set_transport_url(&g_client_config, "amqp://guest:guest@localhost:5672/rtl_433");
    
    // Parse command line arguments  
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            if (rtl433_config_set_transport_url(&g_client_config, argv[i + 1]) != 0) {
                fprintf(stderr, "Invalid transport URL: %s\n", argv[i + 1]);
                return 1;
            }
            i++; // Skip URL argument
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            // Store file for later (g_cfg not ready yet)
            if (temp_file_count < 10) {
                temp_files[temp_file_count++] = argv[i + 1];
                printf("📁 Will process file: %s\n", argv[i + 1]);
            }
            i++; // Skip filename argument
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("RTL_433 Client (Minimal/Demo version)\n\n");
            printf("Usage: %s [options] [input_files...]\n", argv[0]);
            printf("Options:\n");
            printf("  -T <url>     Transport URL (default: amqp://guest:guest@localhost:5672/rtl_433)\n");
            printf("  -r <file>    Read data from input file (.cu8, .cs16, .cf32) instead of SDR\n");
            printf("  -h, --help   Show this help\n");
            printf("\nFile formats supported:\n");
            printf("  .cu8         Complex unsigned 8-bit (RTL-SDR format)\n");
            printf("  .cs16        Complex signed 16-bit\n");
            printf("  .cf32        Complex float 32-bit\n");
            printf("\nNote: This is a minimal/demo version. For full functionality use the full client.\n");
            printf("Positional arguments will be treated as input files.\n");
            return 0;
        } else {
            // Positional argument - treat as input file
            if (temp_file_count < 10) {
                temp_files[temp_file_count++] = argv[i];
                printf("📁 Will process file: %s\n", argv[i]);
            }
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize rtl_433
    g_cfg = r_create_cfg();
    if (!g_cfg) {
        fprintf(stderr, "Failed to create rtl_433 config\n");
        return 1;
    }
    
    // Initialize rtl_433 config
    r_init_cfg(g_cfg);
    
    // Set some basic defaults
    g_cfg->center_frequency = 433920000; // 433.92 MHz
    g_cfg->samp_rate = 2048000;          // 2.048 MHz
    
    // Add stored files to config
    for (int i = 0; i < temp_file_count; i++) {
        if (temp_files[i]) {
            add_infile(g_cfg, temp_files[i]);
            printf("📁 Added file to rtl_433 config: %s\n", temp_files[i]);
        }
    }
    
    // Connect to transport
    if (rtl433_transport_connect(&g_transport, &g_client_config.transport) != 0) {
        fprintf(stderr, "Failed to connect to transport\n");
        return 1;
    }
    
    printf("Connected to %s://%s:%d/%s\n",
           rtl433_transport_type_to_string(g_client_config.transport.type),
           g_client_config.transport.host,
           g_client_config.transport.port,
           g_client_config.transport.queue);
    
    int result = 0;
    
    // Check if we have input files
    if (g_cfg->in_files.len > 0) {
        printf("📁 Processing %d input file(s) in minimal mode...\n", (int)g_cfg->in_files.len);
        printf("⚠️ Note: File processing in minimal client is limited - signals will be forwarded but not fully decoded\n");
        
        // For minimal client, we'll simulate file processing
        // In a real implementation, we'd need to integrate with rtl_433's file reading
        for (void **iter = g_cfg->in_files.elems; iter && *iter; ++iter) {
            printf("📄 Would process file: %s\n", (char*)*iter);
            // TODO: Implement actual file reading and pulse extraction
            g_stats.signals_sent++;
        }
        
        printf("✅ File processing simulation completed\n");
    } else {
        printf("📡 Starting live SDR processing...\n");
        
        // Setup SDR device
        if (sdr_open(&g_cfg->dev, NULL, 1) != 0) {
            fprintf(stderr, "SDR: No supported devices found.\n");
            fprintf(stderr, "Failed to open SDR device\n");
            result = 1;
            goto cleanup;
        }
        
        // Configure SDR
        sdr_set_center_freq(g_cfg->dev, g_cfg->center_frequency, 1);
        
        // Start async reading (minimal implementation)
        printf("⚠️ Note: Minimal client has limited SDR processing\n");
        result = sdr_start(g_cfg->dev, NULL, NULL, 4, 16384);
    }

cleanup:
    
    // Cleanup
    rtl433_transport_disconnect(&g_transport);
    rtl433_config_free(&g_client_config);
    
    if (g_cfg) {
        r_free_cfg(g_cfg);
    }
    
    printf("Client finished with result: %d\n", result);
    return result;
}
