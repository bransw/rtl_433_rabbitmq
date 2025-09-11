/** @file
    rtl_433_server - сервер для декодирования демодулированных сигналов.
    
    Copyright (C) 2024
    
    Основан на rtl_433 by Benjamin Larsson и других.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "server_config.h"
#include "server_transport.h"
#include "queue_manager.h"
#include "device_decoder.h"
#include "signal_processor.h"
#include "logger.h"
#include "r_util.h"

/// Глобальная конфигурация сервера
static server_config_t g_server_cfg;

/// Глобальный флаг завершения работы
static volatile sig_atomic_t exit_flag = 0;

/// Менеджер очередей
static queue_manager_t g_queue_manager;

/// Процессор сигналов
static signal_processor_t g_signal_processor;

/// Транспортный слой
static server_transport_t g_transport;

/// Счетчики статистики
static struct {
    uint64_t signals_received;
    uint64_t signals_processed;
    uint64_t devices_decoded;
    uint64_t unknown_signals;
    uint64_t processing_errors;
    time_t start_time;
    pthread_mutex_t mutex;
} g_stats = {0};

/// Обработчик сигналов
static void signal_handler(int sig)
{
    print_logf(LOG_INFO, "Server", "Received signal %d, shutting down...", sig);
    exit_flag = 1;
}

/// Инициализация сигналов
static void init_signals(void)
{
#ifndef _WIN32
    struct sigaction sigact;
    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGHUP, &sigact, NULL);
    signal(SIGPIPE, SIG_IGN); // Игнорируем SIGPIPE
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
}

/// Печать статистики
static void print_statistics(void)
{
    time_t now;
    time(&now);
    double uptime = difftime(now, g_stats.start_time);
    
    pthread_mutex_lock(&g_stats.mutex);
    
    print_logf(LOG_INFO, "Server", "=== Runtime Statistics ===");
    print_logf(LOG_INFO, "Server", "Uptime: %.0f sec", uptime);
    print_logf(LOG_INFO, "Server", "Signals received: %lu", g_stats.signals_received);
    print_logf(LOG_INFO, "Server", "Signals processed: %lu", g_stats.signals_processed);
    print_logf(LOG_INFO, "Server", "Devices decoded: %lu", g_stats.devices_decoded);
    print_logf(LOG_INFO, "Server", "Unknown signals: %lu", g_stats.unknown_signals);
    print_logf(LOG_INFO, "Server", "Processing errors: %lu", g_stats.processing_errors);
    
    if (uptime > 0) {
        print_logf(LOG_INFO, "Server", "Rate: %.1f signals/min", 
                  g_stats.signals_received * 60.0 / uptime);
        print_logf(LOG_INFO, "Server", "Recognition rate: %.1f%%", 
                  g_stats.devices_decoded * 100.0 / (g_stats.devices_decoded + g_stats.unknown_signals));
    }
    
    pthread_mutex_unlock(&g_stats.mutex);
    
    // Печатаем статистику очередей
    char *queue_stats = queue_manager_get_statistics(&g_queue_manager);
    if (queue_stats) {
        print_logf(LOG_INFO, "Server", "Queue statistics: %s", queue_stats);
        free(queue_stats);
    }
}

/// Обновить статистику
static void update_stats(int signals_received, int signals_processed, 
                        int devices_decoded, int unknown_signals, int errors)
{
    pthread_mutex_lock(&g_stats.mutex);
    g_stats.signals_received += signals_received;
    g_stats.signals_processed += signals_processed;
    g_stats.devices_decoded += devices_decoded;
    g_stats.unknown_signals += unknown_signals;
    g_stats.processing_errors += errors;
    pthread_mutex_unlock(&g_stats.mutex);
}

/// Callback для обработки входящих сигналов
static void signal_received_callback(const char *source_id, const char *json_data, void *user_data)
{
    if (!source_id || !json_data) {
        update_stats(0, 0, 0, 0, 1);
        return;
    }
    
    update_stats(1, 0, 0, 0, 0);
    
    if (g_server_cfg.verbosity >= LOG_DEBUG) {
        print_logf(LOG_DEBUG, "Server", "Received signal from %s: %.100s...", source_id, json_data);
    }
    
    // Передаем сигнал процессору для обработки
    if (signal_processor_process_json(&g_signal_processor, source_id, json_data) == 0) {
        update_stats(0, 1, 0, 0, 0);
    } else {
        update_stats(0, 0, 0, 0, 1);
        print_logf(LOG_WARNING, "Server", "Signal processing error from %s", source_id);
    }
}

/// Callback для результатов декодирования
static void device_decoded_callback(const char *source_id, const char *device_name, 
                                   int device_id, data_t *device_data, float confidence,
                                   const pulse_data_t *pulse_data, void *user_data)
{
    update_stats(0, 0, 1, 0, 0);
    
    if (g_server_cfg.verbosity >= LOG_INFO) {
        print_logf(LOG_INFO, "Server", "Decoded device: %s (ID:%d, confidence:%.2f) from %s", 
                  device_name, device_id, confidence, source_id);
    }
    
    // Отправляем в очередь готовых устройств
    queue_manager_send_decoded_device(&g_queue_manager, source_id, device_name, 
                                     device_id, device_data, confidence, pulse_data);
}

/// Callback для неизвестных сигналов
static void unknown_signal_callback(const char *source_id, const pulse_data_t *pulse_data,
                                   const char *raw_data_hex, float signal_strength,
                                   const char *analysis_notes, void *user_data)
{
    update_stats(0, 0, 0, 1, 0);
    
    if (g_server_cfg.verbosity >= LOG_DEBUG) {
        print_logf(LOG_DEBUG, "Server", "Unknown signal from %s (strength:%.1f dB)", 
                  source_id, signal_strength);
    }
    
    // Отправляем в очередь неизвестных сигналов
    queue_manager_send_unknown_signal(&g_queue_manager, source_id, pulse_data,
                                     raw_data_hex, signal_strength, analysis_notes);
}

/// Инициализация daemon режима
static int init_daemon_mode(void)
{
    if (!g_server_cfg.daemon_mode) {
        return 0;
    }
    
#ifndef _WIN32
    pid_t pid = fork();
    if (pid < 0) {
        print_log(LOG_ERROR, "Server", "Failed to create daemon process");
        return -1;
    }
    
    if (pid > 0) {
        // Родительский процесс завершается
        exit(0);
    }
    
    // Дочерний процесс становится daemon
    if (setsid() < 0) {
        print_log(LOG_ERROR, "Server", "Failed to create new session");
        return -1;
    }
    
    // Второй fork для предотвращения получения терминала
    pid = fork();
    if (pid < 0) {
        print_log(LOG_ERROR, "Server", "Second fork failed");
        return -1;
    }
    
    if (pid > 0) {
        exit(0);
    }
    
    // Закрываем стандартные потоки
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Записываем PID файл
    if (g_server_cfg.pid_file) {
        FILE *pid_file = fopen(g_server_cfg.pid_file, "w");
        if (pid_file) {
            fprintf(pid_file, "%d\n", getpid());
            fclose(pid_file);
        }
    }
    
    print_log(LOG_INFO, "Server", "Daemon mode activated");
#else
    print_log(LOG_WARNING, "Server", "Daemon mode not supported on Windows");
#endif
    
    return 0;
}

/// Главная функция
int main(int argc, char **argv)
{
    int ret = 0;
    
    print_log(LOG_INFO, "Server", "rtl_433_server started");
    
    // Инициализируем мьютекс статистики
    pthread_mutex_init(&g_stats.mutex, NULL);
    
    // Инициализируем конфигурацию
    server_config_init(&g_server_cfg);
    
    // Парсим аргументы командной строки
    if (server_config_parse_args(&g_server_cfg, argc, argv) != 0) {
        print_log(LOG_ERROR, "Server", "Command line argument error");
        ret = 1;
        goto cleanup;
    }
    
    // Валидируем конфигурацию
    if (server_config_validate(&g_server_cfg) != 0) {
        print_log(LOG_ERROR, "Server", "Configuration error");
        ret = 1;
        goto cleanup;
    }
    
    // Устанавливаем уровень логирования
    set_log_level(g_server_cfg.verbosity);
    
    // Инициализируем daemon режим если нужно
    if (init_daemon_mode() != 0) {
        ret = 2;
        goto cleanup;
    }
    
    // Инициализируем статистику
    time(&g_stats.start_time);
    
    // Инициализируем обработчики сигналов
    init_signals();
    
    // Инициализируем менеджер очередей
    if (queue_manager_init(&g_queue_manager, &g_server_cfg) != 0) {
        print_log(LOG_ERROR, "Server", "Queue manager initialization error");
        ret = 3;
        goto cleanup;
    }
    
    // Запускаем менеджер очередей
    if (queue_manager_start(&g_queue_manager) != 0) {
        print_log(LOG_ERROR, "Server", "Queue manager startup error");
        ret = 4;
        goto cleanup;
    }
    
    // Инициализируем процессор сигналов
    signal_processor_callbacks_t callbacks = {
        .device_decoded = device_decoded_callback,
        .unknown_signal = unknown_signal_callback,
        .user_data = NULL
    };
    
    if (signal_processor_init(&g_signal_processor, &g_server_cfg, &callbacks) != 0) {
        print_log(LOG_ERROR, "Server", "Signal processor initialization error");
        ret = 5;
        goto cleanup;
    }
    
    // Инициализируем транспортный слой
    server_transport_callbacks_t transport_callbacks = {
        .signal_received = signal_received_callback,
        .user_data = NULL
    };
    
    if (server_transport_init(&g_transport, &g_server_cfg, &transport_callbacks) != 0) {
        print_log(LOG_ERROR, "Server", "Transport initialization error");
        ret = 6;
        goto cleanup;
    }
    
    // Запускаем транспортный слой
    if (server_transport_start(&g_transport) != 0) {
        print_log(LOG_ERROR, "Server", "Transport startup error");
        ret = 7;
        goto cleanup;
    }
    
    print_log(LOG_INFO, "Server", "Server ready for operation");
    
    // Основной цикл сервера
    while (!exit_flag) {
        // Проверяем состояние компонентов
        if (!queue_manager_is_running(&g_queue_manager)) {
            print_log(LOG_ERROR, "Server", "Queue manager stopped");
            break;
        }
        
        if (!server_transport_is_running(&g_transport)) {
            print_log(LOG_ERROR, "Server", "Transport layer stopped");
            break;
        }
        
        // Ждем немного
        sleep(1);
        
        // Печатаем статистику каждые 60 секунд
        static time_t last_stats = 0;
        time_t now = time(NULL);
        if (g_server_cfg.stats_enabled && difftime(now, last_stats) >= g_server_cfg.stats_interval_sec) {
            if (g_server_cfg.verbosity >= LOG_INFO) {
                print_statistics();
            }
            last_stats = now;
        }
    }
    
    print_log(LOG_INFO, "Server", "Server shutting down...");
    print_statistics();

cleanup:
    // Остановка и очистка ресурсов
    server_transport_stop(&g_transport);
    server_transport_cleanup(&g_transport);
    
    signal_processor_cleanup(&g_signal_processor);
    
    queue_manager_stop(&g_queue_manager);
    queue_manager_cleanup(&g_queue_manager);
    
    server_config_free(&g_server_cfg);
    
    pthread_mutex_destroy(&g_stats.mutex);
    
    // Удаляем PID файл
    if (g_server_cfg.pid_file) {
        unlink(g_server_cfg.pid_file);
    }
    
    print_logf(LOG_INFO, "Server", "rtl_433_server finished with code %d", ret);
    return ret;
}



