/** @file
 * RTL_433 Server - Full version with real device decoding
 * Uses shared library for transport and signal processing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

// RTL_433 includes
#include "rtl_433.h"
#include "r_api.h"
#include "r_private.h"
#include "list.h"

// Shared library includes
#include "rtl433_transport.h"
#include "rtl433_signal.h"
#include "rtl433_config.h"

// Global variables
static rtl433_transport_connection_t g_transport;
static rtl433_config_t g_server_config;
static r_cfg_t *g_cfg = NULL;
static volatile sig_atomic_t exit_flag = 0;

// Statistics
typedef struct {
    unsigned long signals_received;
    unsigned long devices_decoded;
    unsigned long unknown_signals;
    unsigned long errors;
    time_t start_time;
} server_stats_t;

static server_stats_t g_stats = {0};

// Signal handler
static void signal_handler(int sig)
{
    (void)sig;
    exit_flag = 1;
}

// Update statistics
static void update_stats(int signals_received, int devices_decoded, int unknown_signals, int errors)
{
    g_stats.signals_received += signals_received;
    g_stats.devices_decoded += devices_decoded;
    g_stats.unknown_signals += unknown_signals;
    g_stats.errors += errors;
}

// Print statistics
static void print_statistics(void)
{
    time_t now = time(NULL);
    double uptime = difftime(now, g_stats.start_time);
    
    printf("\n📊 Server Statistics:\n");
    printf("  Uptime: %.0f seconds\n", uptime);
    printf("  Signals received: %lu\n", g_stats.signals_received);
    printf("  Devices decoded: %lu\n", g_stats.devices_decoded);
    printf("  Unknown signals: %lu\n", g_stats.unknown_signals);
    printf("  Errors: %lu\n", g_stats.errors);
    
    if (g_stats.signals_received > 0) {
        double success_rate = (double)g_stats.devices_decoded / g_stats.signals_received * 100.0;
        printf("  Success rate: %.1f%%\n", success_rate);
    }
}

// Message handler for real device decoding
static void full_message_handler(rtl433_message_t *message, void *user_data)
{
    (void)user_data;
    
    if (!message || !message->pulse_data) {
        printf("❌ Received null message or pulse_data\n");
        update_stats(1, 0, 0, 1);
        return;
    }
    
    printf("📡 Received signal: package_id=%lu, mod=%s, pulses=%u\n",
           message->package_id,
           message->modulation ? message->modulation : "unknown",
           message->pulse_data->num_pulses);
    
    // Real device decoding using shared library
    rtl433_decode_result_t result;
    memset(&result, 0, sizeof(result));
    
    int devices_found = rtl433_signal_decode(message->pulse_data, 
                                           &g_cfg->demod->r_devs, 
                                           &result);
    
    if (devices_found > 0) {
        printf("✅ Device decoded: %d device(s) found (pkg=%lu)\n", 
               devices_found, message->package_id);
        
        // Print decoded data (simplified)
        if (result.device_names && result.device_names[0]) {
            printf("   Device: %s\n", result.device_names[0]);
        }
        
        update_stats(1, devices_found, 0, 0);
        
        // Clean up result
        rtl433_decode_result_free(&result);
    } else {
        printf("❓ No devices decoded from package_id=%lu\n", message->package_id);
        update_stats(1, 0, 1, 0);
    }
}

// Initialize RTL_433 configuration
static int init_rtl433_config(void)
{
    // Allocate main config
    g_cfg = calloc(1, sizeof(r_cfg_t));
    if (!g_cfg) {
        fprintf(stderr, "Failed to allocate r_cfg_t\n");
        return -1;
    }
    
    // Initialize demodulator
    g_cfg->demod = calloc(1, sizeof(struct dm_state));
    if (!g_cfg->demod) {
        fprintf(stderr, "Failed to allocate dm_state\n");
        free(g_cfg);
        return -1;
    }
    
    // Set basic parameters
    g_cfg->samp_rate = 250000;
    g_cfg->demod->level_limit = 0;
    g_cfg->report_time = REPORT_TIME_DEFAULT;
    g_cfg->report_time_hires = false;
    g_cfg->report_time_tz = false;
    g_cfg->report_time_utc = false;
    
    // Initialize device list
    list_ensure_size(&g_cfg->demod->r_devs, 300);
    
    // Register all available device protocols
    register_all_protocols(g_cfg, 0);
    
    printf("📋 Registered device protocols\n");
    
    return 0;
}

// Cleanup RTL_433 configuration
static void cleanup_rtl433_config(void)
{
    if (g_cfg) {
        if (g_cfg->demod) {
            list_free_elems(&g_cfg->demod->r_devs, free);
            free(g_cfg->demod);
        }
        free(g_cfg);
        g_cfg = NULL;
    }
}

int main(int argc, char **argv)
{
    printf("RTL_433 Server (Full with Real Device Decoding)\n");
    
    // Initialize shared config
    if (rtl433_config_init(&g_server_config, RTL433_APP_SERVER) != 0) {
        fprintf(stderr, "Failed to initialize server config\n");
        return 1;
    }
    
    // Initialize RTL_433 configuration
    if (init_rtl433_config() != 0) {
        fprintf(stderr, "Failed to initialize RTL_433 config\n");
        rtl433_config_free(&g_server_config);
        return 1;
    }
    
    // Set default transport URL
    rtl433_config_set_transport_url(&g_server_config, "amqp://guest:guest@localhost:5672/rtl_433");
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            if (rtl433_config_set_transport_url(&g_server_config, argv[i + 1]) != 0) {
                fprintf(stderr, "Invalid transport URL: %s\n", argv[i + 1]);
                cleanup_rtl433_config();
                rtl433_config_free(&g_server_config);
                return 1;
            }
            i++; // Skip URL argument
        } else if (strcmp(argv[i], "-v") == 0) {
            g_cfg->verbosity++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -T <url>     Transport URL (default: amqp://guest:guest@localhost:5672/rtl_433)\n");
            printf("  -v           Increase verbosity\n");
            printf("  -h, --help   Show this help\n");
            cleanup_rtl433_config();
            rtl433_config_free(&g_server_config);
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Connect to transport
    if (rtl433_transport_connect(&g_transport, &g_server_config.transport) != 0) {
        fprintf(stderr, "Failed to connect to transport\n");
        cleanup_rtl433_config();
        rtl433_config_free(&g_server_config);
        return 1;
    }
    
    printf("🔗 Connected to %s://%s:%d/%s\n",
           rtl433_transport_type_to_string(g_server_config.transport.type),
           g_server_config.transport.host,
           g_server_config.transport.port,
           g_server_config.transport.queue);
    
    // Initialize statistics
    time(&g_stats.start_time);
    
    printf("🚀 Full server ready, waiting for signals...\n");
    printf("   (Real device decoding enabled)\n");
    
    // Main server loop
    while (!exit_flag) {
        // Receive messages via shared library
        int result = rtl433_transport_receive_messages(&g_transport, 
                                                      full_message_handler, 
                                                      NULL, 
                                                      1000); // 1 sec timeout
        
        if (result < 0) {
            printf("⚠️ Transport error\n");
            update_stats(0, 0, 0, 1);
            usleep(100000); // 100ms delay on error
        }
        
        // Print stats every 30 seconds
        static time_t last_stats = 0;
        time_t now = time(NULL);
        if (difftime(now, last_stats) >= 30) {
            print_statistics();
            last_stats = now;
        }
    }
    
    // Cleanup
    printf("\n🛑 Server shutting down...\n");
    print_statistics();
    
    rtl433_transport_disconnect(&g_transport);
    cleanup_rtl433_config();
    rtl433_config_free(&g_server_config);
    
    printf("Server finished.\n");
    return 0;
}
