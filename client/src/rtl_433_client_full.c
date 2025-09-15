/** @file
 * RTL_433 Client - Full version with real pulse processing
 * Uses shared library for transport and signal processing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#ifdef _WIN32
    #include <windows.h>
#endif
#include <stdint.h>

// RTL_433 includes
#include "rtl_433.h" 
#include "r_api.h"
#include "r_private.h"
#include "r_util.h"
#include "sdr.h"
#include "confparse.h"
#include "pulse_detect.h"
#include "pulse_detect_fsk.h"
#include "pulse_data.h"
#include "logger.h"
#include "fileformat.h"
#include "optparse.h"
#include <json-c/json.h>

// Constants
#ifndef DEFAULT_BUF_LENGTH
#define DEFAULT_BUF_LENGTH (16 * 16384)
#endif
#ifndef DEFAULT_ASYNC_BUF_NUMBER  
#define DEFAULT_ASYNC_BUF_NUMBER 4
#endif

// Shared library includes  
#include "rtl433_transport.h"
#include "rtl433_signal.h"
#include "rtl433_config.h"

// Forward declarations
r_device *flex_create_device(char *spec); // from flex.c

// Helper function to show available protocols
static void help_protocols(r_device *devices, unsigned num_devices, int exit_code)
{
    unsigned i;
    char disabledc;

    if (devices) {
        FILE *fp = exit_code ? stderr : stdout;
        fprintf(fp, "\t\t= Supported device protocols =\n");
        for (i = 0; i < num_devices; i++) {
            disabledc = devices[i].disabled ? '*' : ' ';
            if (devices[i].disabled <= 2) // if not hidden
                fprintf(fp, "    [%02u]%c %s\n", i + 1, disabledc, devices[i].name);
        }
        fprintf(fp, "\n* Disabled by default, use -R n or a conf file to enable\n");
    }
    exit(exit_code);
}

// Global variables
static r_cfg_t *g_cfg = NULL;
static rtl433_transport_connection_t g_transport;
static rtl433_config_t g_client_config;

// Variables for tracking detected devices
static volatile int g_detected_devices_count = 0;
static char *g_last_device_json = NULL;
static volatile sig_atomic_t exit_flag = 0;

// Forward declarations
static int process_input_files(r_cfg_t *cfg);
static int start_sdr_processing(r_cfg_t *cfg);
static void full_sdr_callback(sdr_event_t *ev, void *ctx);
static void process_signal_data(unsigned char *buf, uint32_t len, r_cfg_t *cfg);

// Statistics
typedef struct {
    unsigned long signals_processed;
    unsigned long signals_sent;
    unsigned long send_errors;
    unsigned long decode_attempts;
    time_t start_time;
} client_stats_t;

static client_stats_t g_stats = {0};

// Signal handler
static void signal_handler(int sig)
{
    (void)sig;
    exit_flag = 1;
    printf("\n🛑 Stopping client...\n");
}

// Update statistics
static void update_stats(int signals_processed, int signals_sent, int send_errors, int decode_attempts)
{
    g_stats.signals_processed += signals_processed;
    g_stats.signals_sent += signals_sent;
    g_stats.send_errors += send_errors;
    g_stats.decode_attempts += decode_attempts;
}

// Print statistics
static void print_statistics(void)
{
    time_t now = time(NULL);
    double uptime = difftime(now, g_stats.start_time);
    
    printf("\n📊 Client Statistics:\n");
    printf("  Uptime: %.0f seconds\n", uptime);
    printf("  Signals processed: %lu\n", g_stats.signals_processed);
    printf("  Signals sent: %lu\n", g_stats.signals_sent);
    printf("  Send errors: %lu\n", g_stats.send_errors);
    printf("  Decode attempts: %lu\n", g_stats.decode_attempts);
    
    if (g_stats.signals_processed > 0) {
        double send_rate = (double)g_stats.signals_sent / g_stats.signals_processed * 100.0;
        printf("  Send success rate: %.1f%%\n", send_rate);
    }
}

// Full pulse handler with real processing
static void full_pulse_handler(pulse_data_t *pulse_data, unsigned package_type)
{
    if (!pulse_data || pulse_data->num_pulses == 0) return;
    
    update_stats(1, 0, 0, 0);
    
    printf("📡 Processing signal: %u pulses, type=%s\n", 
           pulse_data->num_pulses,
           package_type == PULSE_DATA_OOK ? "OOK" : "FSK");
    
    // Determine modulation type
    const char *modulation = (package_type == PULSE_DATA_FSK) ? "FSK" : "OOK";
    
    // Create message using shared library
    rtl433_message_t *message = rtl433_message_create_from_pulse_data(
        pulse_data, modulation, rtl433_generate_package_id());
    
    if (message) {
        // Send ALL demodulated signals to "signals" queue
        int result = rtl433_transport_send_message_to_queue(&g_transport, message, "signals");
        if (result == 0) {
            update_stats(0, 1, 0, 0);
            printf("✅ Sent signal to queue 'signals': package_id=%lu, pulses=%u\n", 
                   message->package_id, pulse_data->num_pulses);
        } else {
            update_stats(0, 0, 1, 0);
            printf("❌ Failed to send signal to 'signals': package_id=%lu\n", message->package_id);
        }
        rtl433_message_free(message);
    } else {
        update_stats(0, 0, 1, 0);
        printf("❌ Failed to create message from pulse data\n");
    }
    
    // Optional: Try to decode locally for debugging
    if (g_cfg->verbosity >= LOG_DEBUG) {
        update_stats(0, 0, 0, 1);
        
        rtl433_decode_result_t result;
        memset(&result, 0, sizeof(result));
        
        // Skip local decoding for now due to complexity
        // Would require full device initialization which is complex
        printf("🔍 Local debug decode: skipped (deferred to server)\n");
    }
}

// Custom output handler to capture device data  
static void client_device_output_handler(r_device *r_dev, data_t *data)
{
    printf("🎯 DEVICE OUTPUT HANDLER CALLED: %s\n", r_dev ? r_dev->name : "unknown");
    
    // Call original handler first
    data_acquired_handler(r_dev, data);
    
    // Count detected devices
    g_detected_devices_count++;
    
    // Convert device data to JSON string
    json_object *device_obj = json_object_new_object();
    json_object *device_name_obj = json_object_new_string(r_dev->name);
    json_object_object_add(device_obj, "decoder", device_name_obj);
    
    // Add all data fields
    for (data_t *d = data; d; d = d->next) {
        json_object *value_obj = NULL;
        
        switch (d->type) {
            case DATA_STRING:
                value_obj = json_object_new_string(d->value.v_ptr);
                break;
            case DATA_INT:
                value_obj = json_object_new_int(d->value.v_int);
                break;
            case DATA_DOUBLE:
                value_obj = json_object_new_double(d->value.v_dbl);
                break;
            default:
                value_obj = json_object_new_string("unknown_type");
                break;
        }
        
        if (value_obj && d->key) {
            json_object_object_add(device_obj, d->key, value_obj);
        }
    }
    
    // Store JSON string
    if (g_last_device_json) {
        free(g_last_device_json);
    }
    g_last_device_json = strdup(json_object_to_json_string(device_obj));
    
    printf("🎯 Device detected by %s: %s\n", r_dev->name, 
           data && data->key ? data->key : "unknown");
    
    json_object_put(device_obj);
}

// Send detected devices to "detected" queue
static void send_detected_devices(pulse_data_t *pulse_data, int device_count, const char *modulation)
{
    if (device_count <= 0) return;
    
    printf("🎯 Sending %d detected device(s) to 'detected' queue\n", device_count);
    
    // Create detection result message (different from raw signal)
    rtl433_message_t *message = rtl433_message_create_from_pulse_data(
        pulse_data, modulation, rtl433_generate_package_id());
    if (!message) {
        printf("❌ Failed to create detection result message\n");
        return;
    }
    
    // Create detection result with REAL device data
    json_object *detection_obj = json_object_new_object();
    json_object *type_obj = json_object_new_string("detection_result");
    json_object *device_count_obj = json_object_new_int(device_count);
    json_object *pulses_obj = json_object_new_int(pulse_data->num_pulses);
    json_object *rssi_obj = json_object_new_double(pulse_data->rssi_db);
    json_object *freq_obj = json_object_new_double(pulse_data->freq1_hz);
    
    json_object_object_add(detection_obj, "message_type", type_obj);
    json_object_object_add(detection_obj, "devices_found", device_count_obj);
    json_object_object_add(detection_obj, "source_pulses", pulses_obj);
    json_object_object_add(detection_obj, "rssi_db", rssi_obj);
    json_object_object_add(detection_obj, "frequency_hz", freq_obj);
    
    // Add REAL device data if available
    if (g_last_device_json) {
        json_object *device_data_obj = json_tokener_parse(g_last_device_json);
        if (device_data_obj) {
            json_object_object_add(detection_obj, "device_data", device_data_obj);
        }
    }
    
    const char *json_str = json_object_to_json_string(detection_obj);
    
    // Replace the metadata with our detection metadata  
    if (message->metadata) free(message->metadata);
    message->metadata = strdup(json_str);
    
    printf("🔍 Attempting to send to queue 'detected': exchange='%s', routing_key='detected'\n", 
           g_transport.config->exchange);
    
    int result = rtl433_transport_send_message_to_queue(&g_transport, message, "detected");
    if (result == 0) {
        printf("✅ Sent detection result to queue 'detected': package_id=%lu, devices=%d\n", 
               message->package_id, device_count);
        printf("📋 Message content: %s\n", message->metadata ? message->metadata : "no metadata");
    } else {
        printf("❌ Failed to send detection result to 'detected': package_id=%lu, error=%d\n", 
               message->package_id, result);
    }
    
    json_object_put(detection_obj);
    rtl433_message_free(message);
}

// OOK pulse handler wrapper
static void client_ook_pulse_handler(pulse_data_t *pulse_data)
{
    full_pulse_handler(pulse_data, PULSE_DATA_OOK);
}

// FSK pulse handler wrapper
static void client_fsk_pulse_handler(pulse_data_t *pulse_data)
{
    full_pulse_handler(pulse_data, PULSE_DATA_FSK);
}

// Initialize RTL_433 configuration with pulse handlers
static int init_rtl433_config(void)
{
    // Create main config
    g_cfg = r_create_cfg();
    if (!g_cfg) {
        fprintf(stderr, "Failed to create rtl_433 config\n");
        return -1;
    }
    
    // Initialize config
    r_init_cfg(g_cfg);
    
    // Set basic parameters (matching rtl_433 defaults)
    g_cfg->center_frequency = 433920000; // 433.92 MHz (rtl_433 default)
    g_cfg->samp_rate = 250000;           // 250 kHz (rtl_433 default)
    g_cfg->verbosity = LOG_INFO;
    
    // Initialize frequency array like original rtl_433 (critical!)
    if (g_cfg->frequencies == 0) {
        g_cfg->frequency[0] = DEFAULT_FREQUENCY;
        g_cfg->frequencies = 1;
    }
    
    // Initialize device list (like original rtl_433)
    list_ensure_size(&g_cfg->demod->r_devs, 300);
    
    // Don't register protocols here - will be done after parsing arguments
    
    // Add null output to suppress normal rtl_433 output
    add_null_output(g_cfg, NULL);
    
    printf("📋 RTL_433 config initialized\n");
    
    return 0;
}

// Process signal data (real rtl_433 baseband processing)
static void process_signal_data(unsigned char *iq_buf, uint32_t len, r_cfg_t *cfg)
{
    if (!iq_buf || len == 0 || !cfg || !cfg->demod) return;
    
    struct dm_state *demod = cfg->demod;
    unsigned long n_samples;
    
    // Update statistics
    g_stats.signals_processed++;
    
    get_time_now(&demod->now);
    n_samples = len / demod->sample_size;
    
    // Age the frame position if there is one (from rtl_433 sdr_callback)
    if (demod->frame_start_ago)
        demod->frame_start_ago += n_samples;
    if (demod->frame_end_ago)
        demod->frame_end_ago += n_samples;
    if (n_samples * demod->sample_size != len) {
        printf("⚠️ Sample buffer length not aligned to sample size!\n");
    }
    if (!n_samples) {
        printf("⚠️ Sample buffer too short!\n");
        return;
    }
    
    // AM demodulation (from rtl_433)
    float avg_db;
    if (demod->sample_size == 2) { // CU8
        if (demod->use_mag_est) {
            avg_db = magnitude_est_cu8(iq_buf, demod->buf.temp, n_samples);
        } else { // amp est
            avg_db = envelope_detect(iq_buf, demod->buf.temp, n_samples);
        }
    } else { // CS16
        avg_db = magnitude_est_cs16((int16_t *)iq_buf, demod->buf.temp, n_samples);
    }
    
    // Noise level calculation
    if (demod->min_level_auto == 0.0f) {
        demod->min_level_auto = demod->min_level;
    }
    if (demod->noise_level == 0.0f) {
        demod->noise_level = demod->min_level_auto - 3.0f;
    }
    int noise_only = avg_db < demod->noise_level + 3.0f;
    // always process frames if loader, dumper, or analyzers are in use, otherwise skip silent frames
    int process_frame = demod->squelch_offset <= 0 || !noise_only || demod->load_info.format || demod->analyze_pulses || demod->dumper.len || demod->samp_grab;
    cfg->total_frames_count += 1;
    
    if (noise_only) {
        cfg->total_frames_squelch += 1;
        demod->noise_level = (demod->noise_level * 7 + avg_db) / 8; // fast fall over 8 frames
        // If auto_level and noise level well below min_level and significant change in noise level
        if (demod->auto_level > 0 && demod->noise_level < demod->min_level - 3.0f
                && fabsf(demod->min_level_auto - demod->noise_level - 3.0f) > 1.0f) {
            demod->min_level_auto = demod->noise_level + 3.0f;
            print_logf(LOG_WARNING, "Auto Level", "Estimated noise level is %.1f dB, adjusting minimum detection level to %.1f dB",
                    demod->noise_level, demod->min_level_auto);
            pulse_detect_set_levels(demod->pulse_detect, demod->use_mag_est, demod->level_limit, demod->min_level_auto, demod->min_snr, demod->detect_verbosity);
        }
    } else {
        demod->noise_level = (demod->noise_level * 31 + avg_db) / 32; // slow rise over 32 frames
    }
    
    if (process_frame) {
        baseband_low_pass_filter(&demod->lowpass_filter_state, demod->buf.temp, demod->am_buf, n_samples);
    }
    
    // FM demodulation for FSK
    unsigned fpdm = cfg->fsk_pulse_detect_mode;
    if (cfg->fsk_pulse_detect_mode == FSK_PULSE_DETECT_AUTO) {
        if (cfg->frequency[cfg->frequency_index] > FSK_PULSE_DETECTOR_LIMIT)
            fpdm = FSK_PULSE_DETECT_NEW;
        else
            fpdm = FSK_PULSE_DETECT_OLD;
    }
    
    if (demod->enable_FM_demod && process_frame) {
        float low_pass = demod->low_pass != 0.0f ? demod->low_pass : fpdm ? 0.2f : 0.1f;
        if (demod->sample_size == 2) { // CU8
            baseband_demod_FM(&demod->demod_FM_state, iq_buf, demod->buf.fm, n_samples, cfg->samp_rate, low_pass);
        } else { // CS16
            baseband_demod_FM_cs16(&demod->demod_FM_state, (int16_t *)iq_buf, demod->buf.fm, n_samples, cfg->samp_rate, low_pass);
        }
    }
    
    // Real pulse detection (core rtl_433 algorithm)
    if (process_frame) {
        int package_type = PULSE_DATA_OOK;
        
        while (package_type) {
            package_type = pulse_detect_package(demod->pulse_detect, demod->am_buf, demod->buf.fm, 
                                              n_samples, cfg->samp_rate, cfg->input_pos, 
                                              &demod->pulse_data, &demod->fsk_pulse_data, fpdm);
            
            if (package_type == PULSE_DATA_OOK) {
                // New package: set frame start if not tracking
                if (!demod->frame_start_ago)
                    demod->frame_start_ago = demod->pulse_data.start_ago;
                demod->frame_end_ago = demod->pulse_data.end_ago;
                
                calc_rssi_snr(cfg, &demod->pulse_data);
                printf("📡 Real OOK signal detected: %d pulses, RSSI=%.1f dB\n", 
                       demod->pulse_data.num_pulses, demod->pulse_data.rssi_db);
                
                // Reset device counter before decoding
                g_detected_devices_count = 0;
                
                // TEST: Direct call with proper memory management
                printf("🧪 Testing OOK decoding with proper memory management...\n");
                printf("🔍 Before decoding: %d pulses\n", demod->pulse_data.num_pulses);
                
                // Create a local copy to avoid modifying the original
                pulse_data_t ook_copy = demod->pulse_data;
                int p_events = run_ook_demods(&demod->r_devs, &ook_copy);
                
                printf("✅ OOK decoding result: %d events\n", p_events);
                cfg->total_frames_ook += 1;
                cfg->total_frames_events += p_events > 0;
                cfg->frames_ook += 1;
                cfg->frames_events += p_events > 0;

                if (cfg->verbosity >= LOG_TRACE) pulse_data_print(&demod->pulse_data);
                if (cfg->raw_mode == 1 || (cfg->raw_mode == 2 && p_events == 0) || (cfg->raw_mode == 3 && p_events > 0)) {
                    data_t *data = pulse_data_print_data(&demod->pulse_data);
                    event_occurred_handler(cfg, data);
                }
                
                printf("📡 OOK: %d pulses, %.1f dB, %d device(s)\n", 
                       demod->pulse_data.num_pulses, demod->pulse_data.rssi_db, p_events);
                
                // Send to our pulse handler (signals queue)
                client_ook_pulse_handler(&demod->pulse_data);
                
                // If devices were found, send to detected queue
                if (p_events > 0) {
                    send_detected_devices(&demod->pulse_data, p_events, "OOK");
                }
                
                // Real device detection will automatically send to 'detected' queue
                // when p_events > 0 (devices actually decoded)
                
            } else if (package_type == PULSE_DATA_FSK) {
                // New package: set frame start if not tracking  
                if (!demod->frame_start_ago)
                    demod->frame_start_ago = demod->fsk_pulse_data.start_ago;
                demod->frame_end_ago = demod->fsk_pulse_data.end_ago;
                
                // Set centerfreq_hz for proper frequency calculations
                if (demod->fsk_pulse_data.centerfreq_hz == 0) {
                    demod->fsk_pulse_data.centerfreq_hz = cfg->center_frequency ? cfg->center_frequency : 433920000;
                }
                calc_rssi_snr(cfg, &demod->fsk_pulse_data);
                printf("📡 Real FSK signal detected: %d pulses, RSSI=%.1f dB\n", 
                       demod->fsk_pulse_data.num_pulses, demod->fsk_pulse_data.rssi_db);
                printf("🔍 Original FSK data from pulse_detect: fsk_f2_est=%d, freq=%.1f Hz\n",
                       demod->fsk_pulse_data.fsk_f2_est, demod->fsk_pulse_data.centerfreq_hz);
                
                // Reset device counter before decoding
                g_detected_devices_count = 0;
                
                // TEST: Original FSK with proper cleanup like original rtl_433
                printf("🧪 Testing original FSK with proper state management...\n");
                printf("🔍 Before FSK decoding: %d pulses\n", demod->fsk_pulse_data.num_pulses);
                
                // CRITICAL: Validate pulse_data like original rtl_433
                int p_events = 0;
                if (demod->fsk_pulse_data.num_pulses == 0) {
                    printf("⚠️ No FSK pulses to decode, skipping\n");
                    p_events = 0;
                } else if (demod->fsk_pulse_data.num_pulses > PD_MAX_PULSES) {
                    printf("❌ Too many FSK pulses (%d > %d), skipping for safety\n", 
                           demod->fsk_pulse_data.num_pulses, PD_MAX_PULSES);
                    p_events = 0;
                } else {
                    // Create a safe copy and validate it
                    pulse_data_t fsk_copy = demod->fsk_pulse_data;
                    
                    // Add safety validations
                    if (fsk_copy.sample_rate == 0) fsk_copy.sample_rate = 250000;
                    // Don't force centerfreq_hz - let pulse detection set it properly
                    
                        printf("🔍 FSK validation: pulses=%d, sample_rate=%u, freq=%.1f Hz\n",
                           fsk_copy.num_pulses, fsk_copy.sample_rate, fsk_copy.centerfreq_hz);
                        printf("🔍 FSK data: fsk_f2_est=%.1f, ook_low_estimate=%d, ook_high_estimate=%d\n",
                               fsk_copy.fsk_f2_est, fsk_copy.ook_low_estimate, fsk_copy.ook_high_estimate);
                        printf("🔍 FSK first pulses: [0]=%d [1]=%d [2]=%d [3]=%d\n",
                               fsk_copy.num_pulses > 0 ? fsk_copy.pulse[0] : -1,
                               fsk_copy.num_pulses > 1 ? fsk_copy.pulse[1] : -1,
                               fsk_copy.num_pulses > 2 ? fsk_copy.pulse[2] : -1,
                               fsk_copy.num_pulses > 3 ? fsk_copy.pulse[3] : -1);
                    
                    // FINAL SOLUTION: Allow only one FSK decoding per session
                    static int fsk_decode_count = 0;
                    fsk_decode_count++;
                    if (fsk_decode_count > 1) {
                        printf("⚠️ FSK decode limit reached (%d/1), sending to server for safety\n", fsk_decode_count);
                        p_events = 0;
                    } else {
                        printf("🔧 FSK decode attempt %d/1 (others will be sent to server)...\n", fsk_decode_count);
                        
                        // Call original FSK decoder with validated data
                        printf("🔍 Calling run_fsk_demods with %d registered devices\n", (int)demod->r_devs.len);
                        for (void **iter = demod->r_devs.elems; iter && *iter; ++iter) {
                            r_device *r_dev = *iter;
                            printf("🔍 Device [%d] %s: modulation=%u, priority=%u\n", 
                                   r_dev->protocol_num, r_dev->name, r_dev->modulation, r_dev->priority);
                        }
                        p_events = run_fsk_demods(&demod->r_devs, &fsk_copy);
                        printf("✅ Original FSK result: %d events\n", p_events);
                    }
                }
                cfg->total_frames_fsk += 1;
                cfg->total_frames_events += p_events > 0;
                cfg->frames_fsk += 1;
                cfg->frames_events += p_events > 0;

                if (cfg->verbosity >= LOG_TRACE) pulse_data_print(&demod->fsk_pulse_data);
                if (cfg->raw_mode == 1 || (cfg->raw_mode == 2 && p_events == 0) || (cfg->raw_mode == 3 && p_events > 0)) {
                    data_t *data = pulse_data_print_data(&demod->fsk_pulse_data);
                    event_occurred_handler(cfg, data);
                }
                
                printf("📡 FSK: %d pulses, %.1f dB, %d device(s)\n", 
                       demod->fsk_pulse_data.num_pulses, demod->fsk_pulse_data.rssi_db, p_events);
                
                // Send to our pulse handler (signals queue)
                client_fsk_pulse_handler(&demod->fsk_pulse_data);
                
                // If devices were found, send to detected queue
                if (p_events > 0) {
                    send_detected_devices(&demod->fsk_pulse_data, p_events, "FSK");
                }
                
                // NOTE: Real FSK device detection will be sent to 'detected' queue automatically  
                // when p_events > 0 (devices actually decoded by run_fsk_demods)
                
                // CRITICAL: Reset FSK pulse counter like original rtl_433
                // This prevents all subsequent signals from being misclassified as FSK
                printf("🔧 Resetting FSK pulse counter from %d to 0\n", demod->fsk_pulse_data.num_pulses);
                pulse_data_clear(&demod->fsk_pulse_data);
                
            }
        }
    }

    // Update position for next round (like original rtl_433)
    cfg->input_pos += n_samples;
}

// Process input files (like rtl_433 does)
static int process_input_files(r_cfg_t *cfg)
{
    printf("📄 Starting file processing mode\n");
    
    // Get demod structure
    struct dm_state *demod = cfg->demod;
    
    // Store original sample rate
    uint32_t sample_rate_0 = cfg->samp_rate;
    
    // Allocate buffers for file reading
    unsigned char *test_mode_buf = malloc(DEFAULT_BUF_LENGTH * sizeof(unsigned char));
    if (!test_mode_buf) {
        fprintf(stderr, "Failed to allocate test_mode_buf\n");
        return -1;
    }
    
    float *test_mode_float_buf = malloc(DEFAULT_BUF_LENGTH / sizeof(int16_t) * sizeof(float));
    if (!test_mode_float_buf) {
        free(test_mode_buf);
        fprintf(stderr, "Failed to allocate test_mode_float_buf\n");
        return -1;
    }
    
    // Process each file
    for (void **iter = cfg->in_files.elems; iter && *iter; ++iter) {
        cfg->in_filename = *iter;
        
        printf("📂 Processing file: %s\n", cfg->in_filename);
        
        // Clear and parse file info
        printf("🔍 Before parsing: center_freq=%.0f, frequency[0]=%.0f\n", 
               cfg->center_frequency, cfg->frequency[0]);
        file_info_clear(&demod->load_info);
        file_info_parse_filename(&demod->load_info, cfg->in_filename);
        printf("🔍 File parsed: sample_rate=%u, center_freq=%.0f\n", 
               demod->load_info.sample_rate, demod->load_info.center_frequency);
        
        // Apply file info or use defaults
        cfg->samp_rate = demod->load_info.sample_rate ? demod->load_info.sample_rate : sample_rate_0;
        cfg->center_frequency = demod->load_info.center_frequency ? demod->load_info.center_frequency : cfg->frequency[0];
        
        // CRITICAL: Ensure center frequency is never 0 (fix for Toyota TPMS detection)
        if (cfg->center_frequency == 0) {
            cfg->center_frequency = DEFAULT_FREQUENCY; // 433.92 MHz
            printf("🔧 Fixed center_frequency: set to %.0f Hz\n", cfg->center_frequency);
        }
        
        printf("🔍 File info parsed: format=%s\n", file_info_string(&demod->load_info));
        printf("🔍 Applied settings: sample_rate=%u, center_freq=%.0f Hz\n", 
               cfg->samp_rate, cfg->center_frequency);
        printf("🔍 Original values: sample_rate_0=%u, frequency[0]=%.0f Hz\n", 
               sample_rate_0, cfg->frequency[0]);
        
        // Open file
        FILE *in_file;
        if (strcmp(demod->load_info.path, "-") == 0) {
            in_file = stdin;
            cfg->in_filename = "<stdin>";
        } else {
            in_file = fopen(demod->load_info.path, "rb");
            if (!in_file) {
                printf("❌ Failed to open file: %s\n", cfg->in_filename);
                continue;
            }
        }
        
        printf("✅ Reading from file: %s\n", cfg->in_filename);
        printf("   Format: %s\n", file_info_string(&demod->load_info));
        printf("   Sample rate: %u Hz\n", cfg->samp_rate);
        printf("   Center freq: %.3f MHz\n", cfg->center_frequency / 1000000.0);
        
        // Set sample size based on format
        if (demod->load_info.format == CU8_IQ || demod->load_info.format == CS8_IQ ||
            demod->load_info.format == S16_AM || demod->load_info.format == S16_FM) {
            demod->sample_size = sizeof(uint8_t) * 2;
        } else if (demod->load_info.format == CS16_IQ || demod->load_info.format == CF32_IQ) {
            demod->sample_size = sizeof(int16_t) * 2;
        } else if (demod->load_info.format == PULSE_OOK) {
            // Special case for pulse data
            printf("📡 Processing pulse data file\n");
            while (!cfg->exit_async) {
                pulse_data_load(in_file, &demod->pulse_data, cfg->samp_rate);
                if (!demod->pulse_data.num_pulses) break;
                
                // Process with our callbacks
                if (demod->pulse_data.fsk_f2_est) {
                    client_fsk_pulse_handler(&demod->pulse_data);
                } else {
                    client_ook_pulse_handler(&demod->pulse_data);
                }
            }
            if (in_file != stdin) fclose(in_file);
            continue;
        } else {
            printf("❌ Unsupported format: %s\n", file_info_string(&demod->load_info));
            if (in_file != stdin) fclose(in_file);
            continue;
        }
        
        // Read and process file data in chunks
        demod->sample_file_pos = 0.0;
        int n_blocks = 0;
        unsigned long n_read;
        
        printf("📊 Processing data chunks...\n");
        do {
            // Read data based on format
            if (demod->load_info.format == CF32_IQ) {
                n_read = fread(test_mode_float_buf, sizeof(float), DEFAULT_BUF_LENGTH / 2, in_file);
                // Convert float to int16
                for (unsigned long n = 0; n < n_read; n++) {
                    int s_tmp = test_mode_float_buf[n] * INT16_MAX;
                    if (s_tmp < -INT16_MAX) s_tmp = -INT16_MAX;
                    else if (s_tmp > INT16_MAX) s_tmp = INT16_MAX;
                    ((int16_t *)test_mode_buf)[n] = s_tmp;
                }
                n_read *= 2; // Convert to byte count
            } else {
                n_read = fread(test_mode_buf, 1, DEFAULT_BUF_LENGTH, in_file);
                
                // Convert CS8 to CU8 if needed
                if (demod->load_info.format == CS8_IQ) {
                    for (unsigned long n = 0; n < n_read; n++) {
                        test_mode_buf[n] = ((int8_t)test_mode_buf[n]) + 128;
                    }
                }
            }
            
            if (n_read == 0) break;
            
            // Update position
            demod->sample_file_pos = (float)(n_blocks * DEFAULT_BUF_LENGTH) / (cfg->samp_rate * demod->sample_size);
            
            // Process the data (custom processing similar to sdr_callback)
            process_signal_data(test_mode_buf, n_read, cfg);
            
            n_blocks++;
            if (n_blocks % 100 == 0) {
                printf("   Processed %d blocks (%.1f sec)\n", n_blocks, demod->sample_file_pos);
            }
            
        } while (n_read > 0 && !cfg->exit_async);
        
        printf("✅ File processed: %d blocks, %.2f seconds\n", n_blocks, demod->sample_file_pos);
        
        if (in_file != stdin) {
            fclose(in_file);
        }
    }
    
    free(test_mode_buf);
    free(test_mode_float_buf);
    
    printf("📁 All files processed successfully\n");
    return 0;
}

// Start SDR processing (like rtl_433 does)
static int start_sdr_processing(r_cfg_t *cfg)
{
    printf("📡 Starting SDR processing mode\n");
    
    // Setup SDR device
    int r = sdr_open(&cfg->dev, NULL, cfg->verbosity);
    if (r < 0) {
        fprintf(stderr, "Failed to open SDR device\n");
        return -1;
    }
    
    cfg->dev_info = sdr_get_dev_info(cfg->dev);
    cfg->demod->sample_size = sdr_get_sample_size(cfg->dev);
    
    // Set sample rate
    sdr_set_sample_rate(cfg->dev, cfg->samp_rate, 1);
    
    printf("📊 SDR configured:\n");
    printf("   Sample rate: %u Hz\n", cfg->samp_rate);
    printf("   Sample size: %u bytes\n", cfg->demod->sample_size);
    
    // Apply settings
    sdr_apply_settings(cfg->dev, cfg->settings_str, 1);
    sdr_set_tuner_gain(cfg->dev, cfg->gain_str, 1);
    
    if (cfg->ppm_error) {
        sdr_set_freq_correction(cfg->dev, cfg->ppm_error, 1);
    }
    
    // Reset buffers
    r = sdr_reset(cfg->dev, cfg->verbosity);
    if (r < 0) {
        printf("⚠️ Failed to reset buffers\n");
    }
    sdr_activate(cfg->dev);
    
    printf("📡 Reading samples in async mode...\n");
    sdr_set_center_freq(cfg->dev, cfg->center_frequency, 1);
    
    // Start async reading with our callback
    r = sdr_start(cfg->dev, full_sdr_callback, (void *)cfg, 
                  DEFAULT_ASYNC_BUF_NUMBER, cfg->out_block_size);
    
    if (r < 0) {
        printf("❌ Async start failed (%d)\n", r);
        return -1;
    }
    
    printf("✅ SDR started successfully\n");
    
    // Main loop - wait until exit signal (like original rtl_433)
    printf("🔄 Running main loop (Ctrl+C to stop)...\n");
    while (!exit_flag) {
        // Sleep for a short time to avoid busy waiting
        #ifdef _WIN32
            Sleep(100); // Windows: sleep 100ms
        #else
            usleep(100000); // Linux: sleep 100ms
        #endif
    }
    
    printf("📡 Stopping SDR...\n");
    sdr_stop(cfg->dev);
    
    return 0;
}

// SDR callback for full client
static void full_sdr_callback(sdr_event_t *ev, void *ctx)
{
    r_cfg_t *cfg = (r_cfg_t *)ctx;
    
    if (!ev || !ev->buf || ev->len == 0) return;
    
    // Process the SDR data using our custom processing
    process_signal_data((unsigned char *)ev->buf, ev->len, cfg);
}

// Parse command line arguments
static int parse_client_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            if (rtl433_config_set_transport_url(&g_client_config, argv[i + 1]) != 0) {
                fprintf(stderr, "Invalid transport URL: %s\n", argv[i + 1]);
                return -1;
            }
            i++; // Skip URL argument
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            if (g_cfg) {
                add_infile(g_cfg, argv[i + 1]);
                printf("📁 Added input file: %s\n", argv[i + 1]);
            }
            i++; // Skip filename argument
        } else if (strcmp(argv[i], "-v") == 0) {
            if (g_cfg) g_cfg->verbosity++;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            if (g_cfg) g_cfg->center_frequency = atoi(argv[i + 1]);
            i++; // Skip frequency argument
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            if (g_cfg) g_cfg->samp_rate = atoi(argv[i + 1]);
            i++; // Skip sample rate argument
        } else if (strcmp(argv[i], "-R") == 0) {
            // Protocol selection option (exactly like original rtl_433)
            char *arg = (i + 1 < argc) ? argv[i + 1] : NULL;
            if (!arg || strcmp(arg, "help") == 0) {
                help_protocols(g_cfg->devices, g_cfg->num_r_devices, 0);
            }
            
            int n = atoi(arg);
            if (n > g_cfg->num_r_devices || -n > g_cfg->num_r_devices) {
                fprintf(stderr, "Protocol number specified (%d) is larger than number of protocols\n\n", n);
                help_protocols(g_cfg->devices, g_cfg->num_r_devices, 1);
            }
            if ((n > 0 && g_cfg->devices[n - 1].disabled > 2) || (n < 0 && g_cfg->devices[-n - 1].disabled > 2)) {
                fprintf(stderr, "Protocol number specified (%d) is invalid\n\n", n);
                help_protocols(g_cfg->devices, g_cfg->num_r_devices, 1);
            }
            
            if (n < 0 && !g_cfg->no_default_devices) {
                register_all_protocols(g_cfg, 0); // register all defaults
            }
            g_cfg->no_default_devices = 1;
            
            if (n >= 1) {
                register_protocol(g_cfg, &g_cfg->devices[n - 1], arg_param(arg));
            }
            else if (n <= -1) {
                unregister_protocol(g_cfg, &g_cfg->devices[-n - 1]);
            }
            else {
                fprintf(stderr, "Disabling all device decoders.\n");
                list_clear(&g_cfg->demod->r_devs, (list_elem_free_fn)free_protocol);
            }
            if (arg && strcmp(arg, "help") != 0) {
                i++; // Skip protocol argument only if not "help"
            }
        } else if (strcmp(argv[i], "-X") == 0) {
            // Flex decoder option (exactly like original rtl_433)
            char *arg = (i + 1 < argc) ? argv[i + 1] : NULL;
            if (!arg) {
                flex_create_device(NULL);
            }
            
            r_device *flex_device = flex_create_device(arg);
            if (flex_device) {
                register_protocol(g_cfg, flex_device, "");
            }
            i++; // Skip flex argument
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options] [input_files...]\n", argv[0]);
            printf("Options:\n");
            printf("  -T <url>     Transport URL (default: amqp://guest:guest@localhost:5672/rtl_433)\n");
            printf("  -r <file>    Read data from input file (.cu8, .cs16, .cf32) instead of SDR\n");
            printf("  -f <freq>    Center frequency (default: 433920000)\n");
            printf("  -s <rate>    Sample rate (default: 250000)\n");
            printf("  -R <n>       Enable protocol n (1-244), use -R help to list protocols\n");
            printf("  -R -<n>      Disable protocol n\n");
            printf("  -R 0         Disable all protocols\n");
            printf("  -X <spec>    Add flex decoder (see rtl_433 docs for syntax)\n");
            printf("  -v           Increase verbosity\n");
            printf("  -h, --help   Show this help\n");
            printf("\nFile formats supported:\n");
            printf("  .cu8         Complex unsigned 8-bit (RTL-SDR format)\n");
            printf("  .cs16        Complex signed 16-bit\n");
            printf("  .cf32        Complex float 32-bit\n");
            printf("\nPositional arguments will be treated as input files.\n");
            printf("\nExamples:\n");
            printf("  %s -R 88                    # Enable only Toyota TPMS\n", argv[0]);
            printf("  %s -R help                  # List all available protocols\n", argv[0]);
            printf("  %s -R 0 -R 88 -R 110        # Enable only Toyota TPMS protocols\n", argv[0]);
            return 1;
        }
    }
    
    // Add remaining positional arguments as input files
    for (int i = 1; i < argc; i++) {
        // Skip already processed arguments
        if ((strcmp(argv[i], "-T") == 0 || strcmp(argv[i], "-r") == 0 || 
             strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc) {
            i++; // Skip option and its argument
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "-h") == 0 || 
                   strcmp(argv[i], "--help") == 0) {
            // Skip single options
        } else {
            // This is a positional argument (input file)
            if (g_cfg) {
                add_infile(g_cfg, argv[i]);
                printf("📁 Added input file: %s\n", argv[i]);
            }
        }
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    printf("RTL_433 Client (Full with Real Pulse Processing)\n");
    
    // Initialize shared config
    if (rtl433_config_init(&g_client_config, RTL433_APP_CLIENT) != 0) {
        fprintf(stderr, "Failed to initialize client config\n");
        return 1;
    }
    
    // Set default transport URL
    rtl433_config_set_transport_url(&g_client_config, "amqp://guest:guest@localhost:5672/rtl_433");
    
    // Initialize RTL_433 configuration
    if (init_rtl433_config() != 0) {
        fprintf(stderr, "Failed to initialize RTL_433 config\n");
        rtl433_config_free(&g_client_config);
        return 1;
    }
    
    // Parse command line arguments (rtl_433 style with our transport support)
    int parse_result = parse_client_args(argc, argv);
    if (parse_result != 0) {
        if (parse_result > 0) {
            // Help was shown
            r_free_cfg(g_cfg);
            rtl433_config_free(&g_client_config);
            return 0;
        } else {
            // Error occurred
            r_free_cfg(g_cfg);
            rtl433_config_free(&g_client_config);
            return 1;
        }
    }
    
    // Register default protocols if no -R options were used (like original rtl_433)
    if (!g_cfg->no_default_devices) {
        register_all_protocols(g_cfg, 0); // register all defaults
        printf("📋 Registered %zu default protocols\n", g_cfg->demod->r_devs.len);
    } else {
        printf("📋 Using custom protocol selection (%zu protocols active)\n", g_cfg->demod->r_devs.len);
    }
    
    // Check if we need FM demod (exactly like original rtl_433)
    for (void **iter = g_cfg->demod->r_devs.elems; iter && *iter; ++iter) {
        r_device *r_dev = *iter;
        if (r_dev->modulation >= FSK_DEMOD_MIN_VAL) {
            g_cfg->demod->enable_FM_demod = 1;
        }
        
        // IMPORTANT: Replace output_fn with our custom handler to capture device data
        printf("🔧 Setting output handler for protocol [%d] %s\n", r_dev->protocol_num, r_dev->name);
        r_dev->output_fn = client_device_output_handler;
        r_dev->output_ctx = g_cfg;
    }
    // if any dumpers are requested the FM demod might be needed
    if (g_cfg->demod->dumper.len) {
        g_cfg->demod->enable_FM_demod = 1;
    }
    printf("📡 FM demodulation: %s\n", g_cfg->demod->enable_FM_demod ? "enabled" : "disabled");
    
    // Set pulse detection levels (critical for proper signal detection!)
    pulse_detect_set_levels(g_cfg->demod->pulse_detect, g_cfg->demod->use_mag_est, 
                           g_cfg->demod->level_limit, g_cfg->demod->min_level, 
                           g_cfg->demod->min_snr, g_cfg->demod->detect_verbosity);
    printf("📡 Pulse detection levels set: min_level=%.1f dB, min_snr=%.1f dB\n", 
           g_cfg->demod->min_level, g_cfg->demod->min_snr);
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Connect to transport
    if (rtl433_transport_connect(&g_transport, &g_client_config.transport) != 0) {
        fprintf(stderr, "Failed to connect to transport\n");
        r_free_cfg(g_cfg);
        rtl433_config_free(&g_client_config);
        return 1;
    }
    
    printf("🔗 Connected to %s://%s:%d/%s\n",
           rtl433_transport_type_to_string(g_client_config.transport.type),
           g_client_config.transport.host,
           g_client_config.transport.port,
           g_client_config.transport.queue);
    
    // Initialize statistics
    time(&g_stats.start_time);
    
    printf("🚀 Full client ready\n");
    printf("   Center frequency: %.3f MHz\n", g_cfg->center_frequency / 1000000.0);
    printf("   Sample rate: %.3f MS/s\n", g_cfg->samp_rate / 1000000.0);
    printf("   Verbosity: %d\n", g_cfg->verbosity);
    
    int result = 0;
    
    // Check if we have input files
    if (g_cfg->in_files.len > 0) {
        printf("📁 Processing %d input file(s)...\n", (int)g_cfg->in_files.len);
        
        // Process files like rtl_433 does
        result = process_input_files(g_cfg);
    } else {
        printf("📡 Starting live SDR processing...\n");
        
        // Start SDR like rtl_433 does
        result = start_sdr_processing(g_cfg);
    }
    
    // Print final statistics
    print_statistics();
    
    // Cleanup
    printf("\n🛑 Client shutting down...\n");
    
    rtl433_transport_disconnect(&g_transport);
    rtl433_config_free(&g_client_config);
    
    if (g_cfg) {
        r_free_cfg(g_cfg);
    }
    
    printf("Client finished with result: %d\n", result);
    return result;
}
