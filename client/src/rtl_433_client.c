/** @file
    rtl_433_client - client for signal demodulation and server transmission.
    
    Copyright (C) 2024
    
    Based on rtl_433 by Benjamin Larsson and others.
    
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

#include "client_config.h"
#include "client_transport.h"
#include "pulse_data.h"
#include "pulse_detect.h"
#include "pulse_detect_fsk.h"
#include "sdr.h"
#include "baseband.h"
#include "logger.h"
#include "r_util.h"
#include "fileformat.h"

/// Global client configuration
static client_config_t g_client_cfg;

/// Global exit flag
static volatile sig_atomic_t exit_flag = 0;

/// Transport connection
static transport_connection_t g_transport_conn;

/// Statistics counters
static struct {
    uint64_t total_samples;
    uint64_t ook_signals_detected;
    uint64_t fsk_signals_detected;
    uint64_t signals_sent;
    uint64_t send_errors;
    time_t start_time;
} g_stats = {0};

/// Signal handler
static void signal_handler(int sig)
{
    print_logf(LOG_INFO, "Client", "Received signal %d, shutting down...", sig);
    exit_flag = 1;
}

/// Initialize signals
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
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#endif
}

/// Print statistics
static void print_statistics(void)
{
    time_t now;
    time(&now);
    double uptime = difftime(now, g_stats.start_time);
    
    print_logf(LOG_INFO, "Client", "=== Operation Statistics ===");
    print_logf(LOG_INFO, "Client", "Uptime: %.0f sec", uptime);
    print_logf(LOG_INFO, "Client", "Samples processed: %lu", g_stats.total_samples);
    print_logf(LOG_INFO, "Client", "OOK signals detected: %lu", g_stats.ook_signals_detected);
    print_logf(LOG_INFO, "Client", "FSK signals detected: %lu", g_stats.fsk_signals_detected);
    print_logf(LOG_INFO, "Client", "Signals sent: %lu", g_stats.signals_sent);
    print_logf(LOG_INFO, "Client", "Send errors: %lu", g_stats.send_errors);
    
    if (uptime > 0) {
        print_logf(LOG_INFO, "Client", "Rate: %.1f signals/min", 
                  (g_stats.ook_signals_detected + g_stats.fsk_signals_detected) * 60.0 / uptime);
    }
}

/// Обработка демодулированного OOK сигнала
static void process_ook_signal(const pulse_data_t *pulse_data)
{
    if (!pulse_data || pulse_data->num_pulses == 0) {
        return;
    }
    
    g_stats.ook_signals_detected++;
    
    // Создаем структуру для отправки
    demod_data_t *data = demod_data_create();
    if (!data) {
        print_log(LOG_ERROR, "Client", "Failed to create demod_data");
        return;
    }
    
    // Заполняем метаданные
    data->device_id = strdup("rtl_433_client");
    data->timestamp_us = time(NULL) * 1000000ULL; // Примерная временная метка
    data->frequency = g_client_cfg.frequency;
    data->sample_rate = g_client_cfg.sample_rate;
    data->rssi_db = pulse_data->rssi_db;
    data->snr_db = pulse_data->snr_db;
    data->noise_db = pulse_data->noise_db;
    data->ook_detected = 1;
    data->fsk_detected = 0;
    
    // Копируем данные о пульсах
    demod_data_set_pulse_data(data, pulse_data);
    
    // Отправляем данные
    if (transport_send_demod_data(&g_transport_conn, data) == 0) {
        g_stats.signals_sent++;
        if (g_client_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Client", "Sent OOK signal: %u pulses", pulse_data->num_pulses);
        }
    } else {
        g_stats.send_errors++;
        print_log(LOG_WARNING, "Client", "Failed to send OOK signal");
    }
    
    demod_data_free(data);
}

/// Обработка демодулированного FSK сигнала
static void process_fsk_signal(const pulse_data_t *pulse_data)
{
    if (!pulse_data || pulse_data->num_pulses == 0) {
        return;
    }
    
    g_stats.fsk_signals_detected++;
    
    // Создаем структуру для отправки
    demod_data_t *data = demod_data_create();
    if (!data) {
        print_log(LOG_ERROR, "Client", "Failed to create demod_data");
        return;
    }
    
    // Заполняем метаданные
    data->device_id = strdup("rtl_433_client");
    data->timestamp_us = time(NULL) * 1000000ULL;
    data->frequency = g_client_cfg.frequency;
    data->sample_rate = g_client_cfg.sample_rate;
    data->rssi_db = pulse_data->rssi_db;
    data->snr_db = pulse_data->snr_db;
    data->noise_db = pulse_data->noise_db;
    data->ook_detected = 0;
    data->fsk_detected = 1;
    
    // Копируем данные о пульсах
    demod_data_set_pulse_data(data, pulse_data);
    
    // Отправляем данные
    if (transport_send_demod_data(&g_transport_conn, data) == 0) {
        g_stats.signals_sent++;
        if (g_client_cfg.verbosity >= LOG_DEBUG) {
            print_logf(LOG_DEBUG, "Client", "Sent FSK signal: %u pulses", pulse_data->num_pulses);
        }
    } else {
        g_stats.send_errors++;
        print_log(LOG_WARNING, "Client", "Failed to send FSK signal");
    }
    
    demod_data_free(data);
}

/// Обработка сэмплов от SDR
static void sdr_callback(unsigned char *iq_buf, uint32_t len, void *ctx)
{
    client_config_t *cfg = (client_config_t *)ctx;
    
    if (exit_flag) {
        return;
    }
    
    g_stats.total_samples += len / 2; // IQ пары
    
    // Здесь должна быть логика демодуляции
    // Для упрощения используем заглушки
    
    // TODO: Интегрировать полную логику демодуляции из rtl_433
    // Включая:
    // - AM демодуляцию
    // - Обнаружение пульсов OOK
    // - Обнаружение FSK сигналов
    // - Анализ pulse_data
    
    if (cfg->verbosity >= LOG_TRACE) {
        print_logf(LOG_TRACE, "Client", "Processed %u bytes of data", len);
    }
}

/// Главная функция
int main(int argc, char **argv)
{
    int ret = 0;
    
    print_log(LOG_INFO, "Client", "rtl_433_client started");
    
    // Инициализируем конфигурацию
    client_config_init(&g_client_cfg);
    
    // Парсим аргументы командной строки
    if (client_config_parse_args(&g_client_cfg, argc, argv) != 0) {
        print_log(LOG_ERROR, "Client", "Command line argument error");
        ret = 1;
        goto cleanup;
    }
    
    // Валидируем конфигурацию
    if (client_config_validate(&g_client_cfg) != 0) {
        print_log(LOG_ERROR, "Client", "Configuration error");
        ret = 1;
        goto cleanup;
    }
    
    // Устанавливаем уровень логирования
    set_log_level(g_client_cfg.verbosity);
    
    // Инициализируем статистику
    time(&g_stats.start_time);
    
    // Инициализируем обработчики сигналов
    init_signals();
    
    // Инициализируем транспорт
    if (transport_init(&g_transport_conn, &g_client_cfg.transport) != 0) {
        print_log(LOG_ERROR, "Client", "Transport initialization error");
        ret = 2;
        goto cleanup;
    }
    
    // Подключаемся к серверу
    if (transport_connect(&g_transport_conn) != 0) {
        print_log(LOG_ERROR, "Client", "Server connection error");
        ret = 3;
        goto cleanup;
    }
    
    print_logf(LOG_INFO, "Client", "Connected to server: %s:%d", 
              g_client_cfg.transport.host, g_client_cfg.transport.port);
    
    // TODO: Инициализация SDR устройства
    // Здесь должна быть логика из sdr.c для инициализации RTL-SDR
    
    print_log(LOG_INFO, "Client", "Starting signal demodulation...");
    
    // Основной цикл обработки
    time_t start_time = time(NULL);
    while (!exit_flag) {
        // Проверяем ограничение времени работы
        if (g_client_cfg.duration > 0) {
            time_t now = time(NULL);
            if (difftime(now, start_time) >= g_client_cfg.duration) {
                print_log(LOG_INFO, "Client", "Runtime limit reached");
                break;
            }
        }
        
        // Проверяем состояние соединения
        if (!transport_is_connected(&g_transport_conn)) {
            print_log(LOG_WARNING, "Client", "Lost connection to server, reconnecting...");
            if (transport_connect(&g_transport_conn) != 0) {
                print_log(LOG_ERROR, "Client", "Failed to reconnect");
                // Подождем перед следующей попыткой
                sleep(5);
                continue;
            }
        }
        
        // TODO: Здесь должна быть интеграция с SDR
        // Пока используем простую задержку
        usleep(100000); // 100ms
        
        // Печатаем статистику каждые 60 секунд
        static time_t last_stats = 0;
        time_t now = time(NULL);
        if (difftime(now, last_stats) >= 60) {
            if (g_client_cfg.verbosity >= LOG_INFO) {
                print_statistics();
            }
            last_stats = now;
        }
    }
    
    print_log(LOG_INFO, "Client", "Shutting down...");
    print_statistics();

cleanup:
    // Очистка ресурсов
    transport_cleanup(&g_transport_conn);
    client_config_free(&g_client_cfg);
    
    print_logf(LOG_INFO, "Client", "rtl_433_client finished with code %d", ret);
    return ret;
}
