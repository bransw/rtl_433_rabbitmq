/** @file
 * Common configuration for rtl_433 client and server
 * 
 * Unified configuration eliminates duplication between
 * client_config.h and server_config.h
 */

#ifndef RTL433_CONFIG_H
#define RTL433_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "rtl433_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Application type
typedef enum {
    RTL433_APP_CLIENT,  // Client (demodulation, sending)
    RTL433_APP_SERVER   // Server (reception, decoding)
} rtl433_app_type_t;

/// Common rtl_433 configuration
typedef struct {
    // === Basic parameters ===
    rtl433_app_type_t app_type;         // Application type
    int verbosity;                      // Logging verbosity level
    char *config_file;                  // Path to configuration file
    char *pid_file;                     // Path to PID file
    bool daemon_mode;                   // Daemon mode
    
    // === Transport ===
    rtl433_transport_config_t transport; // Transport configuration
    
    // === Client parameters (SDR) ===
    struct {
        char *device_selection;         // SDR device selection  
        uint32_t sample_rate;           // Sample rate (Hz)
        uint32_t frequency;             // Center frequency (Hz)
        int gain;                       // Gain
        bool auto_gain;                 // Automatic gain
        int ppm_error;                  // PPM correction
        char *input_file;               // Input file (instead of SDR)
        int hop_time;                   // Frequency hop time
        uint32_t *hop_times;            // Hop times array
        uint32_t *frequencies;          // Frequency array for hopping
        int hop_count;                  // Number of frequencies
        bool test_mode;                 // Test mode
    } sdr;
    
    // === Server parameters ===
    struct {
        int max_connections;            // Maximum connections
        int stats_interval;             // Statistics interval (sec)
        char *database_path;            // Database path
        bool store_unknown;             // Store unknown signals
        int queue_size;                 // Message queue size
        int worker_threads;             // Number of worker threads
    } server;
    
    // === Signal processing ===
    struct {
        bool analyze_pulses;            // Analyze pulses
        int pulse_threshold;            // Pulse detection threshold
        bool enable_ook;                // Enable OOK decoders
        bool enable_fsk;                // Enable FSK decoders
        char **protocol_names;          // Protocol names list
        int protocol_count;             // Number of protocols
        bool flex_decoder;              // Flexible decoder
    } signal;
    
    // === Output ===
    struct {
        char **output_formats;          // Output formats (JSON, CSV, etc)
        int output_count;               // Number of formats
        char *output_file;              // Output file
        bool live_output;               // Live output
        bool timestamp;                 // Add timestamps
    } output;
    
    // === Debug ===
    struct {
        bool debug_demod;               // Debug demodulation
        bool debug_device;              // Debug devices
        bool save_signals;              // Save signals
        char *signal_save_path;         // Signal save path
        int signal_save_limit;          // Signal save limit
    } debug;
    
} rtl433_config_t;

/// Command line options (common)
typedef struct {
    bool help;                          // Show help
    bool version;                       // Show version
    bool list_devices;                  // List SDR devices
    bool list_protocols;                // List protocols
    bool quiet;                         // Quiet mode
    bool verbose;                       // Verbose mode
    char *config_file;                  // Configuration file
} rtl433_options_t;

// === ФУНКЦИИ КОНФИГУРАЦИИ ===

/// Initialize default configuration
int rtl433_config_init(rtl433_config_t *config, rtl433_app_type_t app_type);

/// Free configuration
void rtl433_config_free(rtl433_config_t *config);

/// Load configuration from file
int rtl433_config_load_file(rtl433_config_t *config, const char *filename);

/// Save configuration to file
int rtl433_config_save_file(const rtl433_config_t *config, const char *filename);

/// Parse command line arguments
int rtl433_config_parse_args(rtl433_config_t *config, int argc, char **argv);

/// Validate configuration
int rtl433_config_validate(const rtl433_config_t *config);

/// Apply configuration to transport
int rtl433_config_apply_transport(rtl433_config_t *config);

// === УТИЛИТЫ КОНФИГУРАЦИИ ===

/// Set transport by URL
int rtl433_config_set_transport_url(rtl433_config_t *config, const char *url);

/// Add protocol by name
int rtl433_config_add_protocol(rtl433_config_t *config, const char *protocol_name);

/// Add output format
int rtl433_config_add_output_format(rtl433_config_t *config, const char *format);

/// Set logging level
void rtl433_config_set_verbosity(rtl433_config_t *config, int level);

/// Get string description of application type
const char* rtl433_config_app_type_string(rtl433_app_type_t type);

/// Print configuration (for debugging)
void rtl433_config_print(const rtl433_config_t *config);

/// Print usage help
void rtl433_config_print_help(rtl433_app_type_t app_type);

/// Print version
void rtl433_config_print_version(void);

// === ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ ===

#define RTL433_DEFAULT_SAMPLE_RATE    2048000
#define RTL433_DEFAULT_FREQUENCY      433920000
#define RTL433_DEFAULT_GAIN           0        // auto
#define RTL433_DEFAULT_PPM_ERROR      0
#define RTL433_DEFAULT_HOP_TIME       600
#define RTL433_DEFAULT_STATS_INTERVAL 300
#define RTL433_DEFAULT_QUEUE_SIZE     1000
#define RTL433_DEFAULT_WORKER_THREADS 2
#define RTL433_DEFAULT_VERBOSITY      1        // LOG_INFO

#define RTL433_DEFAULT_TRANSPORT_HOST "localhost"
#define RTL433_DEFAULT_TRANSPORT_PORT 5672
#define RTL433_DEFAULT_TRANSPORT_QUEUE "rtl_433"

// === ПРЕДОПРЕДЕЛЕННЫЕ КОНФИГУРАЦИИ ===

/// Default client configuration
int rtl433_config_client_defaults(rtl433_config_t *config);

/// Default server configuration  
int rtl433_config_server_defaults(rtl433_config_t *config);

/// Configuration for testing
int rtl433_config_test_defaults(rtl433_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // RTL433_CONFIG_H
