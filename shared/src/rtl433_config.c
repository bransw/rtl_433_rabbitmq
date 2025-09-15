/** @file
 * Реализация общей конфигурации для rtl_433
 */

#include "rtl433_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// === ФУНКЦИИ КОНФИГУРАЦИИ ===

int rtl433_config_init(rtl433_config_t *config, rtl433_app_type_t app_type)
{
    if (!config) return -1;
    
    memset(config, 0, sizeof(rtl433_config_t));
    
    config->app_type = app_type;
    config->verbosity = RTL433_DEFAULT_VERBOSITY;
    
    // Инициализация транспорта
    if (rtl433_transport_config_init(&config->transport) != 0) {
        return -1;
    }
    
    // Дефолты в зависимости от типа
    if (app_type == RTL433_APP_CLIENT) {
        return rtl433_config_client_defaults(config);
    } else {
        return rtl433_config_server_defaults(config);
    }
}

void rtl433_config_free(rtl433_config_t *config)
{
    if (!config) return;
    
    free(config->config_file);
    free(config->pid_file);
    
    // SDR
    free(config->sdr.device_selection);
    free(config->sdr.input_file);
    free(config->sdr.hop_times);
    free(config->sdr.frequencies);
    
    // Server
    free(config->server.database_path);
    
    // Signal
    if (config->signal.protocol_names) {
        for (int i = 0; i < config->signal.protocol_count; i++) {
            free(config->signal.protocol_names[i]);
        }
        free(config->signal.protocol_names);
    }
    
    // Output
    if (config->output.output_formats) {
        for (int i = 0; i < config->output.output_count; i++) {
            free(config->output.output_formats[i]);
        }
        free(config->output.output_formats);
    }
    free(config->output.output_file);
    
    // Debug
    free(config->debug.signal_save_path);
    
    // Transport
    rtl433_transport_config_free(&config->transport);
    
    memset(config, 0, sizeof(rtl433_config_t));
}

int rtl433_config_load_file(rtl433_config_t *config, const char *filename)
{
    // TODO: Реализовать загрузку из файла
    (void)config;
    (void)filename;
    return 0;
}

int rtl433_config_save_file(const rtl433_config_t *config, const char *filename)
{
    // TODO: Реализовать сохранение в файл
    (void)config;
    (void)filename;
    return 0;
}

int rtl433_config_parse_args(rtl433_config_t *config, int argc, char **argv)
{
    // TODO: Реализовать парсинг аргументов
    (void)config;
    (void)argc;
    (void)argv;
    return 0;
}

int rtl433_config_validate(const rtl433_config_t *config)
{
    if (!config) return -1;
    
    // Базовая валидация
    if (config->sdr.sample_rate == 0) return -1;
    if (config->sdr.frequency == 0) return -1;
    
    return 0;
}

int rtl433_config_apply_transport(rtl433_config_t *config)
{
    if (!config) return -1;
    
    // Применить настройки транспорта
    return 0;
}

// === УТИЛИТЫ КОНФИГУРАЦИИ ===

int rtl433_config_set_transport_url(rtl433_config_t *config, const char *url)
{
    if (!config || !url) return -1;
    
    return rtl433_transport_parse_url(url, &config->transport);
}

int rtl433_config_add_protocol(rtl433_config_t *config, const char *protocol_name)
{
    if (!config || !protocol_name) return -1;
    
    // Расширить массив протоколов
    char **new_names = realloc(config->signal.protocol_names, 
                              (config->signal.protocol_count + 1) * sizeof(char*));
    if (!new_names) return -1;
    
    config->signal.protocol_names = new_names;
    config->signal.protocol_names[config->signal.protocol_count] = strdup(protocol_name);
    config->signal.protocol_count++;
    
    return 0;
}

int rtl433_config_add_output_format(rtl433_config_t *config, const char *format)
{
    if (!config || !format) return -1;
    
    // Расширить массив форматов
    char **new_formats = realloc(config->output.output_formats,
                                (config->output.output_count + 1) * sizeof(char*));
    if (!new_formats) return -1;
    
    config->output.output_formats = new_formats;
    config->output.output_formats[config->output.output_count] = strdup(format);
    config->output.output_count++;
    
    return 0;
}

void rtl433_config_set_verbosity(rtl433_config_t *config, int level)
{
    if (config) {
        config->verbosity = level;
    }
}

const char* rtl433_config_app_type_string(rtl433_app_type_t type)
{
    switch (type) {
        case RTL433_APP_CLIENT: return "Client";
        case RTL433_APP_SERVER: return "Server";
        default: return "Unknown";
    }
}

void rtl433_config_print(const rtl433_config_t *config)
{
    if (!config) return;
    
    printf("RTL433 Configuration:\n");
    printf("  App Type: %s\n", rtl433_config_app_type_string(config->app_type));
    printf("  Verbosity: %d\n", config->verbosity);
    printf("  Transport: %s://%s:%d/%s\n",
           rtl433_transport_type_to_string(config->transport.type),
           config->transport.host ? config->transport.host : "localhost",
           config->transport.port,
           config->transport.queue ? config->transport.queue : "rtl_433");
    
    if (config->app_type == RTL433_APP_CLIENT) {
        printf("  SDR Frequency: %u Hz\n", config->sdr.frequency);
        printf("  SDR Sample Rate: %u Hz\n", config->sdr.sample_rate);
        printf("  SDR Gain: %d\n", config->sdr.gain);
    } else {
        printf("  Server Stats Interval: %d sec\n", config->server.stats_interval);
        printf("  Server Max Connections: %d\n", config->server.max_connections);
    }
}

void rtl433_config_print_help(rtl433_app_type_t app_type)
{
    printf("RTL433 %s Help:\n", rtl433_config_app_type_string(app_type));
    printf("  -v              Increase verbosity\n");
    printf("  -T <url>        Transport URL (amqp://user:pass@host:port/queue)\n");
    printf("  -c <file>       Configuration file\n");
    printf("  -h              Show this help\n");
    
    if (app_type == RTL433_APP_CLIENT) {
        printf("  -f <freq>       Set frequency in Hz\n");
        printf("  -s <rate>       Set sample rate in Hz\n");
        printf("  -g <gain>       Set gain\n");
        printf("  -r <file>       Read from file instead of SDR\n");
    } else {
        printf("  -S <interval>   Stats interval in seconds\n");
        printf("  -d <path>       Database path\n");
    }
}

void rtl433_config_print_version(void)
{
    printf("RTL433 Shared Library v1.0.0\n");
}

// === ПРЕДОПРЕДЕЛЕННЫЕ КОНФИГУРАЦИИ ===

int rtl433_config_client_defaults(rtl433_config_t *config)
{
    if (!config) return -1;
    
    // SDR дефолты
    config->sdr.sample_rate = RTL433_DEFAULT_SAMPLE_RATE;
    config->sdr.frequency = RTL433_DEFAULT_FREQUENCY;
    config->sdr.gain = RTL433_DEFAULT_GAIN;
    config->sdr.auto_gain = true;
    config->sdr.ppm_error = RTL433_DEFAULT_PPM_ERROR;
    config->sdr.hop_time = RTL433_DEFAULT_HOP_TIME;
    config->sdr.test_mode = false;
    
    // Signal processing
    config->signal.analyze_pulses = true;
    config->signal.enable_ook = true;
    config->signal.enable_fsk = true;
    config->signal.pulse_threshold = 1000;
    
    // Output
    config->output.live_output = true;
    config->output.timestamp = true;
    rtl433_config_add_output_format(config, "json");
    
    return 0;
}

int rtl433_config_server_defaults(rtl433_config_t *config)
{
    if (!config) return -1;
    
    // Server дефолты
    config->server.max_connections = 100;
    config->server.stats_interval = RTL433_DEFAULT_STATS_INTERVAL;
    config->server.store_unknown = false;
    config->server.queue_size = RTL433_DEFAULT_QUEUE_SIZE;
    config->server.worker_threads = RTL433_DEFAULT_WORKER_THREADS;
    
    // Signal processing
    config->signal.analyze_pulses = true;
    config->signal.enable_ook = true;
    config->signal.enable_fsk = true;
    
    // Output
    config->output.live_output = true;
    config->output.timestamp = true;
    rtl433_config_add_output_format(config, "json");
    
    return 0;
}

int rtl433_config_test_defaults(rtl433_config_t *config)
{
    if (!config) return -1;
    
    // Тестовые дефолты
    config->verbosity = 3; // DEBUG
    config->debug.debug_demod = true;
    config->debug.debug_device = true;
    config->debug.save_signals = true;
    
    if (config->app_type == RTL433_APP_CLIENT) {
        config->sdr.test_mode = true;
        return rtl433_config_client_defaults(config);
    } else {
        return rtl433_config_server_defaults(config);
    }
}

