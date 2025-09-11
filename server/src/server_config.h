/** @file
    Конфигурация для rtl_433_server.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <stdint.h>
#include "list.h"

/// Типы входящих транспортов
typedef enum {
    INPUT_TRANSPORT_HTTP,
    INPUT_TRANSPORT_MQTT,
    INPUT_TRANSPORT_RABBITMQ,
    INPUT_TRANSPORT_TCP,
    INPUT_TRANSPORT_UDP
} input_transport_type_t;

/// Типы выходных очередей
typedef enum {
    OUTPUT_QUEUE_MQTT,
    OUTPUT_QUEUE_RABBITMQ,
    OUTPUT_QUEUE_HTTP,
    OUTPUT_QUEUE_FILE,
    OUTPUT_QUEUE_DATABASE,
    OUTPUT_QUEUE_WEBSOCKET
} output_queue_type_t;

/// Конфигурация входящего транспорта
typedef struct input_transport_config {
    input_transport_type_t type;
    char *host;
    int port;
    char *username;
    char *password;
    char *topic_queue;
    char *exchange;
    int ssl_enabled;
    char *ca_cert_path;
    char *client_cert_path;
    char *client_key_path;
    int max_connections;
    int timeout_ms;
} input_transport_config_t;

/// Конфигурация выходной очереди
typedef struct output_queue_config {
    output_queue_type_t type;
    char *name;
    char *host;
    int port;
    char *username;
    char *password;
    char *topic_queue;
    char *exchange;
    char *routing_key;
    char *file_path;
    char *db_path;
    int ssl_enabled;
    char *ca_cert_path;
    char *client_cert_path;
    char *client_key_path;
    int batch_size;
    int batch_timeout_ms;
    int max_queue_size;
} output_queue_config_t;

/// Конфигурация декодера устройств
typedef struct decoder_config {
    int device_id;
    char *device_name;
    int enabled;
    int priority;
    char *parameters;  // Дополнительные параметры для декодера
} decoder_config_t;

/// Конфигурация сервера
typedef struct server_config {
    // Входящие транспорты
    list_t input_transports;
    
    // Выходные очереди
    output_queue_config_t ready_queue;      // Очередь для распознанных устройств
    output_queue_config_t unknown_queue;    // Очередь для неизвестных сигналов
    
    // Декодеры устройств
    list_t decoders;
    
    // Настройки обработки
    int max_concurrent_signals;    // Максимальное количество одновременно обрабатываемых сигналов
    int signal_timeout_ms;         // Таймаут обработки одного сигнала
    int batch_processing;          // Включить пакетную обработку
    int batch_size;                // Размер пакета
    int batch_timeout_ms;          // Таймаут пакета
    
    // Настройки базы данных для неизвестных сигналов
    char *unknown_signals_db_path;
    int db_retention_days;         // Время хранения неизвестных сигналов
    int db_max_size_mb;            // Максимальный размер базы данных
    
    // Веб-интерфейс
    int web_enabled;
    char *web_host;
    int web_port;
    char *web_static_path;
    
    // Логирование
    int verbosity;
    char *log_file;
    int log_rotation;
    int log_max_size_mb;
    int log_max_files;
    
    // Мониторинг и статистика
    int stats_enabled;
    int stats_interval_sec;
    char *stats_output_file;
    
    // Режимы работы
    int daemon_mode;
    char *pid_file;
    char *user;
    char *group;
    
    // Производительность
    int worker_threads;            // Количество рабочих потоков
    int io_threads;                // Количество I/O потоков
    int queue_buffer_size;         // Размер буферов очередей
} server_config_t;

/// Инициализировать конфигурацию сервера с значениями по умолчанию
void server_config_init(server_config_t *cfg);

/// Освободить память конфигурации
void server_config_free(server_config_t *cfg);

/// Загрузить конфигурацию из файла
int server_config_load_file(server_config_t *cfg, const char *filename);

/// Парсить аргументы командной строки
int server_config_parse_args(server_config_t *cfg, int argc, char **argv);

/// Печать справки
void server_config_print_help(const char *program_name);

/// Валидация конфигурации
int server_config_validate(const server_config_t *cfg);

/// Добавить входящий транспорт
int server_config_add_input_transport(server_config_t *cfg, const input_transport_config_t *transport);

/// Добавить декодер устройства
int server_config_add_decoder(server_config_t *cfg, const decoder_config_t *decoder);

/// Найти конфигурацию декодера по ID
decoder_config_t *server_config_find_decoder(const server_config_t *cfg, int device_id);

/// Загрузить декодеры из конфигурационного файла
int server_config_load_decoders(server_config_t *cfg, const char *filename);

/// Сохранить конфигурацию в файл
int server_config_save_file(const server_config_t *cfg, const char *filename);

#endif // SERVER_CONFIG_H

