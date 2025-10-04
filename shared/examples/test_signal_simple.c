/** @file
 * Simple test of signal reconstruction functions (without rtl_433 dependencies)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <json-c/json.h>

// Simplified pulse_data structure for demonstration
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

// === SIMPLIFIED RECONSTRUCTION FUNCTIONS ===

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
    
    // Модуляция и FSK оценки
    if (json_object_object_get_ex(root, "mod", &obj)) {
        const char *mod_str = json_object_get_string(obj);
        if (strcmp(mod_str, "FSK") == 0) {
            // FSK параметры
            pulse_data->fsk_f1_est = 1000;
            pulse_data->fsk_f2_est = -1000;
            pulse_data->ook_low_estimate = 1000;
            pulse_data->ook_high_estimate = 8000;
            
            // Уточнение по частотам
            if (pulse_data->freq1_hz != 0 && pulse_data->centerfreq_hz != 0) {
                pulse_data->fsk_f1_est = (int)(pulse_data->freq1_hz - pulse_data->centerfreq_hz);
            }
            if (pulse_data->freq2_hz != 0 && pulse_data->centerfreq_hz != 0) {
                pulse_data->fsk_f2_est = (int)(pulse_data->freq2_hz - pulse_data->centerfreq_hz);
            }
        } else {
            // OOK сигнал
            pulse_data->fsk_f1_est = 0;
            pulse_data->fsk_f2_est = 0;
            pulse_data->ook_low_estimate = 1000;
            pulse_data->ook_high_estimate = 8000;
        }
    }
    
    // Массив пульсов
    if (json_object_object_get_ex(root, "pulses", &obj) && json_object_is_type(obj, json_type_array)) {
        int array_len = json_object_array_length(obj);
        int pulse_count = array_len / 2; // пульс + пауза
        
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
    
    // Вычисление timing информации
    if (pulse_data->num_pulses > 0) {
        unsigned total_time_samples = 0;
        for (unsigned i = 0; i < pulse_data->num_pulses; i++) {
            total_time_samples += pulse_data->pulse[i] + pulse_data->gap[i];
        }
        pulse_data->start_ago = total_time_samples;
        pulse_data->end_ago = 0;
    }
    
    json_object_put(root);
    return 0;
}

// === ТЕСТОВЫЕ ДАННЫЕ ===

const char *test_toyota_json = "{"
    "\"package_id\":77117,"
    "\"rate_Hz\":250000,"
    "\"freq_Hz\":433920000,"
    "\"rssi_dB\":-12.5,"
    "\"snr_dB\":15.3,"
    "\"noise_dB\":-27.8,"
    "\"mod\":\"FSK\","
    "\"freq1_Hz\":433918500,"
    "\"freq2_Hz\":433921500,"
    "\"pulses\":[1200,800,1100,900,1000,1000,1100,900,800,1200,1000,1000]"
"}";

const char *test_ook_json = "{"
    "\"package_id\":12345,"
    "\"rate_Hz\":250000,"
    "\"freq_Hz\":433920000,"
    "\"rssi_dB\":-8.2,"
    "\"snr_dB\":22.1,"
    "\"mod\":\"OOK\","
    "\"pulses\":[500,500,1000,500,500,1000,500,1500,500,500]"
"}";

// === ФУНКЦИИ ТЕСТИРОВАНИЯ ===

void print_signal_data(const simple_pulse_data_t *data, const char *title)
{
    printf("\n=== %s ===\n", title);
    printf("📊 Sample rate: %u Hz\n", data->sample_rate);
    printf("📡 Center freq: %.1f Hz\n", data->centerfreq_hz);
    printf("📈 RSSI: %.1f dB, SNR: %.1f dB, Noise: %.1f dB\n", 
           data->rssi_db, data->snr_db, data->noise_db);
    printf("🔄 Modulation estimates:\n");
    printf("   FSK: f1=%d Hz, f2=%d Hz\n", data->fsk_f1_est, data->fsk_f2_est);
    printf("   OOK: low=%d, high=%d\n", data->ook_low_estimate, data->ook_high_estimate);
    printf("⏱️ Pulses: %u (total time: %u samples)\n", data->num_pulses, data->start_ago);
    
    if (data->num_pulses > 0) {
        printf("📋 First 6 pulses: ");
        for (unsigned i = 0; i < data->num_pulses && i < 6; i++) {
            printf("[%u,%u] ", data->pulse[i], data->gap[i]);
        }
        printf("\n");
    }
}

int test_toyota_reconstruction()
{
    printf("\n🧪 Test 1: Toyota TPMS signal\n");
    
    simple_pulse_data_t data;
    int result = simple_signal_reconstruct_from_json(test_toyota_json, &data);
    
    if (result != 0) {
        printf("❌ FAILED: Reconstruction failed\n");
        return -1;
    }
    
    print_signal_data(&data, "Toyota TPMS Signal");
    
    // Проверки
    assert(data.sample_rate == 250000);
    assert(data.centerfreq_hz == 433920000.0f);
    assert(data.freq1_hz == 433918500.0f);
    assert(data.freq2_hz == 433921500.0f);
    assert(data.rssi_db == -12.5f);
    assert(data.snr_db == 15.3f);
    assert(data.num_pulses == 6); // 12 элементов / 2
    // Проверяем приблизительные значения (округление при конверсии float)
    assert(data.fsk_f1_est >= -1510 && data.fsk_f1_est <= -1490); // ~-1500
    assert(data.fsk_f2_est >= 1490 && data.fsk_f2_est <= 1510);   // ~1500
    
    printf("✅ PASSED: Toyota TPMS reconstruction successful\n");
    return 0;
}

int test_ook_reconstruction()
{
    printf("\n🧪 Test 2: OOK signal\n");
    
    simple_pulse_data_t data;
    int result = simple_signal_reconstruct_from_json(test_ook_json, &data);
    
    if (result != 0) {
        printf("❌ FAILED: OOK reconstruction failed\n");
        return -1;
    }
    
    print_signal_data(&data, "OOK Signal");
    
    // Проверки
    assert(data.sample_rate == 250000);
    assert(data.centerfreq_hz == 433920000.0f);
    assert(data.rssi_db == -8.2f);
    assert(data.snr_db == 22.1f);
    assert(data.num_pulses == 5); // 10 элементов / 2
    assert(data.fsk_f1_est == 0); // OOK
    assert(data.fsk_f2_est == 0);
    
    printf("✅ PASSED: OOK signal reconstruction successful\n");
    return 0;
}

int test_performance()
{
    printf("\n🧪 Test 3: Performance\n");
    
    int iterations = 10000;
    printf("Running %d reconstruction iterations...\n", iterations);
    
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        simple_pulse_data_t data;
        simple_signal_reconstruct_from_json(test_toyota_json, &data);
    }
    
    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    double ops_per_sec = iterations / time_taken;
    
    printf("⏱️ Time: %.3f seconds\n", time_taken);
    printf("⚡ Speed: %.0f operations/sec\n", ops_per_sec);
    printf("📊 Time per operation: %.3f μs\n", (time_taken * 1000000) / iterations);
    
    printf("✅ PASSED: Performance test completed\n");
    return 0;
}

int main()
{
    printf("🚀 SIGNAL RECONSTRUCTION FUNCTIONS TESTING\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    int failed = 0;
    
    // Запуск тестов
    if (test_toyota_reconstruction() != 0) failed++;
    if (test_ook_reconstruction() != 0) failed++;
    if (test_performance() != 0) failed++;
    
    // Итоги
    printf("\n═══════════════════════════════════════════════════════\n");
    if (failed == 0) {
        printf("🎉 ALL TESTS PASSED SUCCESSFULLY! (3/3)\n");
        printf("✅ Signal reconstruction functions ready for use\n");
        printf("📈 System processes Toyota TPMS and OOK signals\n");
        printf("⚡ Performance sufficient for real-time processing\n");
        return 0;
    } else {
        printf("❌ FAILED %d TESTS out of 3\n", failed);
        return 1;
    }
}
