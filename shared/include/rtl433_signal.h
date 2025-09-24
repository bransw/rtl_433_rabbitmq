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

/// Reconstruct pulse_data from hex string with full metadata
int rtl433_signal_reconstruct_from_hex(const char *hex_str, 
                                      pulse_data_t *pulse_data,
                                      const char *json_metadata);

/// Reconstruct pulse_data from JSON
int rtl433_signal_reconstruct_from_json(const char *json_str, pulse_data_t *pulse_data);

/// Copy all critical pulse_data fields
int rtl433_signal_copy_metadata(const pulse_data_t *src, pulse_data_t *dst);

/// Validate pulse_data for proper decoder operation
bool rtl433_signal_validate_pulse_data(const pulse_data_t *pulse_data);

/// Set default values for pulse_data
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

/// Wrapper for rfraw_parse
int rtl433_signal_rfraw_parse(pulse_data_t *pulse_data, const char *hex_str);

/// Wrapper for rfraw_check
bool rtl433_signal_rfraw_check(const char *hex_str);

#ifdef __cplusplus
}
#endif

#endif // RTL433_SIGNAL_H
