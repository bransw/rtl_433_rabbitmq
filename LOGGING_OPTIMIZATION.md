# Logging Optimization for rtl_433_client

## Problem

The `rtl_433_client` was generating excessive debug output, especially with `-vvv` parameter, making it impossible to stop the application normally due to output flooding.

## Solution

Optimized logging levels and reduced frequency of debug messages:

### 1. **Default Verbosity Reduced**
- **Before**: `LOG_DEBUG` (very verbose by default)
- **After**: `LOG_INFO` (normal operation level)

### 2. **Callback Logging Optimized**
- **Before**: Every 100 callbacks logged at LOG_INFO
- **After**: Every 1000 callbacks logged at LOG_DEBUG, or LOG_TRACE for all

### 3. **Data Callback Logging Reduced**
- **Before**: First 5 callbacks + all DEBUG level
- **After**: First 3 callbacks + LOG_TRACE level only

### 4. **AM Sample Logging Restricted**
- **Before**: Always logged for first 2 packages at LOG_DEBUG
- **After**: Only at LOG_TRACE level

### 5. **Pulse Detection Logging Limited**
- **Before**: Always logged for first 3 callbacks at LOG_DEBUG
- **After**: First 3 callbacks OR LOG_TRACE level

## Verbosity Levels Guide

| Level | Flag | Description | Use Case |
|-------|------|-------------|----------|
| `LOG_INFO` | (none) | Normal operation | Production use |
| `LOG_DEBUG` | `-v` | Verbose logging | Debugging issues |
| `LOG_TRACE` | `-vv` | Very verbose | Deep debugging |
| `LOG_TRACE+` | `-vvv` | Maximum verbosity | **Use with caution!** |

## Usage Examples

### Normal Operation (Recommended)
```bash
./rtl_433_client -T amqp://localhost:5672/rtl_433 -r test.cu8
```

### Debugging
```bash
./rtl_433_client -v -T amqp://localhost:5672/rtl_433 -r test.cu8
```

### Deep Debugging (Use Carefully)
```bash
./rtl_433_client -vv -T amqp://localhost:5672/rtl_433 -r test.cu8
```

### Maximum Verbosity (May Flood Output!)
```bash
./rtl_433_client -vvv -T amqp://localhost:5672/rtl_433 -r test.cu8
```

## Before vs After Comparison

### Before Optimization (with `-vvv`):
```
Client: rtl_433_client started
SDR: Callback #1: ev=0x1, demod=0x..., exit=0
SDR: Data callback #1: 131072 samples (131072 total), buf=0x...
Client: AM samples: 2 2 5 5 1 325 857 2770
SDR: Pulse detection: type=0, OOK_pulses=0, FSK_pulses=0
SDR: Callback #2: ev=0x1, demod=0x..., exit=0
SDR: Data callback #2: 131072 samples (262144 total), buf=0x...
Client: AM samples: 234 970 360 1040 2314 4615 -1225 294
SDR: Pulse detection: type=0, OOK_pulses=0, FSK_pulses=0
[... hundreds more lines ...]
```

### After Optimization (with `-vvv`):
```
Client: rtl_433_client started
Client: Connected to server: localhost:8080
Client: Starting signal demodulation...
Client: Processing CU8 file: test.cu8
Client: Detected OOK package #1 with 1 pulses
Client: OOK package #1: no devices decoded
Client: Detected FSK package #2 with 59 pulses
Client: FSK package #2 decoded 2 device events
Client: File processing completed. Found 8 packages
```

## Benefits

1. **Controllable Output**: Application can be stopped normally even with high verbosity
2. **Performance**: Reduced I/O overhead from excessive logging
3. **Usability**: Clear verbosity level guidance for users
4. **Debugging**: Still provides detailed info when needed at appropriate levels

## Implementation Details

### Key Changes Made:

1. **Default verbosity**: `LOG_INFO` instead of `LOG_DEBUG`
2. **Callback frequency**: 1000x instead of 100x intervals
3. **Conditional logging**: Most debug info only at `LOG_TRACE` level
4. **Help text**: Added verbosity level explanation

### Files Modified:

- `client/src/rtl_433_client_v2.c`: Main logging optimization
- Help text updated with verbosity level guidance

The optimization maintains all debugging capabilities while making the application usable at all verbosity levels.

