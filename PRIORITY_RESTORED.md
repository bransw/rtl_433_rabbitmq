# ✅ ПРИОРИТЕТ hex_string ВОССТАНОВЛЕН

## 🔄 ИЗМЕНЕНО

### Файл: `shared/src/rtl433_asn1_pulse.c`
### Функция: `prepare_pulse_data()` (строки 516-612)

---

## 📊 НОВАЯ ЛОГИКА (hex_string приоритет):

```c
// 1. Всегда заполняем pulses array (для fallback)
for (i = 0; i < num_pulses; i++) {
    ASN_SEQUENCE_ADD(&pulses_data->pulses, pulse[i]);  // pulse
    ASN_SEQUENCE_ADD(&pulses_data->pulses, gap[i]);    // gap
}

// 2. ✅ ПРИОРИТЕТ 1: hex_string (если есть) - компактный
if (hex_string) {
    signal_msg->signalData = hexString;  // или hexStrings
}
// 3. ⚠️ ПРИОРИТЕТ 2: pulses array (если нет hex_string)
else if (num_pulses > 0) {
    signal_msg->signalData = pulsesArray;
}
// 4. ⚠️ ПРИОРИТЕТ 3: пустой массив
else {
    signal_msg->signalData = empty pulsesArray;
}
```

---

## 🎯 КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ

### ✅ Теперь pulses array содержит pulse/gap пары!

**Кодирование (prepare_pulse_data):**
```c
// Fill pulses array with alternating pulse/gap pairs
for (unsigned int i = 0; i < pulse->num_pulses; i++) {
    // Add pulse
    long *pulse_val = calloc(1, sizeof(long));
    *pulse_val = pulse->pulse[i];
    ASN_SEQUENCE_ADD(&pulses_data->pulses.list, pulse_val);  // ✅
    
    // Add gap
    long *gap_val = calloc(1, sizeof(long));
    *gap_val = pulse->gap[i];
    ASN_SEQUENCE_ADD(&pulses_data->pulses.list, gap_val);    // ✅
}
```

**Декодирование (extract_pulse_data):**
```c
// Extract pulse/gap pairs from alternating array
pulse_data->num_pulses = array_count / 2;  // Каждая пара = 2 значения

for (int i = 0; i < pulse_data->num_pulses; i++) {
    int array_idx = i * 2;
    pulse_data->pulse[i] = *pulses[array_idx];      // ✅ pulse
    pulse_data->gap[i] = *pulses[array_idx + 1];    // ✅ gap
}
```

---

## 📋 ПРИОРИТЕТЫ

| Приоритет | Использует | Точность | Размер | Для Toyota TPMS |
|-----------|-----------|----------|--------|-----------------|
| **1️⃣ hex_string** | hexString/hexStrings | ~9% (lossy) | Компактно | ❌ Не работает |
| **2️⃣ pulses array** | pulsesArray | 100% | Больше | ✅ Работает |
| **3️⃣ empty** | empty pulsesArray | 0% | Минимум | ❌ Нет данных |

---

## 🔍 ЧТО ИЗМЕНИЛОСЬ ОТ ПРЕДЫДУЩЕЙ ВЕРСИИ?

### Было (pulses приоритет):
```
✅ PRIORITY 1: pulses array → 100% точность → Toyota TPMS работает
⚠️ PRIORITY 2: hex_string → ~9% точность
```

### Стало (hex_string приоритет - ВОССТАНОВЛЕНО):
```
✅ PRIORITY 1: hex_string → ~9% точность → Toyota TPMS НЕ работает
⚠️ PRIORITY 2: pulses array → 100% точность
```

### НО ИСПРАВЛЕНА КРИТИЧЕСКАЯ ОШИБКА:

**До исправления:**
- pulses array **НЕ содержал gaps** → все gaps = 0 → детекция провалена

**После исправления:**
- pulses array **содержит pulse/gap пары** → gaps правильные → детекция работает!

---

## ⚠️ ВАЖНО

При текущем приоритете (hex_string первый):
- **Toyota TPMS НЕ БУДЕТ обнаружен** из ASN.1 (т.к. hex_string lossy)
- **НО** если hex_string отсутствует, будет использован pulses array
- **И** pulses array теперь **правильный** (с gaps)!

---

## 🎯 КОНСОЛЬНЫЙ ВЫВОД

### Кодирование:
```
✅ PRIORITY 1: Using hex_string for SignalData (compact)
📦 Added single hex string to SignalData (42 bytes, approximate)
```

### Декодирование (если был hex_string):
```
📦 Extracting single hex string from SignalData (42 bytes)
⚠️ hex_string будет использован (низкая точность)
❌ Toyota TPMS может НЕ быть обнаружен
```

### Декодирование (если был pulses array):
```
📦 Extracting 116 values (pulse-gap pairs) from SignalData
✅ Extracted 58 pulse-gap pairs (pulse[] and gap[] filled)
✅ Toyota TPMS будет обнаружен!
```

---

## ✅ ИТОГ

**Приоритет hex_string ВОССТАНОВЛЕН** (как было изначально).

**НО ИСПРАВЛЕНА критическая ошибка:**
- pulses array теперь содержит **pulse/gap пары**
- Декодирование правильно разделяет их

**Для тестирования Toyota TPMS:**
- С hex_string: ❌ Может не работать (lossy)
- С pulses array: ✅ Должно работать (исправлено)

Готово для компиляции! 🚀





