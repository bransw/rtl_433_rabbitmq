# Flex Decoder Examples

This document provides practical examples of using flex decoders with `rtl_433_client`.

## Basic Usage

```bash
# Single flex decoder
./rtl_433_client -T amqp://guest:guest@localhost:5672/rtl_433 \
                 -X 'n=my_device,m=OOK_PWM,s=100,l=200,r=1000'

# Multiple flex decoders
./rtl_433_client -T amqp://guest:guest@localhost:5672/rtl_433 \
                 -X 'n=dfsk_variant1,m=FSK_PCM,s=52,l=52,r=1000' \
                 -X 'n=ook_sensor,m=OOK_PWM,s=150,l=300,r=2000' \
                 -X 'n=manchester_device,m=OOK_MC_ZEROBIT,s=100,l=100,r=5000'
```

## Common Modulation Types

### OOK (On-Off Keying)

#### Pulse Width Modulation (PWM)
```bash
# Basic PWM decoder
-X 'n=pwm_device,m=OOK_PWM,s=100,l=200,r=1000'

# PWM with sync and tolerances
-X 'n=advanced_pwm,m=OOK_PWM,s=150,l=300,r=2000,g=500,t=50,y=1000'
```

#### Pulse Position Modulation (PPM)
```bash
# Basic PPM decoder
-X 'n=ppm_device,m=OOK_PPM,s=100,l=200,r=1500'

# PPM with gap tolerance
-X 'n=ppm_with_gap,m=OOK_PPM,s=120,l=240,r=2000,g=400'
```

#### Pulse Code Modulation (PCM)
```bash
# Basic PCM decoder
-X 'n=pcm_device,m=OOK_PCM,s=100,l=100,r=1000'
```

#### Manchester Encoding
```bash
# Manchester with zero bit
-X 'n=manchester_dev,m=OOK_MC_ZEROBIT,s=100,l=100,r=5000'
```

### FSK (Frequency Shift Keying)

#### FSK PCM
```bash
# Basic FSK PCM
-X 'n=fsk_pcm_device,m=FSK_PCM,s=52,l=52,r=1000'

# FSK PCM with specific timing
-X 'n=tire_sensor,m=FSK_PCM,s=40,l=40,r=800'
```

#### FSK PWM
```bash
# FSK PWM decoder
-X 'n=fsk_pwm_device,m=FSK_PWM,s=100,l=200,r=1500'
```

## Real-World Examples

### Based on pulse_analyzer Recommendations

#### Example 1: PPM Signal
From `pulse_analyzer` output:
```
Use a flex decoder with -X 'n=name,m=OOK_PPM,s=136,l=248,g=252,r=252'
```

Implement as:
```bash
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=mystery_ppm,m=OOK_PPM,s=136,l=248,g=252,r=252'
```

#### Example 2: PWM with Sync
From `pulse_analyzer` output:
```
Use a flex decoder with -X 'n=name,m=OOK_PWM,s=552,l=90856,r=116,g=0,t=0,y=116'
```

Implement as:
```bash
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=pwm_sync_device,m=OOK_PWM,s=552,l=90856,r=116,g=0,t=0,y=116'
```

### TPMS (Tire Pressure Monitoring)

```bash
# Toyota TPMS variant
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=toyota_tpms_variant,m=FSK_PCM,s=50,l=50,r=1200'

# Generic TPMS
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=generic_tpms,m=FSK_PWM,s=40,l=80,r=1000'
```

### Weather Sensors

```bash
# Temperature/Humidity sensor
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=weather_pwm,m=OOK_PWM,s=200,l=400,r=3000'

# Rain gauge
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=rain_gauge,m=OOK_PPM,s=100,l=300,r=2000'
```

### Remote Controls

```bash
# Gate remote
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=gate_remote,m=OOK_PWM,s=300,l=900,r=10000'

# Car key fob
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=car_keyfob,m=OOK_PPM,s=400,l=1200,r=15000'
```

## Parameter Tuning Guide

### Timing Parameters (in microseconds)

- **`s=<short>`**: Short pulse/bit width
- **`l=<long>`**: Long pulse/bit width  
- **`r=<reset>`**: Reset/packet separation limit
- **`g=<gap>`**: Gap between pulses tolerance
- **`t=<tolerance>`**: Timing tolerance for pulse matching
- **`y=<sync>`**: Sync pulse width

### Tuning Tips

1. **Start with pulse_analyzer recommendations** - Run with `-v` to get suggested parameters
2. **Adjust timing gradually** - Change parameters by 10-20% increments
3. **Monitor decode success** - Check logs for "decoded X device events"
4. **Use multiple variants** - Try different parameter sets for the same signal type

### Common Issues and Solutions

#### Issue: No decodes despite signal detection
```bash
# Try increasing tolerance
-X 'n=device,m=OOK_PWM,s=100,l=200,r=1000,t=20'

# Try adjusting reset limit
-X 'n=device,m=OOK_PWM,s=100,l=200,r=1500'
```

#### Issue: Too many false positives
```bash
# Increase reset limit to require longer gaps
-X 'n=device,m=OOK_PWM,s=100,l=200,r=2000'

# Add sync requirement
-X 'n=device,m=OOK_PWM,s=100,l=200,r=1000,y=500'
```

## Monitoring and Debugging

### Log Messages to Watch

```
Client: Added flex decoder: n=my_device,m=OOK_PWM,s=100,l=200,r=1000
Client: Registered 1 flex decoder(s)
Client: FSK package #2 decoded 2 device events (including 'my_device')
```

### Verifying Queue Routing

With flex decoders, decoded signals go to the `detected` queue:

```json
{
  "type": "device_data",
  "device": "General purpose decoder 'my_device'",
  "time": "2025-09-12 14:30:15",
  "model": "my_device",
  "decoded": {...}
}
```

## Advanced Examples

### Multiple Frequency Variants

```bash
# Handle slight frequency variations
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=device_var1,m=OOK_PWM,s=100,l=200,r=1000' \
                 -X 'n=device_var2,m=OOK_PWM,s=95,l=190,r=1000' \
                 -X 'n=device_var3,m=OOK_PWM,s=105,l=210,r=1000'
```

### Mixed Modulation Types

```bash
# Support both OOK and FSK variants of same device
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -X 'n=sensor_ook,m=OOK_PWM,s=150,l=300,r=2000' \
                 -X 'n=sensor_fsk,m=FSK_PCM,s=50,l=50,r=1200'
```

### Production Configuration

```bash
# Comprehensive setup for unknown signal analysis
./rtl_433_client -T amqp://localhost:5672/rtl_433 \
                 -f 433.92M -s 250k -g 40 -v \
                 -X 'n=pwm_short,m=OOK_PWM,s=100,l=200,r=1000' \
                 -X 'n=pwm_medium,m=OOK_PWM,s=200,l=400,r=2000' \
                 -X 'n=pwm_long,m=OOK_PWM,s=400,l=800,r=5000' \
                 -X 'n=ppm_fast,m=OOK_PPM,s=100,l=300,r=1500' \
                 -X 'n=ppm_slow,m=OOK_PPM,s=300,l=900,r=5000' \
                 -X 'n=fsk_tpms,m=FSK_PCM,s=50,l=50,r=1200' \
                 -X 'n=manchester,m=OOK_MC_ZEROBIT,s=100,l=100,r=3000'
```

## Integration with Analysis Tools

Use flex decoders with the triq.org analysis workflow:

1. **Capture unknown signals** with routing to `ook_raw`/`fsk_raw` queues
2. **Analyze pulse data** using the `rabbitmq_to_triq.py` utility
3. **Get modulation recommendations** from triq.org visualization
4. **Create flex decoder** based on analysis results
5. **Deploy new decoder** and monitor `detected` queue for results

This creates a complete signal analysis pipeline from capture to decoding!

