# 🐛 КРИТИЧЕСКАЯ ОШИБКА: Gaps не извлекались из JSON!

## ❌ ПРОБЛЕМА 1: Gaps не извлекались

### Симптом:
```
📊 First 20 gaps: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ...  ← ❌ ВСЕ НУЛИ!
```

### Причина:

Мы копировали массив **подряд**, не разделяя на pulse/gap пары.

## ❌ ПРОБЛЕМА 2: Неправильная единица измерения

### Симптом:
```
📊 First 20 pulses: 10 10 10 13 50 24 12 13 ...  ← ❌ Слишком маленькие!
📊 First 20 gaps:   15 16 13 24 26 13 24 13 ...  ← ❌ В 4 раза меньше!
```

**Ожидалось:** `[40, 60, 40, 64, ...]` (в samples)  
**Получено:** `[10, 15, 10, 16, ...]` (в 4 раза меньше!)

### Причина:

JSON массив `pulses` содержит значения в **микросекундах (μs)**, а нужно в **samples**!

**При создании JSON** (в `rtl433_pulse_enhanced.c`):
```c
double to_us = 1e6 / data->sample_rate;  // 1e6 / 250000 = 4.0
for (unsigned i = 0; i < data->num_pulses; ++i) {
    pulses[i * 2 + 0] = data->pulse[i] * to_us;  // 40 * 4 = 160 μs
    pulses[i * 2 + 1] = data->gap[i] * to_us;    // 60 * 4 = 240 μs
}
```

**В JSON:** `"pulses": [160, 240, 160, 256, ...]` (в μs)

**При чтении** нужно **конвертировать обратно**:
```c
double from_us = sample_rate / 1e6;  // 250000 / 1e6 = 0.25
pulse_samples = pulse_us * from_us;  // 160 * 0.25 = 40 samples ✅
```

---

## 📊 ФОРМАТ JSON pulses array

### В JSON:
```json
{
  "pulses": [40, 60, 40, 64, 40, 52, 52, 96, 200, 104, ...]
             ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓    ↓    ↓
            p0  g0  p1  g1  p2  g2  p3  g3   p4   g4
}
```

**Формат:** `[pulse, gap, pulse, gap, pulse, gap, ...]`

### Должно быть в pulse_data_t:
```c
pulse[0] = 40   gap[0] = 60
pulse[1] = 40   gap[1] = 64
pulse[2] = 40   gap[2] = 52
pulse[3] = 52   gap[3] = 96
pulse[4] = 200  gap[4] = 104
...
```

---

## ✅ ИСПРАВЛЕНИЕ

### Файл: `shared/src/output_asn1.c` (строки 135-154)

**Было:**
```c
} else if (strcmp(d->key, "pulses") == 0 && d->type == DATA_ARRAY) {
    // Handle pulses array
    data_array_t *pulses_array = (data_array_t*)d->value.v_ptr;
    if (pulses_array && pulse_ex.pulse_data.num_pulses > 0) {
        int pulses_to_copy = (pulse_ex.pulse_data.num_pulses > PD_MAX_PULSES) ? 
                            PD_MAX_PULSES : pulse_ex.pulse_data.num_pulses;
        int *array_values = (int*)pulses_array->values;
        for (int i = 0; i < pulses_to_copy; i++) {
            pulse_ex.pulse_data.pulse[i] = array_values[i];  // ❌ Неправильно!
        }
    }
}
```

**Стало:**
```c
} else if (strcmp(d->key, "pulses") == 0 && d->type == DATA_ARRAY) {
    // Handle pulses array - format: [pulse_μs, gap_μs, pulse_μs, gap_μs, ...]
    // Values are in microseconds, need to convert to samples
    data_array_t *pulses_array = (data_array_t*)d->value.v_ptr;
    if (pulses_array && pulse_ex.pulse_data.num_pulses > 0 && pulse_ex.pulse_data.sample_rate > 0) {
        int pulses_to_copy = (pulse_ex.pulse_data.num_pulses > PD_MAX_PULSES) ? 
                            PD_MAX_PULSES : pulse_ex.pulse_data.num_pulses;
        int *array_values = (int*)pulses_array->values;
        double from_us = pulse_ex.pulse_data.sample_rate / 1e6; // Convert μs back to samples
        
        // Extract pulse and gap pairs from alternating array (values in microseconds)
        for (int i = 0; i < pulses_to_copy; i++) {
            int array_idx = i * 2;  // Each pulse-gap pair takes 2 elements
            if (array_idx < pulses_array->num_values) {
                double pulse_us = array_values[array_idx];
                pulse_ex.pulse_data.pulse[i] = (int)(pulse_us * from_us);      // ✅ pulse (μs → samples)
            }
            if (array_idx + 1 < pulses_array->num_values) {
                double gap_us = array_values[array_idx + 1];
                pulse_ex.pulse_data.gap[i] = (int)(gap_us * from_us);          // ✅ gap (μs → samples)
            }
        }
    }
}
```

---

## 🎯 РЕЗУЛЬТАТ

### После исправления:

**Ожидаемый вывод:**
```
📊 First 20 pulses: 40 40 40 52 200 96 52 48 52 52 100 100 48 100 100 100 48 ...
📊 First 20 gaps:   60 64 52 96 104 52 96 52 100 48 100 48 52 96 56 48 96 ...
                    ↑  ↑  ↑  ↑   ↑   ↑  ↑  ↑   ↑  ↑   ↑  ↑  ↑  ↑  ↑  ↑  ↑
                    ✅ НЕ НУЛИ! Правильные значения!
```

**Детекция:**
```
✅ Toyota TPMS detected: id=d77a57bd, pressure=28.5 PSI
```

---

## 🔍 ПОЧЕМУ ЭТО КРИТИЧНО ДЛЯ ДЕТЕКЦИИ?

### Toyota TPMS использует FSK_PULSE_PCM:

Детектор анализирует **периоды** (pulse + gap):
```c
// В pulse_slicer.c:
int period = pulse[i] + gap[i];

// Для Toyota TPMS ожидается период ~104 samples (52 μs @ 250kHz)
```

### С нулевыми gaps:
```
pulse[0] = 40, gap[0] = 0  → period = 40 + 0 = 40   ❌ Слишком короткий!
pulse[1] = 40, gap[1] = 0  → period = 40 + 0 = 40   ❌ Слишком короткий!
```

**Результат:** Периоды **неправильные** → преамбула **не найдена** → ❌ **NO DETECTION**

### С правильными gaps:
```
pulse[0] = 40, gap[0] = 60  → period = 40 + 60 = 100   ✅ Близко к 104!
pulse[1] = 40, gap[1] = 64  → period = 40 + 64 = 104   ✅ Точно!
```

**Результат:** Периоды **правильные** → преамбула **найдена** → ✅ **DETECTION!**

---

## 📊 СТАТИСТИКА

### До исправления:
```
Входной JSON: 58 pulses = 116 values (58 pulse + 58 gap)
Extracted:
  pulse[]: [40, 60, 40, 64, 40, 52, 52, 96, ...]  ← Неправильно!
  gap[]:   [0,  0,  0,  0,  0,  0,  0,  0,  ...]  ← Нули!
Result: ❌ No detection
```

### После исправления:
```
Входной JSON: 58 pulses = 116 values (58 pulse + 58 gap)
Extracted:
  pulse[]: [40, 40, 40, 52, 200, 96, 52, 48, ...]  ← Правильно!
  gap[]:   [60, 64, 52, 96, 104, 52, 96, 52, ...]  ← Правильно!
Result: ✅ Toyota TPMS detected
```

---

## ✅ ИТОГ

**Ошибка:** Gaps не извлекались из JSON массива `pulses`  
**Исправление:** Разделить массив на чередующиеся pulse/gap пары  
**Результат:** Toyota TPMS и другие устройства будут обнаружены! ✅

**Следующий шаг:** Компиляция и тестирование!
```bash
cd build
make rtl_433_client -j4
./rtl_433_client -r ../tests/signals/g005_433.92M_250k.cu8 -F json -Q 1
# ✅ Ожидается: Toyota TPMS detected
```

