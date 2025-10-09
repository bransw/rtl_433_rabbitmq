# Анализ: Как short_width и long_width связаны с массивом pulses

## 📐 ОСНОВНАЯ ФОРМУЛА

```c
// Из src/pulse_slicer.c (строка 67-69):
float samples_per_us = pulses->sample_rate / 1.0e6f;
int s_short = device->short_width * samples_per_us;  // microseconds → samples
int s_long  = device->long_width * samples_per_us;   // microseconds → samples
```

### Конвертация:
- **device->short_width** и **device->long_width** задаются в **микросекундах (μs)**
- **pulse[i]** в массиве хранится в **samples** (отсчётах АЦП)
- **Конвертация:** `samples = microseconds × (sample_rate / 1000000)`

---

## 🔍 ДЛЯ TOYOTA TPMS

### Параметры декодера:
```c
r_device const tpms_toyota = {
    .name        = "Toyota TPMS",
    .modulation  = FSK_PULSE_PCM,
    .short_width = 52,        // 52 μs (микросекунды)
    .long_width  = 52,        // 52 μs (для FSK short == long)
    .reset_limit = 150,       // 150 μs
};
```

### Конвертация в samples:
```c
sample_rate = 250000 Hz
samples_per_us = 250000 / 1000000 = 0.25 samples/μs

s_short = 52 μs × 0.25 = 13 samples
s_long  = 52 μs × 0.25 = 13 samples
s_reset = 150 μs × 0.25 = 37.5 samples ≈ 38 samples
```

---

## 📊 АНАЛИЗ ВАШЕГО СИГНАЛА

### Оригинальные pulses:
```json
"pulses": [40, 60, 40, 64, 40, 52, 52, 96, 200, 104, 96, 52, 48, ...]
"rate_Hz": 250000
```

### Конвертация в микросекунды:
```
samples → μs: time_us = samples / (sample_rate / 1000000)

pulse[0] = 40 samples  → 40 / 0.25 = 160 μs
pulse[1] = 60 samples  → 60 / 0.25 = 240 μs
pulse[2] = 40 samples  → 40 / 0.25 = 160 μs
pulse[3] = 64 samples  → 64 / 0.25 = 256 μs
pulse[4] = 40 samples  → 40 / 0.25 = 160 μs
pulse[5] = 52 samples  → 52 / 0.25 = 208 μs  ✅ Близко к short_width=52 μs? НЕТ!
pulse[6] = 52 samples  → 52 / 0.25 = 208 μs
pulse[7] = 96 samples  → 96 / 0.25 = 384 μs
```

### 🚨 ПРОБЛЕМА!

**Toyota TPMS ожидает:**
- `short_width = 52 μs` → **13 samples** @ 250kHz

**Ваш сигнал имеет:**
- Минимальные пульсы: **40-64 samples** → **160-256 μs**
- Это **в 12-20 раз больше**, чем ожидает декодер!

---

## ❓ КАК ЭТО ВОЗМОЖНО?

### Объяснение: Комментарий в коде!

```c
// Из tpms_toyota.c (строка 119):
.short_width = 52,  // 12-13 samples @250k
```

**Комментарий говорит:** `12-13 samples @250k`

**НО параметр:** `short_width = 52` (μs)

### Расчёт по комментарию:
```
12.5 samples @ 250kHz = 12.5 / 0.25 = 50 μs ✅
```

**Комментарий ПРАВИЛЬНЫЙ:** 52 μs ≈ 13 samples @ 250kHz

---

## 🧪 ПОЧЕМУ ВАШ СИГНАЛ ИМЕЕТ 40-64 SAMPLES?

### Гипотеза 1: Это НЕ "чистые" пульсы Toyota TPMS

Ваш массив `pulses` может содержать:
- **Не демодулированный FSK сигнал** (raw FSK transitions)
- **Несколько периодов FSK** на один "логический" пульс

### Гипотеза 2: Это результат FSK демодуляции

FSK сигнал имеет две частоты:
```json
"freq1_Hz": 433942688,  // FSK частота 1
"freq2_Hz": 433878368,  // FSK частота 2
"fsk_f1_est": 5951,     // FSK estimate
"fsk_f2_est": -10916    // FSK estimate
```

**После FSK демодуляции** получаются пульсы, которые **не равны** базовому `short_width`!

---

## 🎯 ПРАВИЛЬНОЕ ПОНИМАНИЕ

### 1. **short_width и long_width** - это **параметры ДЕКОДЕРА**

Они определяют:
- Какую длительность пульса считать "коротким" битом
- Какую длительность считать "длинным" битом
- **Tolerance (допуск):** обычно ±25%

### 2. **Массив pulses** - это **RAW данные**

Это:
- Реальные измеренные длительности пульсов
- Могут **варьироваться** вокруг `short_width`/`long_width`
- Декодер **классифицирует** их как "короткие" или "длинные"

### 3. **Алгоритм классификации** (из pulse_slicer.c):

```c
s_tolerance = s_long / 4;  // default tolerance is ±25%

// Для каждого пульса:
if (pulses->pulse[n] >= s_short - s_tolerance &&
    pulses->pulse[n] <= s_short + s_tolerance) {
    // Это КОРОТКИЙ пульс → бит 0 (или 1, зависит от кодирования)
}
else if (pulses->pulse[n] >= s_long - s_tolerance &&
         pulses->pulse[n] <= s_long + s_tolerance) {
    // Это ДЛИННЫЙ пульс → бит 1 (или 0)
}
```

---

## 📊 ДЛЯ TOYOTA TPMS (FSK_PULSE_PCM)

### FSK_PULSE_PCM означает:

```c
.short_width = 52,  // 52 μs = 13 samples @ 250kHz
.long_width  = 52,  // Для FSK обычно short == long (фиксированный период)
```

### Tolerance (допуск):

```c
s_tolerance = s_long / 4 = 13 / 4 = 3.25 samples ≈ 3 samples

Допустимый диапазон:
  10 samples (40 μs) ≤ pulse ≤ 16 samples (64 μs)
```

### Сравнение с вашим сигналом:

```
Ваш сигнал: [40, 60, 40, 64, 40, 52, 52, ...]
             ↓   ↓   ↓   ↓   ↓   ↓   ↓
Ожидается:  [13, 13, 13, 13, 13, 13, 13, ...] (±3 samples)
            (10-16 samples допустимо)

40 samples > 16 samples ❌ ВНЕ ДОПУСКА!
60 samples > 16 samples ❌ ВНЕ ДОПУСКА!
64 samples > 16 samples ❌ ВНЕ ДОПУСКА!
```

---

## 🚨 КРИТИЧЕСКАЯ ПРОБЛЕМА!

### Ваш сигнал имеет пульсы **40-200 samples**

**Toyota TPMS ожидает пульсы 10-16 samples (±25% от 13)**

### Это означает:

1. **Либо ваш сигнал НЕ от Toyota TPMS**
2. **Либо это RAW FSK data (не демодулированный)**
3. **Либо sample_rate НЕПРАВИЛЬНЫЙ** (должен быть другой)

---

## 🧪 ПРОВЕРКА: Какой должен быть sample_rate?

Если ваш сигнал **правильный**, но `sample_rate` неправильный:

```
Ваш минимальный пульс: 40 samples
Toyota ожидает: 13 samples (для 52 μs @ 250kHz)

Коэффициент: 40 / 13 = 3.08x

Правильный sample_rate = 250000 / 3.08 ≈ 81 kHz ???
```

**НЕТ!** Это не имеет смысла. rtl_433 использует стандартные sample rates (250kHz, 1024kHz).

---

## 💡 ПРАВИЛЬНОЕ ОБЪЯСНЕНИЕ

### Ваш массив `pulses` - это **FSK transitions**, а не **bits**!

**FSK сигнал работает так:**

```
FSK частота 1 (433.94 MHz): период ~2.3 μs
FSK частота 2 (433.88 MHz): период ~2.3 μs
```

**После захвата @ 250kHz:**
- 1 период FSK = ~0.5 samples
- "Пульс" 40 samples = ~80 периодов FSK = ~160 μs

**Toyota TPMS PCM:**
- Каждый "бит" = ~52 μs = 13 samples битового периода
- Но это **после FSK демодуляции**!

### Ваш массив - это **до демодуляции**!

```
RAW FSK samples → [40, 60, 40, 64, ...] (ваш массив)
         ↓
   FSK demod
         ↓
PCM pulses → [13, 13, 13, 13, ...] (что ожидает decoder)
```

---

## ✅ ВЫВОД

### 1. **short_width и long_width** задаются в **микросекундах**

```c
.short_width = 52,  // 52 μs
```

### 2. **Массив pulses хранит значения в samples**

```
pulse[i] = количество samples (отсчётов АЦП)
```

### 3. **Конвертация:**

```c
time_us = pulse[i] / (sample_rate / 1000000)
time_us = pulse[i] / 0.25  // для 250kHz

pulse[5] = 52 samples → 52/0.25 = 208 μs (НЕ 52 μs!)
```

### 4. **Ваш сигнал:**

```
"pulses": [40, 60, 40, 64, 40, 52, 52, ...]
```

Это **160-256 μs**, а Toyota TPMS ожидает **~52 μs (13 samples)**.

**Ваш массив - это скорее всего RAW FSK transitions, а не демодулированные биты!**

---

## 🎯 ЧТО ЭТО ЗНАЧИТ ДЛЯ ВОССТАНОВЛЕНИЯ?

### Для правильного декодирования Toyota TPMS нужно:

1. **FSK демодуляция** (преобразование двух частот в битовый поток)
2. **PCM slicing** (классификация пульсов как битов)
3. **Differential Manchester декодирование**
4. **CRC проверка**

### Ваш массив `pulses` - это **step 0** (до демодуляции)

**Поэтому прямое восстановление из hex_string НЕ работает** - hex_string содержит упрощённый histogram, который **теряет FSK информацию**!

---

## 📊 ФИНАЛЬНАЯ ТАБЛИЦА

| Параметр | В коде декодера | В вашем сигнале | Соответствие |
|----------|----------------|-----------------|--------------|
| **short_width** | 52 μs (13 samples) | 40-64 samples = 160-256 μs | ❌ НЕТ (12-20x больше) |
| **long_width** | 52 μs (13 samples) | 96-200 samples = 384-800 μs | ❌ НЕТ (29-61x больше) |
| **Tolerance** | ±25% (10-16 samples) | Вариация 40-200 samples | ❌ ВНЕ ДОПУСКА |
| **FSK freq1** | - | 433942688 Hz | ✅ Есть |
| **FSK freq2** | - | 433878368 Hz | ✅ Есть |
| **FSK estimates** | - | f1=5951, f2=-10916 | ✅ Есть |

**ВЫВОД:** Ваш массив `pulses` содержит **RAW FSK data**, требующий демодуляции!

**Для успешного декодирования нужны FSK параметры + точный массив pulses!** ✅





