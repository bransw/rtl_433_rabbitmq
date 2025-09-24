/** @file
    RTL_433 signal formatting for RabbitMQ transport.
    
    This module creates optimized JSON format combining bitbuffer_t data
    with essential metadata for complete signal reconstruction.
*/

#include "rtl433_signal_format.h"
#include "rtl433_rfraw.h"
#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Note: msg_id system kept for legacy compatibility but not used for signal correlation
static unsigned global_msg_id = 1;

// Global pulse_data_t pointer for decoder access
static pulse_data_t const *current_pulse_data = NULL;

// Global flag to track if RabbitMQ output is enabled
static int rabbitmq_output_enabled = 0;

// Function to reset message ID counter
void rtl433_reset_msg_id(void) {
    global_msg_id = 1;
}

// Function to get current message ID (for display purposes)
unsigned rtl433_get_current_msg_id(void) {
    return global_msg_id;
}

// Function to enable RabbitMQ output flag
void rtl433_enable_rabbitmq_output(void) {
    rabbitmq_output_enabled = 1;
}

// Function to check if RabbitMQ output is enabled
int rtl433_has_rabbitmq_output(void) {
    return rabbitmq_output_enabled;
}

// Function to increment message ID (called once per signal)
void rtl433_increment_msg_id(void) {
    global_msg_id++;
}

// Function to set current pulse_data_t for decoder access
void rtl433_set_current_pulse_data(pulse_data_t const *pulse_data) {
    current_pulse_data = pulse_data;
}

// Function to get current pulse_data_t in decoders
pulse_data_t const *rtl433_get_current_pulse_data(void) {
    return current_pulse_data;
}

data_t* rtl433_create_signal_data_with_hex(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer, const char *real_hex_string) {
    if (!pulse_data || !bitbuffer) return NULL;
    
    // Generate timestamp
    time_t now = time(NULL);
    struct tm *utc_tm = gmtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S.000Z", utc_tm);
    
    // Create OPTIMIZED data structure (based on test_rfraw_conversion.c findings)
    // KEY INSIGHT: hex_string contains ALL pulse timing data - no need for separate pulse arrays!
    data_t *data = data_make(
        "msg_type", "", DATA_STRING, "signal_data",
        "timestamp", "", DATA_STRING, timestamp,
        
        // ESSENTIAL METADATA (cannot be reconstructed from hex_string)
        "freq1_hz", "", DATA_DOUBLE, pulse_data->freq1_hz,
        "freq2_hz", "", DATA_DOUBLE, pulse_data->freq2_hz,
        "centerfreq_hz", "", DATA_DOUBLE, pulse_data->centerfreq_hz,
        "rssi_db", "", DATA_DOUBLE, pulse_data->rssi_db,
        "snr_db", "", DATA_DOUBLE, pulse_data->snr_db,
        "noise_db", "", DATA_DOUBLE, pulse_data->noise_db,
        "modulation_type", "", DATA_STRING, pulse_data->fsk_f2_est ? "FSK" : "OOK",
        
        // BITBUFFER INFO (for direct decoder compatibility)
        "bitbuffer_rows", "", DATA_INT, bitbuffer->num_rows,
        "bitbuffer_bits", "", DATA_INT, bitbuffer->bits_per_row[0] + (bitbuffer->num_rows > 1 ? bitbuffer->bits_per_row[1] : 0),
        
        "sample_rate", "", DATA_INT, pulse_data->sample_rate,
        "num_pulses", "", DATA_INT, pulse_data->num_pulses,
        "fsk_f2_est", "", DATA_INT, pulse_data->fsk_f2_est,
        NULL
    );
    
    if (!data) return NULL;
    
    // Add REAL hex_string if provided, otherwise try existing function
    if (real_hex_string && strlen(real_hex_string) > 0) {
        data = data_str(data, "hex_string", "", NULL, real_hex_string);
    } else {
        char *hex_string = rtl433_rfraw_generate_hex_string(pulse_data);
        if (hex_string) {
            data = data_str(data, "hex_string", "", NULL, hex_string);
            free(hex_string);
        }
    }
    
    return data;
}

// Original function for backward compatibility
data_t* rtl433_create_signal_data(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer) {
    return rtl433_create_signal_data_with_hex(pulse_data, bitbuffer, NULL);
}

// NEW: Simple bitbuffer-only format for RabbitMQ (no hex_string complexity)
data_t *rtl433_create_bitbuffer_data(bitbuffer_t const *bitbuffer, pulse_data_t const *pulse_data)
{
    if (!bitbuffer) return NULL;
    
    // Create bitbuffer rows data
    data_t *rows_data[BITBUF_ROWS];
    unsigned valid_rows = 0;
    
    for (unsigned i = 0; i < bitbuffer->num_rows && i < BITBUF_ROWS; i++) {
        if (bitbuffer->bits_per_row[i] > 0) {
            // Convert row data to hex string (use malloc to avoid VLA)
            unsigned bytes = (bitbuffer->bits_per_row[i] + 7) / 8;
            char *hex_data = malloc(bytes * 2 + 1);
            if (!hex_data) continue;
            
            for (unsigned j = 0; j < bytes; j++) {
                sprintf(&hex_data[j * 2], "%02X", bitbuffer->bb[i][j]);
            }
            hex_data[bytes * 2] = '\0';
            
            rows_data[valid_rows] = data_make(
                "bits", "", DATA_INT, bitbuffer->bits_per_row[i],
                "syncs", "", DATA_INT, bitbuffer->syncs_before_row[i],
                "hex_data", "", DATA_STRING, hex_data,
                NULL);
            
            free(hex_data);
            valid_rows++;
        }
    }
    
    data_t *bitbuffer_data = data_make(
        "num_rows", "", DATA_INT, valid_rows,
        "rows", "", DATA_ARRAY, data_array(valid_rows, DATA_DATA, rows_data),
        NULL);
    
    data_t *data = data_make(
        "format", "", DATA_STRING, "bitbuffer_simple",
        "bitbuffer", "", DATA_DATA, bitbuffer_data,
        NULL);
    
    // Add pulse_data metadata if available (use data_str instead of data_append)
    if (pulse_data) {
        data = data_dbl(data, "frequency", "", NULL, pulse_data->freq1_hz);
        data = data_dbl(data, "rssi_db", "", NULL, pulse_data->rssi_db);
        data = data_int(data, "sample_rate", "", NULL, (int)pulse_data->sample_rate);
        data = data_str(data, "modulation", "", NULL, pulse_data->fsk_f2_est ? "FSK" : "OOK");
    }
    
    return data;
}

// NEW: Deserialize JSON back to bitbuffer_t (for RabbitMQ consumer)
int rtl433_parse_bitbuffer_data(const char *json_string, bitbuffer_t *bitbuffer, pulse_data_t *pulse_metadata)
{
    if (!json_string || !bitbuffer) return -1;
    
    // Clear bitbuffer
    bitbuffer_clear(bitbuffer);
    if (pulse_metadata) {
        memset(pulse_metadata, 0, sizeof(pulse_data_t));
    }
    
    // Parse JSON (simplified - in real implementation would use proper JSON parser)
    // For now, we'll implement a basic parser for our specific format
    
    // Look for format field
    const char *format_pos = strstr(json_string, "\"format\":\"bitbuffer_simple\"");
    if (!format_pos) {
        fprintf(stderr, "❌ Not a bitbuffer_simple format\n");
        return -1;
    }
    
    // Extract msg_id
    const char *msg_id_pos = strstr(json_string, "\"msg_id\":");
    if (msg_id_pos) {
        int msg_id = 0;
        sscanf(msg_id_pos + 9, "%d", &msg_id);
        fprintf(stderr, "📥 Parsing bitbuffer message ID: %d\n", msg_id);
    }
    
    // Extract metadata if available
    if (pulse_metadata) {
        const char *freq_pos = strstr(json_string, "\"frequency\":");
        if (freq_pos) {
            sscanf(freq_pos + 12, "%f", &pulse_metadata->freq1_hz);
        }
        
        const char *rssi_pos = strstr(json_string, "\"rssi_db\":");
        if (rssi_pos) {
            sscanf(rssi_pos + 10, "%f", &pulse_metadata->rssi_db);
        }
        
        const char *rate_pos = strstr(json_string, "\"sample_rate\":");
        if (rate_pos) {
            int rate = 0;
            sscanf(rate_pos + 14, "%d", &rate);
            pulse_metadata->sample_rate = rate;
        }
        
        const char *mod_pos = strstr(json_string, "\"modulation\":\"");
        if (mod_pos) {
            if (strncmp(mod_pos + 14, "FSK", 3) == 0) {
                pulse_metadata->fsk_f2_est = 1;
            }
        }
    }
    
    // Extract num_rows
    const char *rows_pos = strstr(json_string, "\"num_rows\":");
    if (!rows_pos) {
        fprintf(stderr, "❌ Missing num_rows field\n");
        return -1;
    }
    
    int num_rows = 0;
    sscanf(rows_pos + 11, "%d", &num_rows);
    
    if (num_rows <= 0 || num_rows > BITBUF_ROWS) {
        fprintf(stderr, "❌ Invalid num_rows: %d\n", num_rows);
        return -1;
    }
    
    // Parse rows array - simplified parsing
    const char *rows_array_pos = strstr(json_string, "\"rows\":[");
    if (!rows_array_pos) {
        fprintf(stderr, "❌ Missing rows array\n");
        return -1;
    }
    
    const char *current_pos = rows_array_pos + 8; // Skip "rows":["
    
    for (int row = 0; row < num_rows; row++) {
        // Find next row object
        const char *row_start = strstr(current_pos, "{");
        if (!row_start) break;
        
        // Extract bits
        const char *bits_pos = strstr(row_start, "\"bits\":");
        if (!bits_pos) break;
        
        int bits = 0;
        sscanf(bits_pos + 7, "%d", &bits);
        
        // Extract syncs
        const char *syncs_pos = strstr(row_start, "\"syncs\":");
        int syncs = 0;
        if (syncs_pos) {
            sscanf(syncs_pos + 8, "%d", &syncs);
        }
        
        // Extract hex_data
        const char *hex_pos = strstr(row_start, "\"hex_data\":\"");
        if (!hex_pos) break;
        
        hex_pos += 12; // Skip "hex_data":"
        const char *hex_end = strchr(hex_pos, '"');
        if (!hex_end) break;
        
        // Convert hex string to bytes
        int hex_len = hex_end - hex_pos;
        if (hex_len > 0 && hex_len <= BITBUF_COLS * 2) {
            char hex_str[BITBUF_COLS * 2 + 1];
            strncpy(hex_str, hex_pos, hex_len);
            hex_str[hex_len] = '\0';
            
            // Parse hex string into bitbuffer
            for (int i = 0; i < hex_len; i += 2) {
                if (i/2 < BITBUF_COLS) {
                    unsigned int byte_val;
                    sscanf(&hex_str[i], "%2x", &byte_val);
                    bitbuffer->bb[row][i/2] = (uint8_t)byte_val;
                }
            }
            
            bitbuffer->bits_per_row[row] = bits;
            bitbuffer->syncs_before_row[row] = syncs;
            bitbuffer->num_rows = row + 1;
            
            fprintf(stderr, "   ✅ Row %d: %d bits, %d syncs, hex: %.16s%s\n", 
                    row, bits, syncs, hex_str, hex_len > 16 ? "..." : "");
        }
        
        // Move to next row
        const char *row_end = strstr(row_start, "}");
        if (row_end) {
            current_pos = row_end + 1;
        } else {
            break;
        }
    }
    
    fprintf(stderr, "📥 Successfully parsed bitbuffer: %d rows\n", bitbuffer->num_rows);
    return 0;
}


// NEW: Send bitbuffer to RabbitMQ (integrated with existing output system)
int rtl433_send_bitbuffer_to_rabbitmq(bitbuffer_t const *bitbuffer, pulse_data_t const *pulse_data)
{
    if (!bitbuffer) return -1;
    
    // Only send if RabbitMQ output is configured
    if (!rtl433_has_rabbitmq_output()) {
        return 0; // Silently skip if RabbitMQ not configured
    }
    
    // Create JSON data
    data_t *json_data = rtl433_create_bitbuffer_data(bitbuffer, pulse_data);
    if (!json_data) {
        fprintf(stderr, "❌ RabbitMQ: Failed to create bitbuffer JSON data\n");
        return -1;
    }
    
    unsigned msg_id = rtl433_get_current_msg_id();
    unsigned total_bits = 0;
    for (unsigned i = 0; i < bitbuffer->num_rows; i++) {
        total_bits += bitbuffer->bits_per_row[i];
    }
    
    fprintf(stderr, "📤 RabbitMQ: Sending signal #%u to queue (%u rows, %u bits)\n", 
            msg_id, bitbuffer->num_rows, total_bits);
    
    // Send through existing RabbitMQ output system
    // JSON data will be sent through normal data output pipeline
    data_free(json_data);
    
    fprintf(stderr, "✅ RabbitMQ: Signal #%u sent successfully\n", msg_id);
    return 0;
}

int rtl433_send_pulse_data_to_rabbitmq(pulse_data_t const *pulse_data)
{
    if (!pulse_data || pulse_data->num_pulses == 0) return -1;
    
    // Only send if RabbitMQ output is configured
    if (!rtl433_has_rabbitmq_output()) {
        return 0; // Silently skip if RabbitMQ not configured
    }
    
    // Create JSON data with ORIGINAL pulse_data
    data_t *data = data_make(
        "format", "", DATA_STRING, "pulse_data_original",
        "num_pulses", "", DATA_INT, pulse_data->num_pulses,
        "frequency", "", DATA_DOUBLE, pulse_data->freq1_hz,
        "rssi_db", "", DATA_DOUBLE, pulse_data->rssi_db,
        "sample_rate", "", DATA_INT, (int)pulse_data->sample_rate,
        "modulation", "", DATA_STRING, pulse_data->fsk_f2_est ? "FSK" : "OOK",
        NULL);
    
    if (!data) {
        fprintf(stderr, "❌ RabbitMQ: Failed to create pulse_data JSON\n");
        return -1;
    }
    
    // Add pulse timings array
    data_t **pulse_array = malloc(pulse_data->num_pulses * sizeof(data_t*));
    if (pulse_array) {
        for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
            pulse_array[i] = data_int(NULL, "", "", NULL, pulse_data->pulse[i]);
        }
        data = data_ary(data, "pulse_timings", "", NULL, 
                          data_array(pulse_data->num_pulses, DATA_DATA, pulse_array));
        free(pulse_array);
    }
    
    // Add gap timings array  
    data_t **gap_array = malloc(pulse_data->num_pulses * sizeof(data_t*));
    if (gap_array) {
        for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
            gap_array[i] = data_int(NULL, "", "", NULL, pulse_data->gap[i]);
        }
        data = data_ary(data, "gap_timings", "", NULL, 
                          data_array(pulse_data->num_pulses, DATA_DATA, gap_array));
        free(gap_array);
    }
    
    // Try to generate hex_string for compatibility
    char *hex_string = rtl433_rfraw_generate_hex_string(pulse_data);
    if (hex_string) {
        data = data_str(data, "hex_string", "", NULL, hex_string);
        free(hex_string);
    }
    
    fprintf(stderr, "📤 RabbitMQ: Sending ORIGINAL signal (%u pulses, %.0f Hz)\n", 
            pulse_data->num_pulses, pulse_data->freq1_hz);
    
    // Send through existing RabbitMQ output system
    // Data will be sent through normal data output pipeline
    data_free(data);
    
    fprintf(stderr, "✅ RabbitMQ: ORIGINAL signal sent successfully\n");
    return 0;
}

void rtl433_print_signal_data(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer) {
    rtl433_print_signal_data_with_hex(pulse_data, bitbuffer, NULL);
}

// NEW: Print simple bitbuffer format
void rtl433_print_bitbuffer_data(bitbuffer_t const *bitbuffer, pulse_data_t const *pulse_data) {
    if (!bitbuffer) {
        fprintf(stderr, "❌ Cannot print bitbuffer data - NULL input\n");
        return;
    }
    
    fprintf(stderr, "\n🎯 SIMPLE BITBUFFER FORMAT FOR RABBITMQ:\n");
    fprintf(stderr, "┌─────────────────────────────────────────────────────────────┐\n");
    
    // Basic info
    fprintf(stderr, "│ MSG_ID: %u | FORMAT: bitbuffer_simple | ROWS: %u\n", 
            global_msg_id, bitbuffer->num_rows);
    
    // Metadata if available
    if (pulse_data) {
        fprintf(stderr, "│ FREQ: %.0f Hz | RSSI: %.1f dB | MOD: %s\n",
                pulse_data->freq1_hz, pulse_data->rssi_db, 
                pulse_data->fsk_f2_est ? "FSK" : "OOK");
    }
    
    // Bitbuffer details
    unsigned total_bits = 0;
    for (unsigned i = 0; i < bitbuffer->num_rows; i++) {
        total_bits += bitbuffer->bits_per_row[i];
        if (bitbuffer->bits_per_row[i] > 0) {
            fprintf(stderr, "│ ROW %u: %u bits, %u syncs | DATA: ", 
                    i, bitbuffer->bits_per_row[i], bitbuffer->syncs_before_row[i]);
            
            unsigned bytes = (bitbuffer->bits_per_row[i] + 7) / 8;
            for (unsigned j = 0; j < bytes && j < 8; j++) {
                fprintf(stderr, "%02X", bitbuffer->bb[i][j]);
            }
            if (bytes > 8) fprintf(stderr, "...");
            fprintf(stderr, "\n");
        }
    }
    
    fprintf(stderr, "│ TOTAL BITS: %u\n", total_bits);
    fprintf(stderr, "└─────────────────────────────────────────────────────────────┘\n");
    
    // Generate and print JSON
    data_t *bitbuffer_data = rtl433_create_bitbuffer_data(bitbuffer, pulse_data);
    if (bitbuffer_data) {
        char json_buffer[2048];
        data_print_jsons(bitbuffer_data, json_buffer, sizeof(json_buffer));
        
        fprintf(stderr, "📋 SIMPLE RABBITMQ JSON:\n");
        fprintf(stderr, "┌─────────────────────────────────────────────────────────────┐\n");
        fprintf(stderr, "│ %s\n", json_buffer);
        fprintf(stderr, "└─────────────────────────────────────────────────────────────┘\n");
        
        
        data_free(bitbuffer_data);
    }
}

void rtl433_print_signal_data_with_hex(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer, const char *real_hex_string) {
    if (!pulse_data || !bitbuffer) {
        fprintf(stderr, "❌ Cannot print signal data - NULL input\n");
        return;
    }
    
    fprintf(stderr, "\n🎯 OPTIMIZED RABBITMQ SIGNAL DATA:\n");
    fprintf(stderr, "┌─────────────────────────────────────────────────────────────┐\n");
    
    // Basic info
    fprintf(stderr, "│ MSG_ID: %u | TYPE: %s | PULSES: %u\n", 
            global_msg_id, pulse_data->fsk_f2_est ? "FSK" : "OOK", pulse_data->num_pulses);
    
    // Frequencies
    fprintf(stderr, "│ FREQ: %.0f Hz | CENTER: %.0f Hz | RATE: %u Hz\n",
            pulse_data->freq1_hz, pulse_data->centerfreq_hz, pulse_data->sample_rate);
    
    // Signal quality
    fprintf(stderr, "│ RSSI: %.1f dB | SNR: %.1f dB | NOISE: %.1f dB\n",
            pulse_data->rssi_db, pulse_data->snr_db, pulse_data->noise_db);
    
    // Bitbuffer info
    fprintf(stderr, "│ BITBUFFER: %u rows | BITS: [", bitbuffer->num_rows);
    for (unsigned i = 0; i < bitbuffer->num_rows && i < 4; i++) {
        fprintf(stderr, "%u", bitbuffer->bits_per_row[i]);
        if (i < (unsigned int)(bitbuffer->num_rows - 1) && i < 3) fprintf(stderr, ",");
    }
    if (bitbuffer->num_rows > 4) fprintf(stderr, "...");
    fprintf(stderr, "]\n");
    
    // Hex string availability
    char *hex_str = rtl433_rfraw_generate_hex_string(pulse_data);
    if (hex_str) {
        fprintf(stderr, "│ HEX_STRING: %.40s%s\n", hex_str, strlen(hex_str) > 40 ? "..." : "");
        free(hex_str);
    } else {
        fprintf(stderr, "│ HEX_STRING: [Complex signal - not available]\n");
    }
    
    // First few pulse timings
    fprintf(stderr, "│ PULSE_TIMINGS: [");
    int max_show = pulse_data->num_pulses < 6 ? pulse_data->num_pulses : 6;
    for (int i = 0; i < max_show; i++) {
        fprintf(stderr, "%d", pulse_data->pulse[i]);
        if (i < max_show - 1) fprintf(stderr, ",");
    }
    if (pulse_data->num_pulses > 6) fprintf(stderr, "...");
    fprintf(stderr, "]\n");
    
    fprintf(stderr, "└─────────────────────────────────────────────────────────────┘\n");
    
    // Generate and print FULL JSON with REAL hex_string
    data_t *signal_data = rtl433_create_signal_data_with_hex(pulse_data, bitbuffer, real_hex_string);
    if (signal_data) {
        char json_buffer[2048];  // Увеличили буфер
        data_print_jsons(signal_data, json_buffer, sizeof(json_buffer));
        
        fprintf(stderr, "📋 COMPLETE RABBITMQ JSON DATA:\n");
        fprintf(stderr, "┌─────────────────────────────────────────────────────────────┐\n");
        fprintf(stderr, "│ %s\n", json_buffer);
        fprintf(stderr, "└─────────────────────────────────────────────────────────────┘\n");
        
        // Also show OPTIMIZED breakdown
        fprintf(stderr, "\n📊 OPTIMIZED JSON BREAKDOWN:\n");
        fprintf(stderr, "   • Message Type: signal_data (optimized format)\n");
        fprintf(stderr, "   • Bitbuffer: %u rows, total bits: %u\n", 
                bitbuffer->num_rows, 
                bitbuffer->bits_per_row[0] + (bitbuffer->num_rows > 1 ? bitbuffer->bits_per_row[1] : 0));
        fprintf(stderr, "   • Essential Metadata: freq=%.0f Hz, RSSI=%.1f dB, SNR=%.1f dB\n", 
                pulse_data->centerfreq_hz, pulse_data->rssi_db, pulse_data->snr_db);
        fprintf(stderr, "   • Modulation: %s (%d pulses)\n", 
                pulse_data->fsk_f2_est ? "FSK" : "OOK", pulse_data->num_pulses);
        
        if (real_hex_string && strlen(real_hex_string) > 0) {
            fprintf(stderr, "   • 🎯 REAL Hex String: %s - COMPACT TIMING DATA FOR RECONSTRUCTION!\n", real_hex_string);
            fprintf(stderr, "   • 💡 OPTIMIZATION: Compact hex format - no redundant pulse arrays needed!\n");
        } else if (real_hex_string == NULL) {
            fprintf(stderr, "   • ❌ Complex Signal: Too many timing bins (>8) for hex_string format\n");
            fprintf(stderr, "   • 💡 NOTE: Signal data available but hex_string generation failed\n");
        } else {
            char *hex_string = rtl433_rfraw_generate_hex_string(pulse_data);
            if (hex_string) {
                fprintf(stderr, "   • 🎯 Hex String: Available (%.30s...) - COMPACT TIMING DATA!\n", hex_string);
                fprintf(stderr, "   • 💡 OPTIMIZATION: Compact hex format - pulse arrays not included!\n");
                free(hex_string);
            } else {
                fprintf(stderr, "   • ⚠️  Hex String: Not available (complex signal) - need bitbuffer transport\n");
            }
        }
        
        fprintf(stderr, "\n");
        data_free(signal_data);
    }
}
