/** @file
 * Тестовая программа для проверки функций реконструкции сигналов
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rtl433_signal.h"
#include "pulse_data.h"

// === ТЕСТОВЫЕ ДАННЫЕ ===

// Тестовый JSON с Toyota TPMS данными
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

// Тестовый JSON с OOK данными
const char *test_ook_json = "{"
    "\"package_id\":12345,"
    "\"rate_Hz\":250000,"
    "\"freq_Hz\":433920000,"
    "\"rssi_dB\":-8.2,"
    "\"snr_dB\":22.1,"
    "\"mod\":\"OOK\","
    "\"pulses\":[500,500,1000,500,500,1000,500,1500,500,500]"
"}";

// Минимальный JSON
const char *test_minimal_json = "{"
    "\"rate_Hz\":250000,"
    "\"freq_Hz\":433920000"
"}";

// === УТИЛИТЫ ===

void print_pulse_data(const pulse_data_t *pulse_data, const char *title)
{
    printf("\n=== %s ===\n", title);
    printf("Sample rate: %u Hz\n", pulse_data->sample_rate);
    printf("Center frequency: %.1f Hz\n", pulse_data->centerfreq_hz);
    printf("RSSI: %.1f dB\n", pulse_data->rssi_db);
    printf("SNR: %.1f dB\n", pulse_data->snr_db);
    printf("Noise: %.1f dB\n", pulse_data->noise_db);
    printf("Number of pulses: %u\n", pulse_data->num_pulses);
    
    if (pulse_data->freq1_hz != 0 || pulse_data->freq2_hz != 0) {
        printf("FSK frequencies: %.1f Hz, %.1f Hz\n", pulse_data->freq1_hz, pulse_data->freq2_hz);
        printf("FSK estimates: %d, %d\n", pulse_data->fsk_f1_est, pulse_data->fsk_f2_est);
    }
    
    printf("OOK estimates: %d (low), %d (high)\n", 
           pulse_data->ook_low_estimate, pulse_data->ook_high_estimate);
    
    if (pulse_data->num_pulses > 0) {
        printf("Pulse timing: start_ago=%u, end_ago=%u\n", 
               pulse_data->start_ago, pulse_data->end_ago);
        
        printf("First 6 pulses: ");
        for (unsigned i = 0; i < pulse_data->num_pulses && i < 6; i++) {
            printf("[%u,%u] ", pulse_data->pulse[i], pulse_data->gap[i]);
        }
        printf("\n");
    }
}

// === ТЕСТЫ ===

int test_json_reconstruction()
{
    printf("\n🧪 Test 1: JSON reconstruction (Toyota TPMS)\n");
    
    pulse_data_t pulse_data;
    int result = rtl433_signal_reconstruct_from_json(test_toyota_json, &pulse_data);
    
    if (result != 0) {
        printf("❌ FAILED: reconstruct_from_json returned %d\n", result);
        return -1;
    }
    
    print_pulse_data(&pulse_data, "Toyota TPMS Signal");
    
    // Проверки
    assert(pulse_data.sample_rate == 250000);
    assert(pulse_data.centerfreq_hz == 433920000.0f);
    assert(pulse_data.freq1_hz == 433918500.0f);
    assert(pulse_data.freq2_hz == 433921500.0f);
    assert(pulse_data.rssi_db == -12.5f);
    assert(pulse_data.snr_db == 15.3f);
    assert(pulse_data.num_pulses == 6); // 12 элементов / 2
    assert(pulse_data.pulse[0] == 1200);
    assert(pulse_data.gap[0] == 800);
    assert(pulse_data.fsk_f1_est == -1500); // 433918500 - 433920000
    assert(pulse_data.fsk_f2_est == 1500);  // 433921500 - 433920000
    
    printf("✅ PASSED: All checks successful\n");
    return 0;
}

int test_ook_reconstruction()
{
    printf("\n🧪 Test 2: OOK signal reconstruction\n");
    
    pulse_data_t pulse_data;
    int result = rtl433_signal_reconstruct_from_json(test_ook_json, &pulse_data);
    
    if (result != 0) {
        printf("❌ FAILED: reconstruct_from_json returned %d\n", result);
        return -1;
    }
    
    print_pulse_data(&pulse_data, "OOK Signal");
    
    // Проверки OOK сигнала
    assert(pulse_data.sample_rate == 250000);
    assert(pulse_data.centerfreq_hz == 433920000.0f);
    assert(pulse_data.rssi_db == -8.2f);
    assert(pulse_data.snr_db == 22.1f);
    assert(pulse_data.num_pulses == 5); // 10 элементов / 2
    assert(pulse_data.fsk_f1_est == 0); // OOK не использует FSK
    assert(pulse_data.fsk_f2_est == 0);
    assert(pulse_data.ook_low_estimate == 1000);
    assert(pulse_data.ook_high_estimate == 8000);
    
    printf("✅ PASSED: OOK signal reconstruction successful\n");
    return 0;
}

int test_minimal_json()
{
    printf("\n🧪 Test 3: Minimal JSON\n");
    
    pulse_data_t pulse_data;
    int result = rtl433_signal_reconstruct_from_json(test_minimal_json, &pulse_data);
    
    if (result != 0) {
        printf("❌ FAILED: minimal JSON reconstruction failed\n");
        return -1;
    }
    
    print_pulse_data(&pulse_data, "Minimal Signal");
    
    // Базовые проверки
    assert(pulse_data.sample_rate == 250000);
    assert(pulse_data.centerfreq_hz == 433920000.0f);
    assert(pulse_data.num_pulses == 0); // Нет массива pulses
    
    printf("✅ PASSED: Minimal JSON handled correctly\n");
    return 0;
}

int test_error_cases()
{
    printf("\n🧪 Test 4: Error handling\n");
    
    pulse_data_t pulse_data;
    
    // NULL указатели
    assert(rtl433_signal_reconstruct_from_json(NULL, &pulse_data) == -1);
    assert(rtl433_signal_reconstruct_from_json(test_toyota_json, NULL) == -1);
    
    // Невалидный JSON
    assert(rtl433_signal_reconstruct_from_json("{invalid json", &pulse_data) == -1);
    
    // Пустая строка
    assert(rtl433_signal_reconstruct_from_json("", &pulse_data) == -1);
    
    printf("✅ PASSED: Error cases handled correctly\n");
    return 0;
}

int test_metadata_copy()
{
    printf("\n🧪 Test 5: Metadata copying\n");
    
    pulse_data_t src, dst;
    
    // Подготовка исходных данных
    rtl433_signal_reconstruct_from_json(test_toyota_json, &src);
    memset(&dst, 0, sizeof(dst));
    
    // Копирование метаданных
    int result = rtl433_signal_copy_metadata(&src, &dst);
    assert(result == 0);
    
    // Проверки
    assert(dst.sample_rate == src.sample_rate);
    assert(dst.centerfreq_hz == src.centerfreq_hz);
    assert(dst.rssi_db == src.rssi_db);
    assert(dst.snr_db == src.snr_db);
    assert(dst.fsk_f1_est == src.fsk_f1_est);
    assert(dst.fsk_f2_est == src.fsk_f2_est);
    
    printf("✅ PASSED: Metadata copy successful\n");
    return 0;
}

int test_timing_calculation()
{
    printf("\n🧪 Test 6: Timing parameter calculation\n");
    
    pulse_data_t pulse_data;
    rtl433_signal_reconstruct_from_json(test_toyota_json, &pulse_data);
    
    // Проверяем что start_ago рассчитан правильно
    int expected_total = 0;
    for (unsigned i = 0; i < pulse_data.num_pulses; i++) {
        expected_total += pulse_data.pulse[i] + pulse_data.gap[i];
    }
    
    assert(pulse_data.start_ago == expected_total);
    assert(pulse_data.end_ago == 0);
    
    printf("Expected total time: %d samples\n", expected_total);
    printf("Actual start_ago: %u samples\n", pulse_data.start_ago);
    printf("✅ PASSED: Timing calculation correct\n");
    return 0;
}

// === MAIN ===

int main()
{
    printf("🚀 Testing RTL433 signal reconstruction functions\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    int failed = 0;
    
    // Запуск тестов
    if (test_json_reconstruction() != 0) failed++;
    if (test_ook_reconstruction() != 0) failed++;
    if (test_minimal_json() != 0) failed++;
    if (test_error_cases() != 0) failed++;
    if (test_metadata_copy() != 0) failed++;
    if (test_timing_calculation() != 0) failed++;
    
    // Итоги
    printf("\n═══════════════════════════════════════════════════════\n");
    if (failed == 0) {
        printf("🎉 ALL TESTS PASSED SUCCESSFULLY! (6/6)\n");
        printf("✅ Signal reconstruction functions work correctly\n");
        return 0;
    } else {
        printf("❌ FAILED %d TESTS out of 6\n", failed);
        return 1;
    }
}
