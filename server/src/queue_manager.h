/** @file
    Менеджер очередей для rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "server_config.h"
#include "pulse_data.h"
#include "data.h"
#include <pthread.h>
#include <time.h>

/// Типы сообщений в очередях
typedef enum {
    QUEUE_MSG_DECODED_DEVICE,    // Успешно декодированное устройство
    QUEUE_MSG_UNKNOWN_SIGNAL,    // Неизвестный сигнал
    QUEUE_MSG_RAW_PULSE_DATA,    // Сырые данные пульсов
    QUEUE_MSG_STATISTICS,        // Статистика
    QUEUE_MSG_ERROR,             // Сообщение об ошибке
    QUEUE_MSG_HEARTBEAT          // Heartbeat сообщение
} queue_message_type_t;

/// Структура сообщения в очереди
typedef struct queue_message {
    queue_message_type_t type;
    uint64_t timestamp_us;
    char *source_id;             // Идентификатор источника (клиента)
    
    union {
        // Для декодированных устройств
        struct {
            char *device_name;
            int device_id;
            data_t *device_data;     // Декодированные данные устройства
            float confidence;        // Уверенность в декодировании (0.0-1.0)
            pulse_data_t pulse_data; // Исходные данные пульсов
        } decoded_device;
        
        // Для неизвестных сигналов
        struct {
            pulse_data_t pulse_data;
            char *raw_data_hex;      // Сырые данные в hex
            float signal_strength;   // Сила сигнала
            char *analysis_notes;    // Заметки анализа
        } unknown_signal;
        
        // Для статистики
        struct {
            char *stats_json;
        } statistics;
        
        // Для ошибок
        struct {
            int error_code;
            char *error_message;
        } error;
    } data;
    
    struct queue_message *next;  // Для связанного списка
} queue_message_t;

/// Структура очереди
typedef struct message_queue {
    char *name;
    output_queue_config_t *config;
    
    // Очередь сообщений
    queue_message_t *head;
    queue_message_t *tail;
    int size;
    int max_size;
    
    // Синхронизация
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    
    // Статистика
    uint64_t messages_sent;
    uint64_t messages_dropped;
    uint64_t bytes_sent;
    time_t created_time;
    time_t last_message_time;
    
    // Состояние
    int active;
    int connected;
    void *transport_data;        // Данные транспорта
    
    // Пакетная обработка
    queue_message_t **batch_buffer;
    int batch_size;
    int batch_count;
    time_t batch_start_time;
    
    pthread_t worker_thread;
} message_queue_t;

/// Менеджер очередей
typedef struct queue_manager {
    message_queue_t *ready_queue;     // Очередь для готовых устройств
    message_queue_t *unknown_queue;   // Очередь для неизвестных сигналов
    
    list_t additional_queues;         // Дополнительные очереди
    
    // Конфигурация
    server_config_t *config;
    
    // Синхронизация
    pthread_mutex_t manager_mutex;
    
    // Статистика
    uint64_t total_messages_processed;
    uint64_t total_bytes_processed;
    time_t start_time;
    
    int shutdown_requested;
} queue_manager_t;

/// Инициализировать менеджер очередей
int queue_manager_init(queue_manager_t *manager, server_config_t *config);

/// Запустить менеджер очередей
int queue_manager_start(queue_manager_t *manager);

/// Остановить менеджер очередей
void queue_manager_stop(queue_manager_t *manager);

/// Освободить ресурсы менеджера очередей
void queue_manager_cleanup(queue_manager_t *manager);

/// Проверить работает ли менеджер очередей
int queue_manager_is_running(const queue_manager_t *manager);

/// Отправить сообщение в очередь готовых устройств
int queue_manager_send_decoded_device(queue_manager_t *manager, 
                                     const char *source_id,
                                     const char *device_name,
                                     int device_id,
                                     data_t *device_data,
                                     float confidence,
                                     const pulse_data_t *pulse_data);

/// Отправить сообщение в очередь неизвестных сигналов
int queue_manager_send_unknown_signal(queue_manager_t *manager,
                                     const char *source_id,
                                     const pulse_data_t *pulse_data,
                                     const char *raw_data_hex,
                                     float signal_strength,
                                     const char *analysis_notes);

/// Отправить статистику
int queue_manager_send_statistics(queue_manager_t *manager,
                                 const char *source_id,
                                 const char *stats_json);

/// Отправить сообщение об ошибке
int queue_manager_send_error(queue_manager_t *manager,
                            const char *source_id,
                            int error_code,
                            const char *error_message);

/// Получить статистику очередей
char *queue_manager_get_statistics(queue_manager_t *manager);

/// Функции для работы с отдельными очередями

/// Создать очередь
message_queue_t *message_queue_create(const char *name, output_queue_config_t *config);

/// Уничтожить очередь
void message_queue_destroy(message_queue_t *queue);

/// Отправить сообщение в очередь
int message_queue_send(message_queue_t *queue, queue_message_t *message);

/// Получить сообщение из очереди (блокирующий вызов)
queue_message_t *message_queue_receive(message_queue_t *queue, int timeout_ms);

/// Проверить размер очереди
int message_queue_size(message_queue_t *queue);

/// Очистить очередь
void message_queue_clear(message_queue_t *queue);

/// Функции для работы с сообщениями

/// Создать сообщение
queue_message_t *queue_message_create(queue_message_type_t type);

/// Уничтожить сообщение
void queue_message_destroy(queue_message_t *message);

/// Сериализовать сообщение в JSON
char *queue_message_to_json(const queue_message_t *message);

/// Десериализовать сообщение из JSON
queue_message_t *queue_message_from_json(const char *json);

/// Скопировать сообщение
queue_message_t *queue_message_copy(const queue_message_t *message);

#endif // QUEUE_MANAGER_H

