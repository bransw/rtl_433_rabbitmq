# RTL_433 Общая Библиотека

Общая библиотека для rtl_433 клиента и сервера, обеспечивающая единую реализацию транспортного уровня и функций работы с сигналами.

## Преимущества

### 1. Уменьшение дублирования кода
- Единая реализация транспортных протоколов
- Общие функции работы с pulse_data
- Согласованность структур данных между клиентом и сервером

### 2. Использование оригинальных функций rtl_433
- Максимальная совместимость с оригинальным кодом
- Обертки для основных функций (pulse_slicer, rfraw_parse и т.д.)
- Прямое использование библиотек rtl_433

### 3. Легкость сопровождения
- Изменения в одном месте влияют на клиент и сервер
- Единая точка обновления для транспортных протоколов
- Упрощенное тестирование

## Компоненты

### 1. Транспортный модуль (`rtl433_transport.h/c`)

Обеспечивает единую реализацию транспортных протоколов:

```c
#include "rtl433_transport.h"

// Инициализация конфигурации
rtl433_transport_config_t config;
rtl433_transport_config_init(&config);

// Парсинг URL
rtl433_transport_parse_url("amqp://user:pass@host:port/queue", &config);

// Подключение
rtl433_transport_connection_t conn;
rtl433_transport_connect(&conn, &config);

// Отправка сообщения
rtl433_message_t *msg = rtl433_message_create_from_pulse_data(pulse_data, "FSK", package_id);
rtl433_transport_send_message(&conn, msg);
```

**Поддерживаемые протоколы:**
- RabbitMQ (AMQP)
- MQTT (планируется)
- TCP/UDP (планируется)

### 2. Модуль работы с сигналами (`rtl433_signal.h/c`)

Общие функции для обработки и анализа сигналов:

```c
#include "rtl433_signal.h"

// Реконструкция pulse_data из JSON
pulse_data_t pulse_data;
rtl433_signal_reconstruct_from_json(json_str, &pulse_data);

// Анализ качества сигнала
rtl433_signal_quality_t quality;
rtl433_signal_analyze_quality(&pulse_data, &quality);

// Декодирование устройств
rtl433_decode_result_t result;
rtl433_signal_decode(&pulse_data, r_devs, &result);

// Использование оригинальных функций rtl_433
int events = rtl433_signal_slice_pcm(&pulse_data, r_device);
```

**Основные функции:**
- Реконструкция сигналов из различных форматов
- Анализ качества и типа модуляции
- Обертки для оригинальных функций rtl_433
- Валидация и нормализация данных

## Интеграция с клиентом и сервером

### В клиенте:
```c
#include "rtl433_transport.h"
#include "rtl433_signal.h"

// Замена собственной реализации транспорта
rtl433_transport_connection_t transport;
rtl433_transport_connect(&transport, &config);

// Отправка сигналов
rtl433_message_t *msg = rtl433_message_create_from_pulse_data(pulse_data, mod, id);
rtl433_transport_send_message(&transport, msg);
```

### В сервере:
```c
#include "rtl433_transport.h"
#include "rtl433_signal.h"

// Замена собственной реализации приема
rtl433_transport_receive_messages(&transport, message_handler, user_data, timeout);

// В обработчике сообщений:
void message_handler(rtl433_message_t *message, void *user_data) {
    // Декодирование с использованием общих функций
    rtl433_decode_result_t result;
    rtl433_signal_decode(message->pulse_data, r_devs, &result);
}
```

## Сборка

### Требования:
- CMake 3.10+
- json-c
- librabbitmq (для RabbitMQ)

### Сборка:
```bash
mkdir build && cd build
cmake -DBUILD_SHARED=ON ..
make rtl433_shared
```

### Использование в других проектах:
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTL433_SHARED REQUIRED rtl433_shared)

target_include_directories(myproject PRIVATE ${RTL433_SHARED_INCLUDE_DIRS})
target_link_libraries(myproject ${RTL433_SHARED_LIBRARIES})
```

## Примеры

### Простой пример транспорта:
```bash
# Компиляция примера
gcc -o transport_example shared/examples/simple_transport_example.c -lrtl433_shared -ljson-c -lrabbitmq

# Отправка тестовых сообщений
./transport_example send amqp://guest:guest@localhost:5672/test_queue

# Прием сообщений
./transport_example receive amqp://guest:guest@localhost:5672/test_queue
```

## Преимущества перед дублированием кода

### До (дублирование):
```
client/src/client_transport.c    - 800 строк
server/src/server_transport.c    - 600 строк
client/src/signal_processing.c   - 400 строк  
server/src/signal_processing.c   - 350 строк
===================================
Итого: 2150 строк дублированного кода
```

### После (общая библиотека):
```
shared/src/rtl433_transport.c    - 500 строк
shared/src/rtl433_signal.c       - 450 строк
client/src/ (использует общую)    - 200 строк
server/src/ (использует общую)    - 150 строк
===================================
Итого: 1300 строк (экономия 40%)
```

## Дальнейшее развитие

1. **Добавление новых транспортов** - MQTT, TCP, UDP
2. **Расширение функций сигналов** - больше оберток для rtl_433
3. **Оптимизация производительности** - кеширование, пулы объектов
4. **Улучшение совместимости** - поддержка больше форматов данных

## API Reference

Подробная документация API доступна в заголовочных файлах:
- `shared/include/rtl433_transport.h` - транспортный API
- `shared/include/rtl433_signal.h` - API работы с сигналами

