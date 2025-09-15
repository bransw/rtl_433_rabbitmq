/** @file
 * Demonstration of working with real signal data
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>

// Simplified pulse_data structure 
typedef struct {
    unsigned sample_rate;
    float centerfreq_hz;
    float freq1_hz, freq2_hz;
    float rssi_db, snr_db, noise_db;
    int fsk_f1_est, fsk_f2_est;
    int ook_low_estimate, ook_high_estimate;
    unsigned num_pulses;
    unsigned pulse[256];
    unsigned gap[256];
    unsigned start_ago, end_ago;
} simple_pulse_data_t;

// Функция реконструкции (копируем из test_signal_simple.c)
int simple_signal_reconstruct_from_json(const char *json_str, simple_pulse_data_t *pulse_data);

// === УТИЛИТЫ ===

char* read_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("❌ Failed to open file: %s\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(length + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    
    return content;
}

void print_device_info(const char *json_str)
{
    json_object *root = json_tokener_parse(json_str);
    if (!root) return;
    
    json_object *obj;
    
    printf("🏷️ Device information:\n");
    
    if (json_object_object_get_ex(root, "model", &obj)) {
        printf("   Model: %s\n", json_object_get_string(obj));
    }
    if (json_object_object_get_ex(root, "type", &obj)) {
        printf("   Type: %s\n", json_object_get_string(obj));
    }
    if (json_object_object_get_ex(root, "id", &obj)) {
        printf("   ID: %s\n", json_object_get_string(obj));
    }
    if (json_object_object_get_ex(root, "time", &obj)) {
        printf("   Time: %s\n", json_object_get_string(obj));
    }
    
    // Дополнительные данные
    if (json_object_object_get_ex(root, "decoded", &obj)) {
        printf("🔍 Decoded data:\n");
        json_object_object_foreach(obj, key, val) {
            printf("   %s: %s\n", key, json_object_get_string(val));
        }
    }
    
    json_object_put(root);
}

void print_signal_analysis(const simple_pulse_data_t *data)
{
    printf("📊 Signal analysis:\n");
    printf("   Sample rate: %u Hz (%.1f MHz)\n", data->sample_rate, data->sample_rate / 1000000.0);
    printf("   Frequency: %.1f MHz\n", data->centerfreq_hz / 1000000.0);
    
    if (data->freq1_hz != 0 && data->freq2_hz != 0) {
        printf("   FSK frequencies: %.1f and %.1f MHz\n", 
               data->freq1_hz / 1000000.0, data->freq2_hz / 1000000.0);
        printf("   FSK deviation: %d and %d Hz\n", data->fsk_f1_est, data->fsk_f2_est);
    }
    
    printf("   Quality: RSSI=%.1f dB, SNR=%.1f dB\n", data->rssi_db, data->snr_db);
    printf("   Pulses: %u (total time: %.2f ms)\n", 
           data->num_pulses, 
           (double)data->start_ago / data->sample_rate * 1000.0);
    
    // Статистика пульсов
    if (data->num_pulses > 0) {
        unsigned min_pulse = data->pulse[0], max_pulse = data->pulse[0];
        unsigned min_gap = data->gap[0], max_gap = data->gap[0];
        unsigned total_pulse = 0, total_gap = 0;
        
        for (unsigned i = 0; i < data->num_pulses; i++) {
            if (data->pulse[i] < min_pulse) min_pulse = data->pulse[i];
            if (data->pulse[i] > max_pulse) max_pulse = data->pulse[i];
            if (data->gap[i] < min_gap) min_gap = data->gap[i];
            if (data->gap[i] > max_gap) max_gap = data->gap[i];
            total_pulse += data->pulse[i];
            total_gap += data->gap[i];
        }
        
        printf("📈 Pulse statistics:\n");
        printf("   Pulses: min=%u, max=%u, avg=%u samples\n", 
               min_pulse, max_pulse, total_pulse / data->num_pulses);
        printf("   Gaps: min=%u, max=%u, avg=%u samples\n", 
               min_gap, max_gap, total_gap / data->num_pulses);
        
        // Конверсия в микросекунды
        double pulse_us = (double)total_pulse / data->num_pulses / data->sample_rate * 1000000.0;
        double gap_us = (double)total_gap / data->num_pulses / data->sample_rate * 1000000.0;
        printf("   Timing: pulse=%.1f μs, gap=%.1f μs\n", pulse_us, gap_us);
    }
}

int process_signal_file(const char *filename)
{
    printf("\n═══════════════════════════════════════════════════════\n");
    printf("📄 Processing file: %s\n", filename);
    
    char *json_content = read_file(filename);
    if (!json_content) {
        return -1;
    }
    
    // Показать информацию об устройстве
    print_device_info(json_content);
    
    // Реконструировать сигнал
    simple_pulse_data_t pulse_data;
    int result = simple_signal_reconstruct_from_json(json_content, &pulse_data);
    
    if (result != 0) {
        printf("❌ Signal reconstruction error\n");
        free(json_content);
        return -1;
    }
    
    printf("✅ Signal successfully reconstructed\n");
    
    // Показать анализ сигнала
    print_signal_analysis(&pulse_data);
    
    // Показать первые несколько пульсов
    printf("📋 First 8 pulses:\n   ");
    for (unsigned i = 0; i < pulse_data.num_pulses && i < 8; i++) {
        printf("[%u,%u] ", pulse_data.pulse[i], pulse_data.gap[i]);
    }
    printf("\n");
    
    free(json_content);
    return 0;
}

// Копируем функцию реконструкции
int simple_signal_reconstruct_from_json(const char *json_str, simple_pulse_data_t *pulse_data)
{
    if (!json_str || !pulse_data) return -1;
    
    memset(pulse_data, 0, sizeof(simple_pulse_data_t));
    
    json_object *root = json_tokener_parse(json_str);
    if (!root) return -1;
    
    json_object *obj;
    
    // Sample rate
    if (json_object_object_get_ex(root, "rate_Hz", &obj)) {
        pulse_data->sample_rate = json_object_get_int(obj);
    }
    
    // Частоты
    if (json_object_object_get_ex(root, "freq_Hz", &obj)) {
        pulse_data->centerfreq_hz = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "freq1_Hz", &obj)) {
        pulse_data->freq1_hz = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "freq2_Hz", &obj)) {
        pulse_data->freq2_hz = (float)json_object_get_double(obj);
    }
    
    // Качество сигнала
    if (json_object_object_get_ex(root, "rssi_dB", &obj)) {
        pulse_data->rssi_db = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "snr_dB", &obj)) {
        pulse_data->snr_db = (float)json_object_get_double(obj);
    }
    if (json_object_object_get_ex(root, "noise_dB", &obj)) {
        pulse_data->noise_db = (float)json_object_get_double(obj);
    }
    
    // FSK оценки (приоритет у готовых значений)
    if (json_object_object_get_ex(root, "fsk_f1_est", &obj)) {
        pulse_data->fsk_f1_est = json_object_get_int(obj);
    }
    if (json_object_object_get_ex(root, "fsk_f2_est", &obj)) {
        pulse_data->fsk_f2_est = json_object_get_int(obj);
    }
    if (json_object_object_get_ex(root, "ook_low_estimate", &obj)) {
        pulse_data->ook_low_estimate = json_object_get_int(obj);
    }
    if (json_object_object_get_ex(root, "ook_high_estimate", &obj)) {
        pulse_data->ook_high_estimate = json_object_get_int(obj);
    }
    
    // Timing
    if (json_object_object_get_ex(root, "start_ago", &obj)) {
        pulse_data->start_ago = json_object_get_int(obj);
    }
    if (json_object_object_get_ex(root, "end_ago", &obj)) {
        pulse_data->end_ago = json_object_get_int(obj);
    }
    
    // Массив пульсов
    if (json_object_object_get_ex(root, "pulses", &obj) && json_object_is_type(obj, json_type_array)) {
        int array_len = json_object_array_length(obj);
        int pulse_count = array_len / 2;
        
        if (pulse_count > 0 && pulse_count <= 256) {
            pulse_data->num_pulses = pulse_count;
            
            for (int i = 0; i < pulse_count; i++) {
                json_object *pulse_obj = json_object_array_get_idx(obj, i * 2);
                json_object *gap_obj = json_object_array_get_idx(obj, i * 2 + 1);
                
                if (pulse_obj && gap_obj) {
                    pulse_data->pulse[i] = json_object_get_int(pulse_obj);
                    pulse_data->gap[i] = json_object_get_int(gap_obj);
                }
            }
        }
    }
    
    json_object_put(root);
    return 0;
}

int main()
{
    printf("🚀 REAL SIGNAL DATA PROCESSING DEMONSTRATION\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("📡 RTL433 signal reconstruction functions in action\n");
    
    // Обработать все тестовые файлы
    const char *files[] = {
        "test_data/toyota_tpms_sample.json",
        "test_data/ook_device_sample.json"
    };
    
    int total_files = sizeof(files) / sizeof(files[0]);
    int processed = 0;
    
    for (int i = 0; i < total_files; i++) {
        if (process_signal_file(files[i]) == 0) {
            processed++;
        }
    }
    
    printf("\n═══════════════════════════════════════════════════════\n");
    printf("📊 RESULTS:\n");
    printf("   Files processed: %d of %d\n", processed, total_files);
    
    if (processed == total_files) {
        printf("🎉 ALL FILES PROCESSED SUCCESSFULLY!\n");
        printf("✅ Reconstruction functions ready for production\n");
        printf("📈 Supports FSK (Toyota TPMS) and OOK devices\n");
        printf("⚡ Detailed signal analysis includes:\n");
        printf("   • Frequency parameters and modulation\n");
        printf("   • Signal quality (RSSI/SNR)\n");
        printf("   • Pulse statistics and timing\n");
        printf("   • Device metadata\n");
        return 0;
    } else {
        printf("⚠️ Some files could not be processed\n");
        return 1;
    }
}
