# 🧪 ASN.1 Testing Guide

Руководство по тестированию ASN.1 декодирования и обнаружения устройств в RTL433 Split Architecture.

## 📋 Обзор

Система ASN.1 в RTL433 поддерживает:
- ✅ **Валидацию hex_string** перед кодированием
- ✅ **Автоматический fallback** на pulses при невалидных hex_string
- ✅ **Константы из ASN.1 спецификации** (до 512 байт, 32 hex_strings)
- ✅ **Полный цикл** encode → decode → device detection

## 🔧 Доступные инструменты тестирования

### 1. **C Test Utility** 
```bash
./test_asn1_decoder
```
- Проверяет доступность ASN.1 функций
- Базовая проверка компиляции и линковки

### 2. **Python Test Suite**
```bash
python3 test_asn1_device_detection.py
```
- **Комплексное тестирование** ASN.1 encode → decode → device detection
- **Три метода сравнения**: Direct JSON, ASN.1 roundtrip, RabbitMQ input  
- **Готовые тестовые случаи**: Toyota TPMS, OOK signals

## 📊 Тестовые сценарии

### 🧪 **Test Case 1: Toyota TPMS Signal**
```json
{
  "mod": "FSK",
  "freq1_Hz": 433939136,
  "freq2_Hz": 433871584,
  "hex_string": "AAB00B0401002C00AC003800608155+AAB00C0401002C00AC00380060828355+AAB00B0401002C00AC003800609355"
}
```
- **Цель**: Тестирование множественных hex_strings
- **Ожидается**: Успешное кодирование/декодирование

### 🧪 **Test Case 2: OOK Device Signal**
```json
{
  "mod": "OOK",
  "freq1_Hz": 433939232,
  "hex_string": "AAB011060100DC00900034005C2710FFFF81A3C555"
}
```
- **Цель**: Тестирование одиночного hex_string
- **Ожидается**: Валидация и обработка

## 🔍 Валидация hex_string

### ✅ **Валидные форматы**
```
AAB011060100DC00900034005C2710FFFF81A3C555           # Одиночный
AAB011060100...+AAB011060100...+AAB011060100...      # Множественный
```

### ❌ **Невалидные форматы** (автоматический fallback на pulses)
```
XXX011060100DC00900034005C2710FFFF81A3C555           # Неправильный заголовок
AAB010FF9745E8C8155                                 # > 16 timing bins (старый лимит)
AAB011060100DC00900034005C2710...+[>32 hex_strings] # Слишком много
```

## 📈 Лимиты и константы

### 📏 **ASN.1 Константы** (из `asn_constant.h`)
```c
#define hexStringMaxSize (512)      // Макс. размер одного hex_string
#define hexStringsMaxCount (32)     // Макс. количество hex_strings  
#define hexStringMinSize (1)        // Мин. размер hex_strings
#define pulsesMaxCount (65535)      // Макс. количество импульсов
```

### 🔧 **Валидатор rtl433_rfraw** 
- **Timing bins**: до 16 (увеличено с 8)
- **Формат**: `AA + BB + CC + DD + [hex_data]`
- **Заголовок**: `0xAA & 0xF0 == 0xA0`

## 🚀 Пример использования

### 1. **Прямое тестирование**
```bash
# Отправка в ASN.1 очередь
echo '{"hex_string":"AAB011060100DC00900034005C2710FFFF81A3C555","mod":"OOK"}' | \
    ./build/rtl_433_client -F asn1://localhost/signal_queue -r -

# Чтение из ASN.1 очереди  
./build/rtl_433_client -F json -r asn1://localhost/signal_queue
```

### 2. **Отладочный режим**
```bash
# Включить отладку валидации
./build/rtl_433_client -F asn1://localhost/signal_queue -r test_data.json -v 2>&1 | grep -E "🔍|✅|🔴"
```

## 📋 Checklist для тестирования

### ✅ **Базовые проверки**
- [ ] Компиляция без ошибок
- [ ] ASN.1 константы генерируются корректно  
- [ ] Функции кодирования/декодирования доступны

### ✅ **Валидация hex_string**
- [ ] Валидные hex_strings принимаются
- [ ] Невалидные hex_strings отклоняются
- [ ] Fallback на pulses работает
- [ ] Лимиты размеров соблюдаются

### ✅ **Полный цикл**
- [ ] JSON → ASN.1 кодирование работает
- [ ] ASN.1 → pulse_data декодирование работает  
- [ ] Обнаружение устройств из декодированных данных
- [ ] RabbitMQ транспорт функционирует

## 🔧 Устранение неполадок

### ❌ **"No devices detected"**
- Нормально для тестовых данных
- Реальные устройства требуют правильных декодеров

### ❌ **"ASN1: Failed to encode ASN.1 message: -4"**  
- Проверьте размер hex_string (должен быть ≤ 512 байт)
- Убедитесь, что hex_strings_count ≤ 32

### ❌ **"RabbitMQ may not be available"**
- Запустите RabbitMQ: `docker run -d --name rabbitmq -p 5672:5672 rabbitmq:3-management`
- Проверьте подключение: `telnet localhost 5672`

## 🎯 Следующие шаги

1. **Расширение тестов**: Добавьте реальные устройства с известными декодерами
2. **Автоматизация**: Интегрируйте в CI/CD pipeline  
3. **Производительность**: Замерьте скорость encode/decode
4. **Мониторинг**: Добавьте метрики для RabbitMQ throughput

---

💡 **Для дополнительной информации см.**:
- `shared/asn1/rtl433-rabbitmq.asn1` - ASN.1 спецификация
- `shared/src/rtl433_asn1.c` - Функции encode/decode  
- `shared/src/output_asn1.c` - Валидация и обработка




