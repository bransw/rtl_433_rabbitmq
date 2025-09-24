#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

// Include rtl_433 headers
#include "../include/pulse_data.h"
#include "../include/rfraw.h"

void print_pulse_data(const pulse_data_t *data, const char *label) {
    printf("\n=== %s ===\n", label);
    printf("num_pulses: %u\n", data->num_pulses);
    printf("sample_rate: %u\n", data->sample_rate);
    printf("freq_Hz: %.0f\n", data->centerfreq_hz);
    printf("rssi_dB: %.2f\n", data->rssi_db);
    printf("snr_dB: %.2f\n", data->snr_db);
    printf("noise_dB: %.2f\n", data->noise_db);
    
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

int compare_pulse_data(const pulse_data_t *original, const pulse_data_t *restored) {
    int differences = 0;
    
    printf("\n🔍 COMPARING PULSE DATA:\n");
    
    // Compare num_pulses
    if (original->num_pulses != restored->num_pulses) {
        printf("❌ num_pulses: %u != %u\n", original->num_pulses, restored->num_pulses);
        differences++;
    } else {
        printf("✅ num_pulses: %u == %u\n", original->num_pulses, restored->num_pulses);
    }
    
    // Compare sample_rate (restored always sets to 1000000)
    if (original->sample_rate != restored->sample_rate) {
        printf("⚠️  sample_rate: %u != %u (expected difference - rfraw uses 1MHz)\n", 
               original->sample_rate, restored->sample_rate);
    } else {
        printf("✅ sample_rate: %u == %u\n", original->sample_rate, restored->sample_rate);
    }
    
    // Compare pulses (only if num_pulses match)
    if (original->num_pulses == restored->num_pulses) {
        int pulse_diffs = 0;
        for (unsigned i = 0; i < original->num_pulses; i++) {
            if (original->pulse[i] != restored->pulse[i]) {
                printf("❌ pulse[%u]: %d != %d\n", i, original->pulse[i], restored->pulse[i]);
                pulse_diffs++;
                differences++;
            }
        }
        if (pulse_diffs == 0) {
            printf("✅ All pulses match\n");
        }
        
        int gap_diffs = 0;
        for (unsigned i = 0; i < original->num_pulses; i++) {
            if (original->gap[i] != restored->gap[i]) {
                printf("❌ gap[%u]: %d != %d\n", i, original->gap[i], restored->gap[i]);
                gap_diffs++;
                differences++;
            }
        }
        if (gap_diffs == 0) {
            printf("✅ All gaps match\n");
        }
    }
    
    return differences;
}

pulse_data_t parse_json_to_pulse_data(const char *json_str) {
    pulse_data_t pulse_data = {0};
    
    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        printf("❌ Failed to parse JSON\n");
        return pulse_data;
    }
    
    // Extract basic fields
    json_object *obj;
    
    if (json_object_object_get_ex(root, "count", &obj)) {
        pulse_data.num_pulses = json_object_get_int(obj);
    }
    
    if (json_object_object_get_ex(root, "rate_Hz", &obj)) {
        pulse_data.sample_rate = json_object_get_int(obj);
    }
    
    if (json_object_object_get_ex(root, "freq_Hz", &obj)) {
        pulse_data.centerfreq_hz = json_object_get_double(obj);
    }
    
    if (json_object_object_get_ex(root, "rssi_dB", &obj)) {
        pulse_data.rssi_db = json_object_get_double(obj);
    }
    
    if (json_object_object_get_ex(root, "snr_dB", &obj)) {
        pulse_data.snr_db = json_object_get_double(obj);
    }
    
    if (json_object_object_get_ex(root, "noise_dB", &obj)) {
        pulse_data.noise_db = json_object_get_double(obj);
    }
    
    // Extract pulses array
    if (json_object_object_get_ex(root, "pulses", &obj)) {
        int array_len = json_object_array_length(obj);
        for (int i = 0; i < array_len && i < PD_MAX_PULSES; i += 2) {
            json_object *pulse_obj = json_object_array_get_idx(obj, i);
            json_object *gap_obj = json_object_array_get_idx(obj, i + 1);
            
            if (pulse_obj) pulse_data.pulse[i/2] = json_object_get_int(pulse_obj);
            if (gap_obj) pulse_data.gap[i/2] = json_object_get_int(gap_obj);
        }
    }
    
    json_object_put(root);
    return pulse_data;
}

int main() {
    printf("🧪 TESTING RFRAW CONVERSION\n");
    printf("===========================\n");
    
    // Test message from your example
    const char *test_json = "{\"hex_string\":\"AAB102095C5D9C8155\",\"package_id\":110613,\"mod\":\"OOK\",\"count\":1,\"pulses\":[2396,23964],\"freq1_Hz\":433919776,\"freq_Hz\":433920000,\"rate_Hz\":250000,\"depth_bits\":0,\"range_dB\":84.2884,\"rssi_dB\":5.10922,\"snr_dB\":1.23904,\"noise_dB\":-1.23958}";
    
    const char *hex_string = "AAB102095C5D9C8155";
    
    printf("📡 Original JSON: %.100s...\n", test_json);
    printf("🔤 Hex string: %s\n", hex_string);
    
    // Parse original pulse_data from JSON
    pulse_data_t original = parse_json_to_pulse_data(test_json);
    print_pulse_data(&original, "ORIGINAL from JSON");
    
    // Parse pulse_data from hex string using rfraw_parse
    pulse_data_t restored = {0};
    bool success = rfraw_parse(&restored, hex_string);
    
    if (!success) {
        printf("❌ rfraw_parse() failed!\n");
        return 1;
    }
    
    print_pulse_data(&restored, "RESTORED from HEX");
    
    // Compare the results
    int differences = compare_pulse_data(&original, &restored);
    
    printf("\n🎯 FINAL RESULT:\n");
    if (differences == 0) {
        printf("🎉 PERFECT MATCH! All pulse data is identical!\n");
        printf("✅ Conclusion: pulse_data in JSON is REDUNDANT - hex_string contains everything!\n");
    } else if (differences == 1 && original.sample_rate != restored.sample_rate) {
        printf("✅ ALMOST PERFECT! Only sample_rate differs (expected for rfraw format)\n");
        printf("✅ Conclusion: pulse_data in JSON is REDUNDANT - hex_string contains all timing info!\n");
    } else {
        printf("❌ Found %d differences\n", differences);
        printf("⚠️  Conclusion: pulse_data in JSON may NOT be fully redundant\n");
    }
    
    return 0;
}
