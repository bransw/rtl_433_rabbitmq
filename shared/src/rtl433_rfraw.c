/** @file
 * RFRAW conversion and signal reconstruction utilities for rtl_433
 */

#include "rtl433_rfraw.h"
#include "pulse_data.h"
#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <limits.h>

// === HISTOGRAM IMPLEMENTATION (copied from pulse_analyzer.c) ===

#define MAX_HIST_BINS 16
#define TOLERANCE (0.2f) // 20% tolerance should still discern between the pulse widths: 0.33, 0.66, 1.0
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/// Histogram data for single bin
typedef struct {
    unsigned count;
    int sum;
    int mean;
    int min;
    int max;
} hist_bin_t;

/// Histogram data for all bins
typedef struct {
    unsigned bins_count;
    hist_bin_t bins[MAX_HIST_BINS];
} histogram_t;

/// Delete bin from histogram
static void histogram_delete_bin(histogram_t *hist, unsigned index)
{
    hist_bin_t const zerobin = {0};
    if (hist->bins_count < 1) return;    // Avoid out of bounds
    // Move all bins afterwards one forward
    for (unsigned n = index; n < hist->bins_count-1; ++n) {
        hist->bins[n] = hist->bins[n+1];
    }
    hist->bins_count--;
    hist->bins[hist->bins_count] = zerobin;    // Clear previously last bin
}

/// Generate a histogram (unsorted)
static void histogram_sum(histogram_t *hist, int const *data, unsigned len, float tolerance)
{
    unsigned bin;    // Iterator will be used outside for!

    for (unsigned n = 0; n < len; ++n) {
        // Search for match in existing bins
        for (bin = 0; bin < hist->bins_count; ++bin) {
            int bn = data[n];
            int bm = hist->bins[bin].mean;
            if (abs(bn - bm) < (tolerance * MAX(bn, bm))) {
                hist->bins[bin].count++;
                hist->bins[bin].sum += data[n];
                hist->bins[bin].mean = hist->bins[bin].sum / hist->bins[bin].count;
                hist->bins[bin].min    = MIN(data[n], hist->bins[bin].min);
                hist->bins[bin].max    = MAX(data[n], hist->bins[bin].max);
                break;    // Match found! Data added to existing bin
            }
        }
        // No match found? Add new bin
        if (bin == hist->bins_count && bin < MAX_HIST_BINS) {
            hist->bins[bin].count    = 1;
            hist->bins[bin].sum        = data[n];
            hist->bins[bin].mean    = data[n];
            hist->bins[bin].min        = data[n];
            hist->bins[bin].max        = data[n];
            hist->bins_count++;
        } // for bin
    } // for data
}

/// Fuse histogram bins with means within tolerance
static void histogram_fuse_bins(histogram_t *hist, float tolerance)
{
    if (hist->bins_count < 2) return;        // Avoid underflow
    // Compare all bins
    for (unsigned n = 0; n < hist->bins_count-1; ++n) {
        for (unsigned m = n+1; m < hist->bins_count; ++m) {
            int bn = hist->bins[n].mean;
            int bm = hist->bins[m].mean;
            // if within tolerance
            if (abs(bn - bm) < (tolerance * MAX(bn, bm))) {
                // Fuse data for bin[n] and bin[m]
                hist->bins[n].count += hist->bins[m].count;
                hist->bins[n].sum    += hist->bins[m].sum;
                hist->bins[n].mean    = hist->bins[n].sum / hist->bins[n].count;
                hist->bins[n].min    = MIN(hist->bins[n].min, hist->bins[m].min);
                hist->bins[n].max    = MAX(hist->bins[n].max, hist->bins[m].max);
                // Delete bin[m]
                histogram_delete_bin(hist, m);
                m--;    // Compare new bin in same place!
            }
        }
    }
}

/// Find bin index
static int histogram_find_bin_index(histogram_t const *hist, int width)
{
    for (unsigned n = 0; n < hist->bins_count; ++n) {
        if (hist->bins[n].min <= width && width <= hist->bins[n].max) {
            return n;
        }
    }
    return -1;
}

// === HEX STRING BUILDER (copied from pulse_analyzer.c) ===

#define HEXSTR_BUILDER_SIZE 1024
#define HEXSTR_MAX_COUNT 32
#define USHRT_MAX 65535

/// Hex string builder
typedef struct hexstr {
    uint8_t p[HEXSTR_BUILDER_SIZE];
    unsigned idx;
} hexstr_t;

static void hexstr_push_byte(hexstr_t *h, uint8_t v)
{
    if (h->idx < HEXSTR_BUILDER_SIZE)
        h->p[h->idx++] = v;
}

static void hexstr_push_word(hexstr_t *h, uint16_t v)
{
    if (h->idx + 1 < HEXSTR_BUILDER_SIZE) {
        h->p[h->idx++] = v >> 8;
        h->p[h->idx++] = v & 0xff;
    }
}

static char* hexstr_to_string(hexstr_t *h)
{
    char *result = malloc(h->idx * 2 + 1);
    if (!result) return NULL;
    
    for (unsigned i = 0; i < h->idx; ++i) {
        sprintf(result + i * 2, "%02X", h->p[i]);
    }
    result[h->idx * 2] = '\0';
    return result;
}

// === PUBLIC FUNCTIONS ===

char* rtl433_rfraw_generate_hex_string(pulse_data_t const *data)
{
    if (!data || data->num_pulses == 0) {
        return NULL;
    }
    
    // Create histograms for timing analysis (copied from pulse_analyzer.c)
    histogram_t hist_timings = {0};
    histogram_t hist_gaps = {0};
    double to_us = 1e6 / data->sample_rate;
    
    // Build arrays for histogram analysis
    int timings[2 * PD_MAX_PULSES];
    int gaps_only[PD_MAX_PULSES];
    unsigned timings_count = 0;
    unsigned gaps_count = 0;
    
    for (unsigned i = 0; i < data->num_pulses; ++i) {
        if (data->pulse[i] > 0) {
            timings[timings_count++] = data->pulse[i];
        }
        if (data->gap[i] > 0) {
            timings[timings_count++] = data->gap[i];
            gaps_only[gaps_count++] = data->gap[i];
        }
    }
    
    // Generate histograms using histogram_sum (like pulse_analyzer.c)
    histogram_sum(&hist_timings, timings, timings_count, TOLERANCE);
    histogram_sum(&hist_gaps, gaps_only, gaps_count, TOLERANCE);
    
    // Fuse overlapping bins (like pulse_analyzer.c)
    histogram_fuse_bins(&hist_timings, TOLERANCE);
    histogram_fuse_bins(&hist_gaps, TOLERANCE);
    
    if (hist_timings.bins_count == 0 || hist_timings.bins_count > 8) {
        return NULL;
    }
    
    char *result = NULL;
    
    // Generate hex string (logic from pulse_analyzer.c)
    if (hist_gaps.bins_count <= 2) {
        // B1 format - single long code
        hexstr_t hexstr = {.p = {0}};
        hexstr_push_byte(&hexstr, 0xaa);
        hexstr_push_byte(&hexstr, 0xb1);
        hexstr_push_byte(&hexstr, hist_timings.bins_count);
        
        for (unsigned b = 0; b < hist_timings.bins_count; ++b) {
            double w = hist_timings.bins[b].mean * to_us;
            hexstr_push_word(&hexstr, w < USHRT_MAX ? w : USHRT_MAX);
        }
        
        for (unsigned i = 0; i < data->num_pulses; ++i) {
            int p = histogram_find_bin_index(&hist_timings, data->pulse[i]);
            int g = histogram_find_bin_index(&hist_timings, data->gap[i]);
            if (p >= 0 && g >= 0) {
                hexstr_push_byte(&hexstr, 0x80 | (p << 4) | g);
            }
        }
        hexstr_push_byte(&hexstr, 0x55);
        
        result = hexstr_to_string(&hexstr);
    }
    else {
        // B0 format - multiple codes with gap separation
        int limit_bin = MIN(3, hist_gaps.bins_count - 1);
        int limit = hist_gaps.bins[limit_bin].min;
        hexstr_t hexstrs[HEXSTR_MAX_COUNT] = {{.p = {0}}};
        unsigned hexstr_cnt = 0;
        unsigned i = 0;
        
        while (i < data->num_pulses && hexstr_cnt < HEXSTR_MAX_COUNT) {
            hexstr_t *hexstr = &hexstrs[hexstr_cnt];
            hexstr_push_byte(hexstr, 0xaa);
            hexstr_push_byte(hexstr, 0xb0);
            hexstr_push_byte(hexstr, 0); // len placeholder
            hexstr_push_byte(hexstr, hist_timings.bins_count);
            hexstr_push_byte(hexstr, 1); // repeats
            
            for (unsigned b = 0; b < hist_timings.bins_count; ++b) {
                double w = hist_timings.bins[b].mean * to_us;
                hexstr_push_word(hexstr, w < USHRT_MAX ? w : USHRT_MAX);
            }
            
            for (; i < data->num_pulses; ++i) {
                int p = histogram_find_bin_index(&hist_timings, data->pulse[i]);
                int g = histogram_find_bin_index(&hist_timings, data->gap[i]);
                if (p >= 0 && g >= 0) {
                    hexstr_push_byte(hexstr, 0x80 | (p << 4) | g);
                    if (data->gap[i] >= limit) {
                        ++i;
                        break;
                    }
                }
            }
            hexstr_push_byte(hexstr, 0x55);
            hexstr->p[2] = hexstr->idx - 4 <= 255 ? hexstr->idx - 4 : 0; // len
            
            if (hexstr_cnt > 0 && hexstrs[hexstr_cnt - 1].idx == hexstr->idx
                    && !memcmp(&hexstrs[hexstr_cnt - 1].p[5], &hexstr->p[5], hexstr->idx - 5)) {
                hexstr->idx = 0; // clear
                hexstrs[hexstr_cnt - 1].p[4] += 1; // repeats
            } else {
                hexstr_cnt++;
            }
        }
        
        // Combine all hex strings with + separator
        size_t total_len = 0;
        for (unsigned j = 0; j < hexstr_cnt; ++j) {
            total_len += hexstrs[j].idx * 2;
            if (j > 0) total_len += 1; // for '+'
        }
        total_len += 1; // null terminator
        
        result = malloc(total_len);
        if (result) {
            result[0] = '\0';
            for (unsigned j = 0; j < hexstr_cnt; ++j) {
                if (j > 0) strcat(result, "+");
                char *hex_part = hexstr_to_string(&hexstrs[j]);
                if (hex_part) {
                    strcat(result, hex_part);
                    free(hex_part);
                }
            }
        }
    }
    
    return result;
}

char* rtl433_rfraw_generate_triq_url(pulse_data_t const *data)
{
    char *hex_string = rtl433_rfraw_generate_hex_string(data);
    if (!hex_string) {
        return NULL;
    }
    
    char *url = malloc(strlen(hex_string) + 32);
    if (!url) {
        free(hex_string);
        return NULL;
    }
    
    strcpy(url, "https://triq.org/pdv/#");
    strcat(url, hex_string);
    
    free(hex_string);
    return url;
}

char* rtl433_rfraw_pulse_data_to_enhanced_json(pulse_data_t const *data)
{
    if (!data) return NULL;
    
    // DEBUG: Test hex_string generation
    char *hex_string = rtl433_rfraw_generate_hex_string(data);
    char *triq_url = rtl433_rfraw_generate_triq_url(data);
    
    fprintf(stderr, "🔧 DEBUG rtl433_rfraw_pulse_data_to_enhanced_json: pulses=%u\n", data->num_pulses);
    fprintf(stderr, "    hex_string: %s\n", hex_string ? hex_string : "NULL");
    fprintf(stderr, "    triq_url: %s\n", triq_url ? triq_url : "NULL");
    
    if (hex_string) free(hex_string);
    if (triq_url) free(triq_url);
    
    // Use the enhanced pulse_data_print_data function (now includes all reconstruction fields)
    data_t *pulse_data_obj = pulse_data_print_data(data);
    if (!pulse_data_obj) {
        return NULL;
    }
    
    // Get required buffer size first
    size_t required_size = data_print_jsons(pulse_data_obj, NULL, 0);
    if (required_size == 0) {
        data_free(pulse_data_obj);
        return NULL;
    }
    
    // Allocate buffer and generate JSON
    char *json_str = malloc(required_size + 1);
    if (!json_str) {
        data_free(pulse_data_obj);
        return NULL;
    }
    
    data_print_jsons(pulse_data_obj, json_str, required_size + 1);
    data_free(pulse_data_obj);
    
    return json_str;
}

int rtl433_rfraw_parse_hex_string(const char *hex_string, pulse_data_t *pulse_data)
{
    if (!hex_string || !pulse_data) {
        return -1;
    }
    
    // Clear pulse_data
    memset(pulse_data, 0, sizeof(pulse_data_t));
    
    // Parse hex string format: AABBCCDDEEFF...
    // AA = header, BB = modulation info, CC = length, DD = timings count, EE... = timing histogram
    
    size_t hex_len = strlen(hex_string);
    if (hex_len < 8) return -1; // Minimum header size
    
    // Parse header (AA/AB/etc.)
    char header_str[3] = {hex_string[0], hex_string[1], '\0'};
    unsigned int header = strtoul(header_str, NULL, 16);
    if ((header & 0xF0) != 0xA0) return -1; // Invalid header family
    
    // Parse modulation info (BB) 
    char mod_str[3] = {hex_string[2], hex_string[3], '\0'};
    unsigned int mod_info = strtoul(mod_str, NULL, 16);
    
    // Extract timing info from modulation byte
    pulse_data->sample_rate = 250000; // Default sample rate
    
    // Parse length (CC)
    char len_str[3] = {hex_string[4], hex_string[5], '\0'};
    unsigned int payload_len = strtoul(len_str, NULL, 16);
    
    // Parse timing histogram count (DD)
    char timing_count_str[3] = {hex_string[6], hex_string[7], '\0'};
    unsigned int timing_count = strtoul(timing_count_str, NULL, 16);
    
    if (timing_count > 8) return -1; // Too many timing bins
    
    // Parse timing histogram values
    unsigned int timings[8];
    for (unsigned int i = 0; i < timing_count && i < 8; i++) {
        if (hex_len < 8 + (i + 1) * 4) return -1;
        char timing_str[5] = {
            hex_string[8 + i * 4],
            hex_string[8 + i * 4 + 1], 
            hex_string[8 + i * 4 + 2],
            hex_string[8 + i * 4 + 3],
            '\0'
        };
        timings[i] = strtoul(timing_str, NULL, 16);
    }
    
    // Parse pulse/gap data after timing histogram
    unsigned int data_start = 8 + timing_count * 4;
    unsigned int pulse_count = 0;
    
    for (unsigned int i = data_start; i < hex_len && pulse_count < PD_MAX_PULSES; i += 2) {
        if (i + 1 >= hex_len) break;
        
        char data_byte_str[3] = {hex_string[i], hex_string[i + 1], '\0'};
        unsigned int data_byte = strtoul(data_byte_str, NULL, 16);
        
        if (data_byte == 0x55) break; // End marker
        
        if (data_byte & 0x80) { // Pulse/gap pair
            unsigned int pulse_idx = (data_byte >> 4) & 0x07;
            unsigned int gap_idx = data_byte & 0x0F;
            
            if (pulse_idx < timing_count && gap_idx < timing_count) {
                pulse_data->pulse[pulse_count] = timings[pulse_idx];
                pulse_data->gap[pulse_count] = timings[gap_idx];
                pulse_count++;
            }
        }
    }
    
    pulse_data->num_pulses = pulse_count;
    
    // Set reasonable defaults for other fields
    pulse_data->centerfreq_hz = 433920000;
    pulse_data->freq1_hz = pulse_data->centerfreq_hz;
    pulse_data->depth_bits = 8;
    
    return 0; // Success
}

int rtl433_rfraw_reconstruct_from_json(const char *json_str, pulse_data_t *pulse_data)
{
    if (!json_str || !pulse_data) {
        return -1;
    }
    
    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        return -1;
    }
    
    // Try to extract hex_string first (preferred method)
    json_object *hex_obj;
    if (json_object_object_get_ex(root, "hex_string", &hex_obj)) {
        const char *hex_string = json_object_get_string(hex_obj);
        if (hex_string) {
            int result = rtl433_rfraw_parse_hex_string(hex_string, pulse_data);
            json_object_put(root);
            return result;
        }
    }
    
    // Fallback: reconstruct from pulse data fields (existing method)
    // ... existing pulse data reconstruction logic would go here ...
    
    json_object_put(root);
    return -1; // Not implemented yet
}
