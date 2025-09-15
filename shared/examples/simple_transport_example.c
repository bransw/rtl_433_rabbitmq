/** @file
 * Пример использования общей библиотеки rtl_433 для транспорта
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rtl433_transport.h"
#include "rtl433_signal.h"

void example_message_handler(rtl433_message_t *message, void *user_data)
{
    printf("Message received:\n");
    printf("  Package ID: %lu\n", message->package_id);
    printf("  Modulation: %s\n", message->modulation);
    printf("  Pulses: %u\n", message->pulse_data->num_pulses);
    printf("  Frequency: %.1f Hz\n", message->pulse_data->centerfreq_hz);
    printf("  RSSI: %.1f dB\n", message->pulse_data->rssi_db);
    printf("\n");
    
    // Попытка декодирования (требует r_devs)
    rtl433_signal_quality_t quality;
    if (rtl433_signal_analyze_quality(message->pulse_data, &quality) == 0) {
        printf("Signal quality:\n");
        printf("  SNR: %.1f dB\n", quality.snr_db);
        printf("  Average pulse width: %.1f μs\n", quality.avg_pulse_width_us);
        printf("  Valid: %s\n", quality.is_valid ? "Yes" : "No");
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s [send|receive] [url]\n", argv[0]);
        printf("Example:\n");
        printf("  %s send amqp://guest:guest@localhost:5672/test_queue\n", argv[0]);
        printf("  %s receive amqp://guest:guest@localhost:5672/test_queue\n", argv[0]);
        return 1;
    }
    
    const char *mode = argv[1];
    const char *url = argc > 2 ? argv[2] : "amqp://guest:guest@localhost:5672/test_queue";
    
    // Инициализация конфигурации
    rtl433_transport_config_t config;
    if (rtl433_transport_config_init(&config) != 0) {
        fprintf(stderr, "Configuration initialization error\n");
        return 1;
    }
    
    // Парсинг URL
    if (rtl433_transport_parse_url(url, &config) != 0) {
        fprintf(stderr, "URL parsing error: %s\n", url);
        rtl433_transport_config_free(&config);
        return 1;
    }
    
    printf("Connecting to %s (%s:%d)\n", 
           rtl433_transport_type_to_string(config.type),
           config.host, config.port);
    
    // Создание соединения
    rtl433_transport_connection_t conn;
    if (rtl433_transport_connect(&conn, &config) != 0) {
        fprintf(stderr, "Connection error\n");
        rtl433_transport_config_free(&config);
        return 1;
    }
    
    printf("Connection established\n");
    
    if (strcmp(mode, "send") == 0) {
        // Отправка тестовых сообщений
        printf("Sending test messages...\n");
        
        for (int i = 0; i < 5; i++) {
            // Создание тестового pulse_data
            pulse_data_t test_pulse_data;
            memset(&test_pulse_data, 0, sizeof(test_pulse_data));
            rtl433_signal_set_defaults(&test_pulse_data, 250000);
            
            // Заполнение тестовыми данными
            test_pulse_data.num_pulses = 10;
            test_pulse_data.centerfreq_hz = 433920000.0f;
            test_pulse_data.rssi_db = -20.0f + i;
            test_pulse_data.snr_db = 15.0f;
            test_pulse_data.noise_db = -35.0f;
            
            for (unsigned j = 0; j < test_pulse_data.num_pulses; j++) {
                test_pulse_data.pulse[j] = 50 + (j % 3) * 25; // Вариация пульсов
                test_pulse_data.gap[j] = 75 + (j % 2) * 25;   // Вариация пауз
            }
            
            // Создание сообщения
            rtl433_message_t *message = rtl433_message_create_from_pulse_data(
                &test_pulse_data, 
                "FSK", 
                rtl433_generate_package_id()
            );
            
            if (message) {
                // Отправка
                if (rtl433_transport_send_message(&conn, message) == 0) {
                    printf("Message %d sent (ID: %lu)\n", i + 1, message->package_id);
                } else {
                    printf("Error sending message %d\n", i + 1);
                }
                
                rtl433_message_free(message);
            }
            
            sleep(1); // Пауза между отправками
        }
        
    } else if (strcmp(mode, "receive") == 0) {
        // Прием сообщений
        printf("Waiting for messages... (Ctrl+C to exit)\n");
        
        while (1) {
            int result = rtl433_transport_receive_messages(&conn, example_message_handler, NULL, 1000);
            if (result < 0) {
                printf("Message reception error\n");
                break;
            }
            if (result == 0) {
                printf("No new messages...\n");
            }
        }
        
    } else {
        printf("Unknown mode: %s\n", mode);
    }
    
    // Получение статистики
    rtl433_transport_stats_t stats;
    rtl433_transport_get_stats(&conn, &stats);
    printf("\nStatistics:\n");
    printf("  Sent: %lu\n", stats.messages_sent);
    printf("  Received: %lu\n", stats.messages_received);
    printf("  Send errors: %lu\n", stats.send_errors);
    printf("  Receive errors: %lu\n", stats.receive_errors);
    
    // Очистка
    rtl433_transport_disconnect(&conn);
    rtl433_transport_config_free(&config);
    
    printf("Completed\n");
    return 0;
}
