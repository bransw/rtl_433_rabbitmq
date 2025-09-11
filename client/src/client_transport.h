/** @file
    Транспортный слой для rtl_433_client.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef CLIENT_TRANSPORT_H
#define CLIENT_TRANSPORT_H

#include "client_config.h"
#include "pulse_data.h"
#include "bitbuffer.h"
#include <time.h>

/// Структура для передачи демодулированных данных
typedef struct demod_data {
    char *device_id;           // Идентификатор устройства-источника
    uint64_t timestamp_us;     // Временная метка в микросекундах
    uint32_t frequency;        // Частота приема
    uint32_t sample_rate;      // Частота дискретизации
    float rssi_db;             // Уровень сигнала в dB
    float snr_db;              // Отношение сигнал/шум в dB
    float noise_db;            // Уровень шума в dB
    
    // Данные о пульсах
    pulse_data_t pulse_data;
    
    // Альтернативно - битовый буфер (для FSK)
    bitbuffer_t *bitbuffer;
    
    // Метаданные
    int ook_detected;          // 1 если обнаружен OOK сигнал
    int fsk_detected;          // 1 если обнаружен FSK сигнал
    char *raw_data_hex;        // Сырые данные в hex (опционально)
} demod_data_t;

/// Структура транспортного соединения
typedef struct transport_connection {
    transport_config_t *config;
    void *connection_data;     // Указатель на специфичные для транспорта данные
    int connected;
    time_t last_reconnect_attempt;
} transport_connection_t;

/// Инициализировать транспортное соединение
int transport_init(transport_connection_t *conn, transport_config_t *config);

/// Подключиться к серверу
int transport_connect(transport_connection_t *conn);

/// Отключиться от сервера
void transport_disconnect(transport_connection_t *conn);

/// Отправить демодулированные данные
int transport_send_demod_data(transport_connection_t *conn, const demod_data_t *data);

/// Отправить pulse data
int transport_send_pulse_data(transport_connection_t *conn, const pulse_data_t *pulse_data);

/// Отправить батч демодулированных данных
int transport_send_demod_batch(transport_connection_t *conn, const demod_data_t *data_array, int count);

/// Проверить состояние соединения
int transport_is_connected(const transport_connection_t *conn);

/// Освободить ресурсы транспорта
void transport_cleanup(transport_connection_t *conn);

/// Создать структуру demod_data
demod_data_t *demod_data_create(void);

/// Освободить структуру demod_data
void demod_data_free(demod_data_t *data);

/// Копировать pulse_data в demod_data
void demod_data_set_pulse_data(demod_data_t *data, const pulse_data_t *pulse_data);

/// Копировать bitbuffer в demod_data
void demod_data_set_bitbuffer(demod_data_t *data, const bitbuffer_t *bitbuffer);

/// Сериализовать demod_data в JSON
char *demod_data_to_json(const demod_data_t *data);

/// Сериализовать массив demod_data в JSON
char *demod_data_batch_to_json(const demod_data_t *data_array, int count);

// Специфичные функции для каждого транспорта

#ifdef ENABLE_MQTT
/// MQTT специфичные функции
int transport_mqtt_init(transport_connection_t *conn);
int transport_mqtt_connect(transport_connection_t *conn);
int transport_mqtt_send(transport_connection_t *conn, const char *json_data);
void transport_mqtt_cleanup(transport_connection_t *conn);
#endif

/// HTTP специфичные функции
int transport_http_init(transport_connection_t *conn);
int transport_http_send(transport_connection_t *conn, const char *json_data);
void transport_http_cleanup(transport_connection_t *conn);

#ifdef ENABLE_RABBITMQ
/// RabbitMQ специфичные функции
int transport_rabbitmq_init(transport_connection_t *conn);
int transport_rabbitmq_connect(transport_connection_t *conn);
int transport_rabbitmq_send(transport_connection_t *conn, const char *json_data);
void transport_rabbitmq_cleanup(transport_connection_t *conn);
#endif

/// TCP/UDP специфичные функции
int transport_socket_init(transport_connection_t *conn);
int transport_socket_connect(transport_connection_t *conn);
int transport_socket_send(transport_connection_t *conn, const char *json_data);
void transport_socket_cleanup(transport_connection_t *conn);

#endif // CLIENT_TRANSPORT_H

