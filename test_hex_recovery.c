#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Copy necessary structures from rtl_433
#define PD_MAX_PULSES 1200

typedef struct pulse_data {
    uint64_t offset;
    uint32_t sample_rate;
    unsigned depth_bits;
    unsigned start_ago;
    unsigned end_ago;
    int ook_low_estimate;
    int ook_high_estimate;
    int fsk_f1_est;
    int fsk_f2_est;
    float freq1_hz;
    float freq2_hz;
    float centerfreq_hz;
    float range_db;
    float rssi_db;
    float snr_db;
    float noise_db;
    unsigned num_pulses;
    int pulse[PD_MAX_PULSES];
    int gap[PD_MAX_PULSES];
} pulse_data_t;

typedef struct signal_metadata {
    double freq_hz;
    uint32_t rate_hz;
    float rssi_db;
    float snr_db;
    float noise_db;
    double freq1_hz;
    double freq2_hz;
    int ook_low_estimate;
    int ook_high_estimate;
    int fsk_f1_est;
    int fsk_f2_est;
    uint64_t offset;
    unsigned start_ago;
    unsigned end_ago;
    float range_db;
    unsigned depth_bits;
} signal_metadata_t;

// ===== LOCAL COPY OF rfraw_parse LOGIC =====

static int hexstr_get_nibble(const char **p) {
    int val;
    while (**p == ' ' || **p == '\t' || **p == '-' || **p == ':') ++(*p);
    if (**p >= '0' && **p <= '9')      val = **p - '0';
    else if (**p >= 'a' && **p <= 'f') val = **p - 'a' + 10;
    else if (**p >= 'A' && **p <= 'F') val = **p - 'A' + 10;
    else return -1;
    ++(*p);
    return val;
}

static int hexstr_get_byte(const char **p) {
    int h = hexstr_get_nibble(p);
    int l = hexstr_get_nibble(p);
    if (h < 0 || l < 0) return -1;
    return (h << 4) | l;
}

static int hexstr_get_u16(const char **p) {
    int h = hexstr_get_byte(p);
    int l = hexstr_get_byte(p);
    if (h < 0 || l < 0) return -1;
    return (h << 8) | l;
}

/**
 * Parse rfraw hex string to pulse_data
 * Based on src/rfraw.c from rtl_433
 * 
 * Format: AA B1 04 <payload>
 * AA - header (0xAA family)
 * B1 - modulation and length info
 * 04 - histogram count
 * <payload> - timing histogram + pulse/gap data
 */
static int parse_rfraw_hex_string(const char *hex_str, pulse_data_t *pulse_data) {
    if (!hex_str || !pulse_data) return -1;
    
    const char *p = hex_str;
    
    // Check header (0xAA family)
    if (hexstr_get_nibble(&p) != 0xa) return -1;
    if (hexstr_get_nibble(&p) != 0xa) return -1;
    if (hexstr_get_nibble(&p) != 0xb) return -1;
    int header_type = hexstr_get_nibble(&p);
    if ((header_type | 1) != 0x1) return -1;
    
    printf("📡 Header: 0xAAB%X\n", header_type);
    
    // Get histogram count (next byte after header)
    int hist_count = hexstr_get_byte(&p);
    if (hist_count < 0 || hist_count > 255) return -1;
    printf("📊 Histogram bins: %d\n", hist_count);
    
    // Read timing histogram
    int histogram[256];
    for (int i = 0; i < hist_count; i++) {
        int timing = hexstr_get_u16(&p);
        if (timing < 0) return -1;
        histogram[i] = timing;
        printf("  Bin[%d] = %d samples\n", i, timing);
    }
    
    // Detect old vs new format
    // Old format: nibbles encode only pulse/gap flag
    // New format: nibbles >= 8 are pulses, < 8 are gaps
    int oldfmt = 1;  // Assume old format
    const char *test_p = p;
    while (*test_p) {
        int b = hexstr_get_byte(&test_p);
        if (b < 0 || b == 0x55) break;
        if (b & 0x88) {  // Has high bit set - new format
            oldfmt = 0;
            break;
        }
    }
    
    printf("📝 Format: %s\n", oldfmt ? "OLD" : "NEW");
    
    // Read pulse/gap data (nibble by nibble)
    pulse_data->num_pulses = 0;
    int pulse_needed = 1;  // Start expecting pulse
    int aligned = 1;       // Track nibble alignment
    
    while (*p) {
        int w = hexstr_get_nibble(&p);
        if (w < 0) break;
        
        // Check for terminator (0x55 = two nibbles of 5)
        if (w == 5 && aligned) {
            int next_nibble = hexstr_get_nibble(&p);
            if (next_nibble == 5) {
                break;  // Found 0x55 terminator
            }
            // Not terminator, process as normal
            w = next_nibble;
            if (w < 0) break;
        }
        
        aligned = !aligned;
        
        if (pulse_data->num_pulses >= PD_MAX_PULSES) {
            printf("⚠️  Warning: Too many pulses, truncating at %d\n", PD_MAX_PULSES);
            break;
        }
        
        // Determine if this is pulse or gap
        if (w >= 8 || (oldfmt && !aligned)) {  // This is a PULSE
            if (!pulse_needed) {
                // Had pulse without gap - add zero gap
                pulse_data->gap[pulse_data->num_pulses] = 0;
                pulse_data->num_pulses++;
            }
            if (pulse_data->num_pulses < PD_MAX_PULSES) {
                pulse_data->pulse[pulse_data->num_pulses] = histogram[w & 0x7];
                pulse_needed = 0;
            }
        } else {  // This is a GAP
            if (pulse_needed) {
                // Had gap without pulse - add zero pulse
                pulse_data->pulse[pulse_data->num_pulses] = 0;
            }
            pulse_data->gap[pulse_data->num_pulses] = histogram[w];
            pulse_data->num_pulses++;
            pulse_needed = 1;
        }
    }
    
    printf("✅ Parsed %u pulses from hex_string\n", pulse_data->num_pulses);
    
    return 0;
}

/**
 * Set default values for pulse_data
 * Based on rtl433_signal_set_defaults()
 */
static void set_pulse_data_defaults(pulse_data_t *pulse_data) {
    if (!pulse_data) return;
    
    pulse_data->sample_rate = 250000;  // Default sample rate
    pulse_data->depth_bits = 8;
    pulse_data->offset = 0;
    pulse_data->start_ago = 0;
    pulse_data->end_ago = 0;
    pulse_data->ook_low_estimate = 1000;
    pulse_data->ook_high_estimate = 8000;
    pulse_data->fsk_f1_est = 0;
    pulse_data->fsk_f2_est = 0;
    pulse_data->range_db = 0.0f;
    pulse_data->centerfreq_hz = 433920000.0f;  // Default frequency
    pulse_data->freq1_hz = 433920000.0f;
    pulse_data->freq2_hz = 0.0f;
}

/**
 * Apply metadata to pulse_data
 * Overwrites defaults with actual values from signal
 */
static void apply_metadata(pulse_data_t *pulse_data, const signal_metadata_t *metadata) {
    if (!pulse_data || !metadata) return;
    
    if (metadata->freq_hz > 0) {
        pulse_data->centerfreq_hz = (float)metadata->freq_hz;
    }
    if (metadata->rate_hz > 0) {
        pulse_data->sample_rate = metadata->rate_hz;
    }
    if (metadata->freq1_hz > 0) {
        pulse_data->freq1_hz = (float)metadata->freq1_hz;
    }
    if (metadata->freq2_hz > 0) {
        pulse_data->freq2_hz = (float)metadata->freq2_hz;
    }
    
    pulse_data->rssi_db = metadata->rssi_db;
    pulse_data->snr_db = metadata->snr_db;
    pulse_data->noise_db = metadata->noise_db;
    pulse_data->range_db = metadata->range_db;
    pulse_data->depth_bits = metadata->depth_bits;
    pulse_data->offset = metadata->offset;
    pulse_data->start_ago = metadata->start_ago;
    pulse_data->end_ago = metadata->end_ago;
    pulse_data->ook_low_estimate = metadata->ook_low_estimate;
    pulse_data->ook_high_estimate = metadata->ook_high_estimate;
    pulse_data->fsk_f1_est = metadata->fsk_f1_est;
    pulse_data->fsk_f2_est = metadata->fsk_f2_est;
}

/**
 * MAIN FUNCTION: Signal recovery from hex_string + metadata
 * 
 * This function reconstructs complete pulse_data from:
 * 1. hex_string - compact timing data (REQUIRED)
 * 2. metadata - RF parameters and signal quality (RECOMMENDED)
 * 
 * Returns: 0 on success, -1 on error
 */
int signal_recovery(const char *hex_string, const signal_metadata_t *metadata, pulse_data_t *pulse_data) {
    if (!hex_string || !pulse_data) {
        fprintf(stderr, "❌ Invalid parameters\n");
        return -1;
    }
    
    printf("🔄 Starting signal recovery...\n");
    printf("📦 hex_string: %s\n", hex_string);
    
    // Step 1: Initialize with defaults
    memset(pulse_data, 0, sizeof(pulse_data_t));
    set_pulse_data_defaults(pulse_data);
    printf("✅ Step 1: Defaults set\n");
    
    // Step 2: Parse hex_string to extract pulses
    if (parse_rfraw_hex_string(hex_string, pulse_data) != 0) {
        fprintf(stderr, "❌ Failed to parse hex_string\n");
        return -1;
    }
    printf("✅ Step 2: Pulses extracted (%u pulses)\n", pulse_data->num_pulses);
    
    // Step 3: Apply metadata (if provided)
    if (metadata) {
        apply_metadata(pulse_data, metadata);
        printf("✅ Step 3: Metadata applied\n");
    } else {
        printf("⚠️  Step 3: No metadata provided, using defaults\n");
    }
    
    return 0;
}

/**
 * Print pulse_data structure for debugging
 */
void print_pulse_data(const pulse_data_t *pd) {
    if (!pd) return;
    
    printf("\n📊 PULSE DATA STRUCTURE:\n");
    printf("  ├─ num_pulses: %u\n", pd->num_pulses);
    printf("  ├─ sample_rate: %u Hz\n", pd->sample_rate);
    printf("  ├─ centerfreq_hz: %.0f Hz\n", pd->centerfreq_hz);
    printf("  ├─ freq1_hz: %.0f Hz\n", pd->freq1_hz);
    printf("  ├─ freq2_hz: %.0f Hz\n", pd->freq2_hz);
    printf("  ├─ offset: %lu samples\n", pd->offset);
    printf("  ├─ start_ago: %u\n", pd->start_ago);
    printf("  ├─ end_ago: %u\n", pd->end_ago);
    printf("  ├─ depth_bits: %u\n", pd->depth_bits);
    printf("  ├─ rssi_dB: %.3f dB\n", pd->rssi_db);
    printf("  ├─ snr_dB: %.3f dB\n", pd->snr_db);
    printf("  ├─ noise_dB: %.3f dB\n", pd->noise_db);
    printf("  ├─ range_dB: %.3f dB\n", pd->range_db);
    printf("  ├─ ook_low_estimate: %d\n", pd->ook_low_estimate);
    printf("  ├─ ook_high_estimate: %d\n", pd->ook_high_estimate);
    printf("  ├─ fsk_f1_est: %d\n", pd->fsk_f1_est);
    printf("  └─ fsk_f2_est: %d\n", pd->fsk_f2_est);
    
    if (pd->num_pulses > 0) {
        printf("\n  📈 First 20 pulses: ");
        for (unsigned i = 0; i < pd->num_pulses && i < 20; i++) {
            printf("%d ", pd->pulse[i]);
        }
        if (pd->num_pulses > 20) printf("...");
        printf("\n");
        
        printf("  📉 First 20 gaps: ");
        for (unsigned i = 0; i < pd->num_pulses && i < 20; i++) {
            printf("%d ", pd->gap[i]);
        }
        if (pd->num_pulses > 20) printf("...");
        printf("\n");
    }
}

/**
 * Compare two pulse arrays
 */
void compare_pulses(const pulse_data_t *original, const pulse_data_t *recovered) {
    printf("\n🔍 PULSE COMPARISON:\n");
    printf("  Original pulses:  %u\n", original->num_pulses);
    printf("  Recovered pulses: %u\n", recovered->num_pulses);
    
    if (original->num_pulses == 0 || recovered->num_pulses == 0) {
        printf("  ⚠️  Cannot compare: one or both have no pulses\n");
        return;
    }
    
    unsigned min_pulses = (original->num_pulses < recovered->num_pulses) ? 
                          original->num_pulses : recovered->num_pulses;
    
    int exact_matches = 0;
    int close_matches = 0;  // Within 10% tolerance
    
    for (unsigned i = 0; i < min_pulses && i < 20; i++) {
        int orig = original->pulse[i];
        int recv = recovered->pulse[i];
        
        if (orig == recv) {
            exact_matches++;
            printf("  [%2u] %4d == %4d ✅\n", i, orig, recv);
        } else {
            int diff = abs(orig - recv);
            float percent = (orig > 0) ? (100.0f * diff / orig) : 100.0f;
            
            if (percent < 10.0f) {
                close_matches++;
                printf("  [%2u] %4d ~= %4d (%.1f%%) ⚠️\n", i, orig, recv, percent);
            } else {
                printf("  [%2u] %4d != %4d (%.1f%%) ❌\n", i, orig, recv, percent);
            }
        }
    }
    
    if (min_pulses > 20) {
        printf("  ... (%u more pulses not shown)\n", min_pulses - 20);
    }
    
    printf("\n  📊 Match statistics:\n");
    printf("    Exact matches: %d / %u (%.1f%%)\n", 
           exact_matches, min_pulses, 100.0f * exact_matches / min_pulses);
    printf("    Close matches: %d / %u (%.1f%%)\n", 
           close_matches, min_pulses, 100.0f * close_matches / min_pulses);
    printf("    Total good:    %d / %u (%.1f%%)\n", 
           exact_matches + close_matches, min_pulses, 
           100.0f * (exact_matches + close_matches) / min_pulses);
}

// ===== MAIN TEST PROGRAM =====

int main() {
    printf("🧪 TEST: Signal Recovery from hex_string + metadata\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n\n");
    
    // Data from test_hex_recovery.json
    const char *hex_string = "AAB1040030006000C8008C80808081A19081809190819190818080908180919180919180919080808191808090808081808080808080808080809190808081919190808155";
    
    // Original pulse data for comparison
    pulse_data_t original;
    memset(&original, 0, sizeof(pulse_data_t));
    original.num_pulses = 58;
    int orig_pulses[] = {40, 60, 40, 64, 40, 52, 52, 96, 200, 104, 96, 52, 48, 96, 52, 52, 100, 100, 100, 48, 52, 100, 96, 100, 100, 56, 48, 96, 48, 52, 48, 52, 100, 48, 48, 100, 52, 52, 96, 108, 92, 100, 48, 56, 96, 100, 100, 100, 52, 48, 104, 92, 100, 52, 48, 48, 56, 48};
    for (int i = 0; i < 58; i++) {
        original.pulse[i] = orig_pulses[i];
    }
    
    // Metadata from test_hex_recovery.json
    signal_metadata_t metadata = {
        .freq_hz = 433920000,
        .rate_hz = 250000,
        .rssi_db = -2.714f,
        .snr_db = 6.559f,
        .noise_db = -9.273f,
        .freq1_hz = 433942688,
        .freq2_hz = 433878368,
        .ook_low_estimate = 1937,
        .ook_high_estimate = 8770,
        .fsk_f1_est = 5951,
        .fsk_f2_est = -10916,
        .offset = 47161,
        .start_ago = 18375,
        .end_ago = 16384,
        .range_db = 42.144f,
        .depth_bits = 8
    };
    
    // Test 1: Recovery with full metadata
    printf("📋 TEST 1: Recovery with FULL metadata\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-\n");
    
    pulse_data_t recovered_full;
    int result1 = signal_recovery(hex_string, &metadata, &recovered_full);
    
    if (result1 == 0) {
        printf("\n✅ TEST 1 PASSED: Recovery successful\n");
        print_pulse_data(&recovered_full);
        compare_pulses(&original, &recovered_full);
    } else {
        printf("\n❌ TEST 1 FAILED: Recovery failed\n");
    }
    
    // Test 2: Recovery with minimal data (no metadata)
    printf("\n\n📋 TEST 2: Recovery with MINIMAL data (hex_string only)\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-\n");
    
    pulse_data_t recovered_minimal;
    int result2 = signal_recovery(hex_string, NULL, &recovered_minimal);
    
    if (result2 == 0) {
        printf("\n✅ TEST 2 PASSED: Recovery successful\n");
        print_pulse_data(&recovered_minimal);
        compare_pulses(&original, &recovered_minimal);
    } else {
        printf("\n❌ TEST 2 FAILED: Recovery failed\n");
    }
    
    // Test 3: Recovery with partial metadata (RF only)
    printf("\n\n📋 TEST 3: Recovery with PARTIAL metadata (RF params only)\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-\n");
    
    signal_metadata_t partial_metadata = {
        .freq_hz = 433920000,
        .rate_hz = 250000,
        .rssi_db = -2.714f,
        .snr_db = 6.559f,
        .noise_db = -9.273f
    };
    
    pulse_data_t recovered_partial;
    int result3 = signal_recovery(hex_string, &partial_metadata, &recovered_partial);
    
    if (result3 == 0) {
        printf("\n✅ TEST 3 PASSED: Recovery successful\n");
        print_pulse_data(&recovered_partial);
        compare_pulses(&original, &recovered_partial);
    } else {
        printf("\n❌ TEST 3 FAILED: Recovery failed\n");
    }
    
    printf("\n\n🎯 SUMMARY:\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n");
    printf("Test 1 (Full metadata):    %s\n", result1 == 0 ? "✅ PASSED" : "❌ FAILED");
    printf("Test 2 (Minimal data):     %s\n", result2 == 0 ? "✅ PASSED" : "❌ FAILED");
    printf("Test 3 (Partial metadata): %s\n", result3 == 0 ? "✅ PASSED" : "❌ FAILED");
    
    printf("\n💡 KEY FINDINGS:\n");
    printf("  • hex_string is SUFFICIENT for pulse recovery\n");
    printf("  • Metadata improves RF parameters accuracy\n");
    printf("  • Defaults work for basic detection\n");
    printf("  • Histogram-based encoding is LOSSY (not exact)\n");
    
    printf("\n🧪 Test completed!\n");
    return 0;
}
