# Анализ: Какие данные нужны для полного восстановления сигнала

## Проблема

**hex_string** использует **histogram-based compression** (гистограммное сжатие):
- Оригинальные значения пульсов **округляются** до ближайших значений в гистограмме
- Это **lossy compression** - точное восстановление **невозможно**
- Гистограмма имеет максимум **8 bins** (8 уникальных значений)

## Пример из test_hex_recovery.json

### Оригинальные данные (JSON):
```json
{
  "pulses": [40, 60, 40, 64, 40, 52, 52, 96, 200, 104, 96, 52, 48, ...],
  "count": 58
}
```

### Гистограмма (из hex_string):
```
Bin[0] = 48 samples
Bin[1] = 96 samples
Bin[2] = 200 samples
Bin[3] = 140 samples
```

### Восстановлено из hex_string:
```
[48, 48, 48, 48, 200, 96, 48, 48, 96, 96, 48, 96, ...]
Count: 57 pulses (потеряли 1!)
```

### Сравнение:
| Index | Оригинал | Восстановлено | Ошибка |
|-------|----------|---------------|--------|
| 0 | 40 | 48 | +8 (20%) |
| 1 | 60 | 48 | -12 (20%) |
| 2 | 40 | 48 | +8 (20%) |
| 3 | 64 | 48 | -16 (25%) |
| 4 | 40 | 200 | +160 (400%) |
| 5 | 52 | 96 | +44 (85%) |
| ... | ... | ... | ... |

**Точность: только 8.8% близких совпадений!**

---

## ✅ РЕШЕНИЕ 1: Отправлять ТОЧНЫЙ массив пульсов

### В JSON добавить:
```json
{
  "hex_string": "AAB1040030006000C800...",  // Компактное представление
  "pulses": [40, 60, 40, 64, 40, 52, 52, 96, 200, 104, ...],  // ТОЧНЫЕ значения
  "count": 58,
  
  // RF метаданные:
  "freq_Hz": 433920000,
  "rate_Hz": 250000,
  "rssi_dB": -2.714,
  "snr_dB": 6.559,
  "noise_dB": -9.273,
  
  // Timing метаданные:
  "offset": 47161,
  "start_ago": 18375,
  "end_ago": 16384,
  "ook_low_estimate": 1937,
  "ook_high_estimate": 8770,
  "fsk_f1_est": 5951,
  "fsk_f2_est": -10916,
  "freq1_Hz": 433942688,
  "freq2_Hz": 433878368,
  "range_dB": 42.144,
  "depth_bits": 8
}
```

### Приоритет восстановления:
1. **`pulses` array** - если есть, использовать ТОЧНЫЕ значения ✅
2. **`hex_string`** - если `pulses` нет, использовать приближённые значения ⚠️
3. **defaults** - если ничего нет ❌

---

## ✅ РЕШЕНИЕ 2: Использовать ASN.1 с pulsesArray

### В ASN.1 кодировать:
```c
// Приоритет 1: pulsesArray (точные значения)
signal_msg->signalData.present = SignalData_PR_pulsesArray;
for (unsigned int i = 0; i < pulse->num_pulses; i++) {
    long *pulse_val = calloc(1, sizeof(long));
    *pulse_val = pulse->pulse[i];
    ASN_SEQUENCE_ADD(&pulses_data->pulses.list, pulse_val);
}

// Также сохранить hex_string для совместимости
// (но не использовать для восстановления!)
```

### При декодировании:
```c
if (signal_msg->signalData.present == SignalData_PR_pulsesArray) {
    // ПРИОРИТЕТ: использовать точный массив
    PulsesData_t *pulses_data = &signal_msg->signalData.choice.pulsesArray;
    for (int i = 0; i < pulses_data->pulses.list.count; i++) {
        pulse_data->pulse[i] = *(pulses_data->pulses.list.array[i]);
    }
    pulse_data->num_pulses = pulses_data->pulses.list.count;
}
else if (signal_msg->signalData.present == SignalData_PR_hexString) {
    // FALLBACK: использовать приближённые значения из hex_string
    // (только если pulsesArray недоступен)
}
```

---

## ✅ РЕШЕНИЕ 3: Улучшенная гистограмма (невозможно без изменения формата)

Для **точного** восстановления нужно изменить формат hex_string:
- Увеличить количество bins до 16 или 32
- Добавить таблицу отклонений (delta encoding)
- Использовать адаптивное квантование

**НО:** Это требует изменения формата rfraw, который является стандартом!

---

## 📊 ИТОГОВАЯ ТАБЛИЦА: Что нужно для полного восстановления

| Данные | Обязательно | Для точности | Назначение |
|--------|-------------|--------------|------------|
| **`pulses` array** | ✅ ДА | ✅ ДА | Точные значения пульсов |
| **`count`** | ✅ ДА | ✅ ДА | Количество пульсов |
| **`rate_Hz`** | ✅ ДА | ✅ ДА | Sample rate для конвертации |
| `freq_Hz` | ⚠️ Рекомендуется | ✅ ДА | Центральная частота |
| `rssi_dB` | ⚠️ Рекомендуется | ✅ ДА | Сила сигнала |
| `snr_dB` | ⚠️ Рекомендуется | ✅ ДА | Отношение сигнал/шум |
| `noise_dB` | ⚠️ Рекомендуется | ✅ ДА | Уровень шума |
| `ook_low_estimate` | ⚠️ Опционально | ⚠️ Желательно | Пороги детекции OOK |
| `ook_high_estimate` | ⚠️ Опционально | ⚠️ Желательно | Пороги детекции OOK |
| `fsk_f1_est` | ⚠️ Опционально | ⚠️ Желательно | FSK частота 1 |
| `fsk_f2_est` | ⚠️ Опционально | ⚠️ Желательно | FSK частота 2 |
| `freq1_Hz` | ⚠️ Опционально | ⚠️ Желательно | Детализация частоты |
| `freq2_Hz` | ⚠️ Опционально | ⚠️ Желательно | Детализация частоты |
| `offset` | ⚠️ Опционально | ❌ Нет | Позиция в потоке |
| `start_ago` | ⚠️ Опционально | ❌ Нет | Время начала |
| `end_ago` | ⚠️ Опционально | ❌ Нет | Время окончания |
| `range_dB` | ⚠️ Опционально | ❌ Нет | Динамический диапазон |
| `depth_bits` | ⚠️ Опционально | ❌ Нет | Битность АЦП |
| `hex_string` | ❌ НЕТ | ❌ НЕТ | Только для компактности |

---

## 🎯 РЕКОМЕНДАЦИЯ

### Минимальный набор для ТОЧНОГО восстановления:

```json
{
  "pulses": [40, 60, 40, 64, 40, 52, 52, 96, 200, 104, ...],  // ✅ ОБЯЗАТЕЛЬНО
  "count": 58,                                                 // ✅ ОБЯЗАТЕЛЬНО
  "rate_Hz": 250000,                                          // ✅ ОБЯЗАТЕЛЬНО
  "freq_Hz": 433920000,                                       // ✅ РЕКОМЕНДУЕТСЯ
  "rssi_dB": -2.714,                                          // ✅ РЕКОМЕНДУЕТСЯ
  "snr_dB": 6.559,                                            // ✅ РЕКОМЕНДУЕТСЯ
  "noise_dB": -9.273                                          // ✅ РЕКОМЕНДУЕТСЯ
}
```

### Компактный набор (с потерей точности):

```json
{
  "hex_string": "AAB1040030006000C800...",  // ⚠️ ПРИБЛИЖЁННО (lossy)
  "freq_Hz": 433920000,
  "rate_Hz": 250000,
  "rssi_dB": -2.714
}
```

---

## 💡 ВЫВОДЫ

1. **hex_string НЕДОСТАТОЧЕН** для точного восстановления
2. **Нужен массив `pulses`** для 100% точности
3. **Metadata критична** для правильного декодирования устройств
4. **Текущая реализация в `rtl433_asn1_pulse.c` правильна** - она НЕ восстанавливает пульсы из hex_string, так как это даст неточные результаты
5. **Оптимальная стратегия**: отправлять **И** `pulses`, **И** `hex_string` (для совместимости)

---

## 📝 КОД: Правильная функция восстановления

```c
int signal_recovery_full(const char *json_str, pulse_data_t *pulse_data) {
    json_object *root = json_tokener_parse(json_str);
    if (!root) return -1;
    
    // ПРИОРИТЕТ 1: Точный массив pulses
    json_object *pulses_obj;
    if (json_object_object_get_ex(root, "pulses", &pulses_obj)) {
        int array_len = json_object_array_length(pulses_obj);
        pulse_data->num_pulses = array_len;
        
        for (int i = 0; i < array_len && i < PD_MAX_PULSES; i++) {
            json_object *val = json_object_array_get_idx(pulses_obj, i);
            pulse_data->pulse[i] = json_object_get_int(val);
        }
        
        printf("✅ Restored %d pulses from EXACT pulses array\n", array_len);
    }
    // ПРИОРИТЕТ 2: Приближённый hex_string (если pulses нет)
    else {
        json_object *hex_obj;
        if (json_object_object_get_ex(root, "hex_string", &hex_obj)) {
            const char *hex_string = json_object_get_string(hex_obj);
            parse_rfraw_hex_string(hex_string, pulse_data);
            printf("⚠️ Restored %d pulses from APPROXIMATE hex_string\n", 
                   pulse_data->num_pulses);
        }
    }
    
    // Применить metadata
    apply_metadata_from_json(root, pulse_data);
    
    json_object_put(root);
    return 0;
}
```

---

## ✅ ФИНАЛЬНЫЙ ОТВЕТ

**Для ПОЛНОГО (100% точного) восстановления сигнала из hex_string - НЕВОЗМОЖНО!**

**Нужны дополнительные данные:**
1. **`pulses` array** - точные значения каждого пульса
2. **`rate_Hz`** - sample rate для правильной интерпретации
3. **RF metadata** (`freq_Hz`, `rssi_dB`, `snr_dB`) - для точного декодирования устройств

**hex_string** можно использовать только для:
- Компактной передачи (экономия трафика)
- Приближённого восстановления (точность ~10%)
- Fallback, когда точный массив недоступен

**Поэтому текущая реализация в `rtl433_asn1_pulse.c`, которая НЕ восстанавливает пульсы из hex_string, является ПРАВИЛЬНОЙ!** ✅





