#ifndef INCLUDE_RTL433_SIGNAL_FORMAT_H_
#define INCLUDE_RTL433_SIGNAL_FORMAT_H_

#include "pulse_data.h"
#include "bitbuffer.h"
#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates optimized signal data structure for RabbitMQ transport.
 * 
 * Combines bitbuffer_t (for decoding) with essential metadata (RSSI, frequencies)
 * and backup pulse data (hex_string, timings) for complete signal reconstruction.
 * 
 * @param pulse_data Original pulse data with metadata
 * @param bitbuffer Processed bitbuffer ready for decoders
 * @return data_t* JSON-compatible data structure, or NULL on failure
 */
data_t* rtl433_create_signal_data(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer);

/**
 * @brief Creates optimized signal data structure with custom hex_string.
 * 
 * @param pulse_data Original pulse data with metadata
 * @param bitbuffer Processed bitbuffer ready for decoders
 * @param real_hex_string Custom hex_string to use (if NULL, generates automatically)
 * @return data_t* JSON-compatible data structure, or NULL on failure
 */
data_t* rtl433_create_signal_data_with_hex(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer, const char *real_hex_string);

/**
 * @brief Prints formatted signal data information to stderr.
 * 
 * Shows a nice formatted summary of the signal data that would be sent
 * to RabbitMQ, including bitbuffer info, metadata, and sample JSON.
 * 
 * @param pulse_data Original pulse data
 * @param bitbuffer Processed bitbuffer
 */
void rtl433_print_signal_data(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer);

/**
 * @brief Prints formatted signal data information with custom hex_string.
 * 
 * @param pulse_data Original pulse data
 * @param bitbuffer Processed bitbuffer
 * @param real_hex_string Custom hex_string to use (if NULL, generates automatically)
 */
void rtl433_print_signal_data_with_hex(pulse_data_t const *pulse_data, bitbuffer_t const *bitbuffer, const char *real_hex_string);

/**
 * @brief Reset message ID counter (for testing)
 */
void rtl433_reset_msg_id(void);

/**
 * @brief Get current message ID (for display purposes)
 * @return Current message ID that will be used for next signal
 */
unsigned rtl433_get_current_msg_id(void);

/**
 * @brief Increment message ID (called once per signal)
 */
void rtl433_increment_msg_id(void);

/**
 * @brief Set current pulse_data_t for decoder access
 * @param pulse_data Pointer to current pulse_data_t being processed
 */
void rtl433_set_current_pulse_data(pulse_data_t const *pulse_data);

/**
 * @brief Get current pulse_data_t in decoders
 * @return Pointer to current pulse_data_t or NULL if not set
 */
pulse_data_t const *rtl433_get_current_pulse_data(void);

/**
 * @brief Create simple bitbuffer-only data for RabbitMQ (no hex_string complexity)
 * 
 * This is the SIMPLE approach - directly serialize bitbuffer_t without
 * complex pulse reconstruction. Perfect for multi-signal handling.
 * 
 * @param bitbuffer Processed bitbuffer ready for decoders
 * @param pulse_data Optional pulse data for metadata (can be NULL)
 * @return data_t* JSON-compatible data structure, or NULL on failure
 */
data_t *rtl433_create_bitbuffer_data(bitbuffer_t const *bitbuffer, pulse_data_t const *pulse_data);


/**
 * @brief Parse JSON string back to bitbuffer_t (for RabbitMQ consumer)
 * 
 * This is the REVERSE operation - takes our simple JSON format and
 * reconstructs the bitbuffer_t for direct decoder usage.
 * 
 * @param json_string JSON string in bitbuffer_simple format
 * @param bitbuffer Output bitbuffer structure
 * @param pulse_metadata Optional pulse metadata output (can be NULL)
 * @return 0 on success, -1 on error
 */
int rtl433_parse_bitbuffer_data(const char *json_string, bitbuffer_t *bitbuffer, pulse_data_t *pulse_metadata);

/**
 * @brief Check if RabbitMQ output is configured
 * 
 * This function checks if the application has RabbitMQ output enabled
 * via the -F rabbitmq://... parameter.
 * 
 * @return 1 if RabbitMQ output is configured, 0 otherwise
 */
int rtl433_has_rabbitmq_output(void);
int rtl433_has_rabbitmq_input(void);
void rtl433_set_rabbitmq_input_enabled(int enabled);
int rtl433_has_rabbitmq_active(void);

/**
 * @brief Send bitbuffer to RabbitMQ (integrated with existing output system)
 * 
 * This function will be called from pulse_slicer after each successful
 * bitbuffer generation to send the data to RabbitMQ for processing.
 * Only sends if RabbitMQ output is configured via -F parameter.
 * 
 * @param bitbuffer Processed bitbuffer ready for decoders
 * @param pulse_data Optional pulse metadata (can be NULL)
 * @return 0 on success, -1 on error
 */
int rtl433_send_bitbuffer_to_rabbitmq(bitbuffer_t const *bitbuffer, pulse_data_t const *pulse_data);


/**
 * @brief Print bitbuffer data (debug function)
 * @param bitbuffer The bitbuffer to print
 * @param pulse_data The associated pulse data
 */
void rtl433_print_bitbuffer_data(bitbuffer_t const *bitbuffer, pulse_data_t const *pulse_data);

/**
 * @brief Enable RabbitMQ output flag
 */
void rtl433_enable_rabbitmq_output(void);

/**
 * @brief Check if RabbitMQ output is enabled
 * @return 1 if enabled, 0 if disabled
 */
int rtl433_has_rabbitmq_output(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_RTL433_SIGNAL_FORMAT_H_ */
