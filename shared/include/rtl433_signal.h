/** @file
 * Common signal processing module for rtl_433
 * 
 * Contains functions for signal reconstruction, analysis and processing,
 * shared between client and server.
 */

#ifndef RTL433_SIGNAL_H
#define RTL433_SIGNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "pulse_data.h"
#include "bitbuffer.h"
#include "list.h"

#ifdef __cplusplus
extern "C" {
#endif

// === SIGNAL RECONSTRUCTION ===

/**
 * Reconstruct pulse_data from hex string with full metadata
 * 
 * ВАЖНО: Функция ПОЛНОСТЬЮ ОЧИЩАЕТ pulse_data через memset()!
 * Все существующие метаданные будут потеряны.
 * 
 * @param hex_str        ASCII hex-строка в rfraw формате (например, "AAB10400...")
 *                       Должна начинаться с 0xAAB0 или 0xAAB1
 * @param pulse_data     Выходная структура для восстановленных пульсов (будет очищена)
 * @param json_metadata  JSON-строка с метаданными (может быть NULL)
 *                       Поддерживаемые поля: freq_Hz, rate_Hz, rssi_dB, snr_dB, 
 *                       noise_dB, mod, pulses
 * @return 0 при успехе, -1 при ошибке
 * 
 * Алгоритм:
 * 1. memset(pulse_data, 0) - УНИЧТОЖАЕТ ВСЕ ДАННЫЕ!
 * 2. rtl433_signal_set_defaults() - устанавливает sample_rate=250000
 * 3. rfraw_check() - проверяет формат hex-строки
 * 4. rfraw_parse() - извлекает пульсы (устанавливает sample_rate=1000000!)
 * 5. rtl433_signal_reconstruct_from_json() - применяет метаданные из JSON
 * 
 * НЕ ИСПОЛЬЗОВАТЬ если pulse_data уже содержит метаданные!
 * Для таких случаев вызывайте rfraw_parse() напрямую.
 */
int rtl433_signal_reconstruct_from_hex(const char *hex_str, 
                                      pulse_data_t *pulse_data,
                                      const char *json_metadata);

/**
 * Reconstruct pulse_data from JSON
 * 
 * Применяет метаданные из JSON к существующей структуре pulse_data.
 * НЕ очищает pulse_data, только обновляет поля.
 * 
 * @param json_str    JSON-строка с метаданными
 * @param pulse_data  Структура для заполнения (существующие данные сохраняются)
 * @return 0 при успехе, -1 при ошибке
 * 
 * Поддерживаемые поля JSON:
 * - rate_Hz (uint32_t) → sample_rate
 * - freq_Hz (double) → centerfreq_hz
 * - freq1_Hz (double) → freq1_hz
 * - freq2_Hz (double) → freq2_hz
 * - rssi_dB (double) → rssi_db
 * - snr_dB (double) → snr_db
 * - noise_dB (double) → noise_db
 * - mod (string: "FSK"/"OOK") → устанавливает FSK/OOK параметры
 * - pulses (array) → pulse[] и gap[] массивы
 */
int rtl433_signal_reconstruct_from_json(const char *json_str, pulse_data_t *pulse_data);

/**
 * Copy all critical pulse_data fields
 * 
 * Копирует ВСЕ метаданные (но НЕ сами пульсы) из src в dst.
 * 
 * @param src  Источник метаданных
 * @param dst  Назначение для копирования
 * @return 0 при успехе, -1 при ошибке
 * 
 * Копируемые поля:
 * - offset, sample_rate, depth_bits
 * - start_ago, end_ago
 * - ook_low_estimate, ook_high_estimate
 * - fsk_f1_est, fsk_f2_est
 * - freq1_hz, freq2_hz, centerfreq_hz
 * - range_db, rssi_db, snr_db, noise_db
 * 
 * НЕ копируются: num_pulses, pulse[], gap[]
 */
int rtl433_signal_copy_metadata(const pulse_data_t *src, pulse_data_t *dst);

/**
 * Validate pulse_data for proper decoder operation
 * 
 * Проверяет корректность структуры pulse_data перед декодированием.
 * 
 * @param pulse_data  Структура для проверки
 * @return true если валидна, false иначе
 * 
 * Проверки:
 * - sample_rate != 0
 * - 0 < num_pulses <= PD_MAX_PULSES
 * - Все пульсы и гэпы >= 0
 * - Пульсы в разумных пределах: 1 мкс < pulse < 100 мс
 */
bool rtl433_signal_validate_pulse_data(const pulse_data_t *pulse_data);

/**
 * Set default values for pulse_data
 * 
 * Устанавливает значения по умолчанию для структуры pulse_data.
 * 
 * @param pulse_data   Структура для инициализации
 * @param sample_rate  Частота дискретизации (0 = использовать 250000)
 * 
 * Устанавливаемые значения:
 * - sample_rate = параметр или 250000
 * - depth_bits = 8
 * - offset = 0
 * - start_ago = 0, end_ago = 0
 * - ook_low_estimate = 1000, ook_high_estimate = 8000
 * - fsk_f1_est = 0, fsk_f2_est = 0
 * - range_db = 0.0
 */
void rtl433_signal_set_defaults(pulse_data_t *pulse_data, uint32_t sample_rate);

// === АНАЛИЗ СИГНАЛОВ ===

/// Determine modulation type from pulse_data
typedef enum {
    RTL433_MODULATION_UNKNOWN,
    RTL433_MODULATION_OOK,
    RTL433_MODULATION_FSK
} rtl433_modulation_type_t;

/// Automatic modulation type detection
rtl433_modulation_type_t rtl433_signal_detect_modulation(const pulse_data_t *pulse_data);

/// Signal quality analysis
typedef struct {
    float snr_db;
    float rssi_db;
    float noise_db;
    int pulse_count;
    float avg_pulse_width_us;
    float avg_gap_width_us;
    bool is_valid;
} rtl433_signal_quality_t;

/// Calculate signal quality
int rtl433_signal_analyze_quality(const pulse_data_t *pulse_data, rtl433_signal_quality_t *quality);

// === ДЕКОДИРОВАНИЕ ===

/// Decoding result
typedef struct {
    int devices_decoded;
    int fsk_events;
    int ook_events;
    char **device_names;    // Список имен декодированных устройств
    void **device_data;     // Данные устройств (data_t*)
} rtl433_decode_result_t;

/// Decode signal using original rtl_433 functions
int rtl433_signal_decode(const pulse_data_t *pulse_data, 
                        list_t *r_devs,
                        rtl433_decode_result_t *result);

/// Decode FSK signals only
int rtl433_signal_decode_fsk(const pulse_data_t *pulse_data, 
                            list_t *r_devs,
                            rtl433_decode_result_t *result);

/// Decode OOK signals only
int rtl433_signal_decode_ook(const pulse_data_t *pulse_data,
                            list_t *r_devs, 
                            rtl433_decode_result_t *result);

/// Free decoding result
void rtl433_decode_result_free(rtl433_decode_result_t *result);

// === ГЕНЕРАЦИЯ HEX-СТРОК ===

/// Generate hex string from pulse_data (triq.org compatible)
int rtl433_signal_generate_hex_string(const pulse_data_t *pulse_data, char *hex_buffer, size_t buffer_size);

/// Check hex string validity
bool rtl433_signal_validate_hex_string(const char *hex_str);

// === БИТБУФЕР ===

/// Convert pulse_data to bitbuffer for decoders
int rtl433_signal_pulse_to_bitbuffer(const pulse_data_t *pulse_data, 
                                    bitbuffer_t *bitbuffer,
                                    const void *r_device);

/// Optimized conversion for FSK signals
int rtl433_signal_fsk_pulse_to_bitbuffer(const pulse_data_t *pulse_data,
                                        bitbuffer_t *bitbuffer,
                                        const void *r_device);

/// Optimized conversion for OOK signals
int rtl433_signal_ook_pulse_to_bitbuffer(const pulse_data_t *pulse_data,
                                        bitbuffer_t *bitbuffer, 
                                        const void *r_device);

// === УТИЛИТЫ ===

/// Calculate signal time in microseconds
uint32_t rtl433_signal_duration_us(const pulse_data_t *pulse_data);

/// Convert time from samples to microseconds
float rtl433_signal_samples_to_us(int samples, uint32_t sample_rate);

/// Convert time from microseconds to samples
int rtl433_signal_us_to_samples(float us, uint32_t sample_rate);

/// Normalize pulse_data (remove zero pulses etc.)
int rtl433_signal_normalize(pulse_data_t *pulse_data);

/// Print debug information about pulse_data
void rtl433_signal_debug_print(const pulse_data_t *pulse_data, const char *prefix);

// === СОВМЕСТИМОСТЬ С ОРИГИНАЛЬНЫМ RTL_433 ===

/// Use original rtl_433 functions for maximum compatibility
#include "pulse_slicer.h"
#include "pulse_analyzer.h"
#include "rfraw.h"

/// Wrapper for pulse_slicer_pcm
int rtl433_signal_slice_pcm(const pulse_data_t *pulse_data, void *r_dev);

/// Wrapper for pulse_analyzer
int rtl433_signal_analyze(const pulse_data_t *pulse_data, rtl433_modulation_type_t mod_type, void *r_dev);

/**
 * Wrapper for rfraw_parse
 * 
 * Обёртка для rfraw_parse() из rtl_433 core.
 * Извлекает массив пульсов из hex-строки rfraw формата.
 * 
 * @param pulse_data  Структура для заполнения пульсами
 * @param hex_str     ASCII hex-строка в rfraw формате
 * @return 0 при успехе, -1 при ошибке
 * 
 * ВАЖНО: rfraw_parse() ВСЕГДА устанавливает sample_rate = 1000000!
 * Если нужен другой sample_rate, восстановите его после вызова.
 */
int rtl433_signal_rfraw_parse(pulse_data_t *pulse_data, const char *hex_str);

/**
 * Wrapper for rfraw_check
 * 
 * Обёртка для rfraw_check() из rtl_433 core.
 * Проверяет корректность формата hex-строки.
 * 
 * @param hex_str  ASCII hex-строка для проверки
 * @return true если формат валиден, false иначе
 * 
 * Проверка: hex-строка должна начинаться с 0xAAB0 или 0xAAB1
 */
bool rtl433_signal_rfraw_check(const char *hex_str);

#ifdef __cplusplus
}
#endif

#endif // RTL433_SIGNAL_H
