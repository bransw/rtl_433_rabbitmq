/** @file
 * Common transport module for rtl_433 client and server
 * 
 * This module provides a unified transport layer implementation
 * to ensure consistency between client and server.
 */

#ifndef RTL433_TRANSPORT_H
#define RTL433_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include "pulse_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Transport types
typedef enum {
    RTL433_TRANSPORT_RABBITMQ,
    RTL433_TRANSPORT_MQTT,
    RTL433_TRANSPORT_TCP,
    RTL433_TRANSPORT_UDP
} rtl433_transport_type_t;

/// Конфигурация транспорта
typedef struct {
    rtl433_transport_type_t type;
    char *host;
    int port;
    char *username;
    char *password;
    char *exchange;
    char *queue;
    char *topic;
    bool ssl_enabled;
    int timeout_ms;
    int reconnect_interval_s;
} rtl433_transport_config_t;

/// Соединение транспорта
typedef struct {
    rtl433_transport_config_t *config;
    void *connection_data;  // Специфичные для транспорта данные
    bool connected;
    int last_error;
} rtl433_transport_connection_t;

/// Структура сообщения для передачи
typedef struct {
    uint64_t package_id;
    char *modulation;       // "OOK" или "FSK"
    uint32_t timestamp;
    pulse_data_t *pulse_data;
    char *hex_string;       // Опциональная hex-строка или binary data
    void *metadata;         // Дополнительные метаданные
    size_t binary_data_size;  // Size of binary data in hex_string
    int is_binary;            // Flag: 1 = binary data, 0 = text/JSON
} rtl433_message_t;

/// Callback для обработки полученных сообщений
typedef void (*rtl433_message_handler_t)(rtl433_message_t *message, void *user_data);

// === ОСНОВНЫЕ ФУНКЦИИ ТРАНСПОРТА ===

/// Инициализация конфигурации транспорта
int rtl433_transport_config_init(rtl433_transport_config_t *config);

/// Освобождение конфигурации транспорта
void rtl433_transport_config_free(rtl433_transport_config_t *config);

/// Парсинг URL транспорта (например: amqp://user:pass@host:port/queue)
int rtl433_transport_parse_url(const char *url, rtl433_transport_config_t *config);

/// Создание соединения
int rtl433_transport_connect(rtl433_transport_connection_t *conn, rtl433_transport_config_t *config);

/// Закрытие соединения
void rtl433_transport_disconnect(rtl433_transport_connection_t *conn);

/// Отправка сообщения
int rtl433_transport_send_message(rtl433_transport_connection_t *conn, rtl433_message_t *message);
int rtl433_transport_send_message_to_queue(rtl433_transport_connection_t *conn, rtl433_message_t *message, const char *queue_name);
int rtl433_transport_send_raw_json_to_queue(rtl433_transport_connection_t *conn, const char *json_data, const char *queue_name);

/// Send binary data to specific queue (for ASN.1 messages)
int rtl433_transport_send_binary_to_queue(rtl433_transport_connection_t *conn, const uint8_t *binary_data, size_t data_size, const char *queue_name);

/// Прием сообщений (для сервера)
int rtl433_transport_receive_messages(rtl433_transport_connection_t *conn, 
                                     rtl433_message_handler_t handler, 
                                     void *user_data, 
                                     int timeout_ms);

/// Проверка состояния соединения
bool rtl433_transport_is_connected(rtl433_transport_connection_t *conn);

/// Переподключение
int rtl433_transport_reconnect(rtl433_transport_connection_t *conn);

// === ФУНКЦИИ РАБОТЫ С СООБЩЕНИЯМИ ===

/// Создание сообщения из pulse_data
rtl433_message_t* rtl433_message_create_from_pulse_data(pulse_data_t *pulse_data, 
                                                       const char *modulation,
                                                       uint64_t package_id);

/// Создание сообщения из JSON
rtl433_message_t* rtl433_message_create_from_json(const char *json_str);

/// Сериализация сообщения в JSON
char* rtl433_message_to_json(rtl433_message_t *message);

/// Enhanced pulse data to JSON conversion (includes all fields for signal reconstruction)
char* rtl433_pulse_data_to_enhanced_json(pulse_data_t const *data);

/// Освобождение сообщения
void rtl433_message_free(rtl433_message_t *message);

// === СТАТИСТИКА И МОНИТОРИНГ ===

/// Статистика транспорта
typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t send_errors;
    uint64_t receive_errors;
    uint64_t reconnections;
    uint32_t last_error_code;
    char last_error_message[256];
} rtl433_transport_stats_t;

/// Получение статистики
void rtl433_transport_get_stats(rtl433_transport_connection_t *conn, rtl433_transport_stats_t *stats);

/// Сброс статистики
void rtl433_transport_reset_stats(rtl433_transport_connection_t *conn);

// === УТИЛИТЫ ===

/// Генерация уникального ID пакета
uint64_t rtl433_generate_package_id(void);

/// Получение строкового описания типа транспорта
const char* rtl433_transport_type_to_string(rtl433_transport_type_t type);

/// Получение типа транспорта из строки
rtl433_transport_type_t rtl433_transport_type_from_string(const char *type_str);

#ifdef __cplusplus
}
#endif

#endif // RTL433_TRANSPORT_H
