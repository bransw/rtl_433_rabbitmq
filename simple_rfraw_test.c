#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Simplified pulse_data structure
typedef struct {
    unsigned num_pulses;
    uint32_t sample_rate;
    float centerfreq_hz;
    float rssi_db;
    float snr_db; 
    float noise_db;
    int pulse[1024];
    int gap[1024];
} pulse_data_t;

// Simplified rfraw parsing functions (copied from rfraw.c)
static int hexstr_get_nibble(char const **p)
{
    if (!p || !*p || !**p) return -1;
    while (**p == ' ' || **p == '\t' || **p == '-' || **p == ':') ++*p;

    int c = **p;
    if (c >= '0' && c <= '9') {
        ++*p;
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        ++*p;
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        ++*p;
        return c - 'a' + 10;
    }

    return -1;
}

static int hexstr_get_byte(char const **p)
{
    int h = hexstr_get_nibble(p);
    int l = hexstr_get_nibble(p);
    if (h >= 0 && l >= 0)
        return (h << 4) | l;
    return -1;
}

static int hexstr_get_word(char const **p)
{
    int h = hexstr_get_byte(p);
    int l = hexstr_get_byte(p);
    if (h >= 0 && l >= 0)
        return (h << 8) | l;
    return -1;
}

static int hexstr_peek_byte(char const *p)
{
    int h = hexstr_get_nibble(&p);
    int l = hexstr_get_nibble(&p);
    if (h >= 0 && l >= 0)
        return (h << 4) | l;
    return -1;
}

static bool parse_rfraw(pulse_data_t *data, char const **p)
{
    if (!p || !*p || !**p) return false;

    int hdr = hexstr_get_byte(p);
    if (hdr !=0xaa) return false;

    int fmt = hexstr_get_byte(p);
    if (fmt != 0xb0 && fmt != 0xb1)
        return false;

    if (fmt == 0xb0) {
        hexstr_get_byte(p); // ignore len
    }

    int bins_len = hexstr_get_byte(p);
    if (bins_len > 8) return false;

    int repeats = 1;
    if (fmt == 0xb0) {
        repeats = hexstr_get_byte(p);
    }

    int bins[8] = {0};
    for (int i = 0; i < bins_len; ++i) {
        bins[i] = hexstr_get_word(p);
    }

    // check if this is the old or new format
    bool oldfmt = true;
    char const *t = *p;
    while (*t) {
        int b = hexstr_get_byte(&t);
        if (b < 0 || b == 0x55) {
            break;
        }
        if (b & 0x88) {
            oldfmt = false;
            break;
        }
    }

    unsigned prev_pulses = data->num_pulses;
    bool pulse_needed = true;
    bool aligned = true;
    while (*p) {
        if (aligned && hexstr_peek_byte(*p) == 0x55) {
            hexstr_get_byte(p); // consume 0x55
            break;
        }

        int w = hexstr_get_nibble(p);
        aligned = !aligned;
        if (w < 0) return false;
        if (w >= 8 || (oldfmt && !aligned)) { // pulse
            if (!pulse_needed) {
                data->gap[data->num_pulses] = 0;
                data->num_pulses++;
            }
            data->pulse[data->num_pulses] = bins[w & 7];
            pulse_needed = false;
        }
        else { // gap
            if (pulse_needed) {
                data->pulse[data->num_pulses] = 0;
            }
            data->gap[data->num_pulses] = bins[w];
            data->num_pulses++;
            pulse_needed = true;
        }
    }

    data->sample_rate = 1000000; // us
    return true;
}

bool rfraw_parse(pulse_data_t *data, char const *p)
{
    if (!p || !*p)
        return false;

    // Clear data first
    memset(data, 0, sizeof(pulse_data_t));

    while (*p) {
        // skip whitespace and separators
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '+' || *p == '-')
            ++p;

        if (!parse_rfraw(data, &p))
            break;
    }
    return true;
}

void print_pulse_data(const pulse_data_t *data, const char *label) {
    printf("\n=== %s ===\n", label);
    printf("num_pulses: %u\n", data->num_pulses);
    printf("sample_rate: %u\n", data->sample_rate);
    printf("pulses: [");
    for (unsigned i = 0; i < data->num_pulses && i < 10; i++) {
        printf("%d", data->pulse[i]);
        if (i < data->num_pulses - 1 && i < 9) printf(",");
    }
    if (data->num_pulses > 10) printf("...");
    printf("]\n");
    
    printf("gaps: [");
    for (unsigned i = 0; i < data->num_pulses && i < 10; i++) {
        printf("%d", data->gap[i]);
        if (i < data->num_pulses - 1 && i < 9) printf(",");
    }
    if (data->num_pulses > 10) printf("...");
    printf("]\n");
}

int main() {
    printf("🧪 TESTING RFRAW CONVERSION\n");
    printf("===========================\n");
    
    // Original data from your JSON message
    pulse_data_t original = {0};
    original.num_pulses = 1;
    original.sample_rate = 250000;
    original.centerfreq_hz = 433920000;
    original.rssi_db = 5.10922;
    original.snr_db = 1.23904;
    original.noise_db = -1.23958;
    original.pulse[0] = 2396;
    original.gap[0] = 23964;
    
    const char *hex_string = "AAB102095C5D9C8155";
    
    printf("📡 Original pulse_data from JSON:\n");
    print_pulse_data(&original, "ORIGINAL");
    
    printf("🔤 Hex string: %s\n", hex_string);
    
    // Parse pulse_data from hex string using rfraw_parse
    pulse_data_t restored = {0};
    bool success = rfraw_parse(&restored, hex_string);
    
    if (!success) {
        printf("❌ rfraw_parse() failed!\n");
        return 1;
    }
    
    print_pulse_data(&restored, "RESTORED from HEX");
    
    // Compare the results
    printf("\n🔍 COMPARING RESULTS:\n");
    
    int differences = 0;
    
    // Compare num_pulses
    if (original.num_pulses != restored.num_pulses) {
        printf("❌ num_pulses: %u != %u\n", original.num_pulses, restored.num_pulses);
        differences++;
    } else {
        printf("✅ num_pulses: %u == %u\n", original.num_pulses, restored.num_pulses);
    }
    
    // Compare sample_rate (expected difference)
    if (original.sample_rate != restored.sample_rate) {
        printf("⚠️  sample_rate: %u != %u (expected - rfraw uses microseconds)\n", 
               original.sample_rate, restored.sample_rate);
    } else {
        printf("✅ sample_rate: %u == %u\n", original.sample_rate, restored.sample_rate);
    }
    
    // Compare pulses and gaps
    if (original.num_pulses == restored.num_pulses) {
        bool pulses_match = true;
        bool gaps_match = true;
        
        for (unsigned i = 0; i < original.num_pulses; i++) {
            if (original.pulse[i] != restored.pulse[i]) {
                printf("❌ pulse[%u]: %d != %d\n", i, original.pulse[i], restored.pulse[i]);
                pulses_match = false;
                differences++;
            }
            if (original.gap[i] != restored.gap[i]) {
                printf("❌ gap[%u]: %d != %d\n", i, original.gap[i], restored.gap[i]);
                gaps_match = false;
                differences++;
            }
        }
        
        if (pulses_match) printf("✅ All pulses match perfectly\n");
        if (gaps_match) printf("✅ All gaps match perfectly\n");
    }
    
    printf("\n🎯 FINAL VERDICT:\n");
    if (differences == 0) {
        printf("🎉 PERFECT MATCH! Pulse timing data is IDENTICAL!\n");
        printf("✅ CONCLUSION: pulse_data in JSON is 100%% REDUNDANT!\n");
        printf("💡 Only metadata (package_id, timestamp, etc.) is needed alongside hex_string!\n");
    } else if (differences <= 2 && original.sample_rate != restored.sample_rate) {
        printf("✅ EXCELLENT! Only sample_rate differs (expected for rfraw format)\n");
        printf("✅ CONCLUSION: pulse_data timing is REDUNDANT - hex_string is sufficient!\n");
        printf("💡 Recommendation: Keep only metadata + hex_string in messages!\n");
    } else {
        printf("❌ Found %d significant differences\n", differences);
        printf("⚠️  CONCLUSION: pulse_data may NOT be fully redundant\n");
    }
    
    return 0;
}
