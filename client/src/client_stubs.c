/** @file
    Client stub functions for missing SDR dependencies.
    
    Copyright (C) 2024
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include <stdint.h>
#include <stdlib.h>

// Stub functions for missing dependencies that client needs
void set_log_level(int level) { (void)level; }
int rfraw_check(const char *data) { (void)data; return 0; }
int rfraw_parse(void *data, const char *input) { (void)data; (void)input; return 0; }

// RTL-SDR stub functions (always available for client-only mode)
uint32_t rtlsdr_get_device_count(void) { return 0; }
int rtlsdr_get_device_usb_strings(uint32_t index, char *vendor, char *product, char *serial) { 
    (void)index; (void)vendor; (void)product; (void)serial; return -1; 
}
int rtlsdr_open(void **dev, uint32_t index) { (void)dev; (void)index; return -1; }
const char* rtlsdr_get_device_name(uint32_t index) { (void)index; return "stub-device"; }
int rtlsdr_get_index_by_serial(const char *serial) { (void)serial; return -1; }
int rtlsdr_get_tuner_gains(void *dev, int *gains) { (void)dev; (void)gains; return 0; }
int rtlsdr_set_center_freq(void *dev, uint32_t freq) { (void)dev; (void)freq; return 0; }
uint32_t rtlsdr_get_center_freq(void *dev) { (void)dev; return 433920000; }
int rtlsdr_set_freq_correction(void *dev, int ppm) { (void)dev; (void)ppm; return 0; }
int rtlsdr_set_tuner_gain_mode(void *dev, int manual) { (void)dev; (void)manual; return 0; }
int rtlsdr_get_tuner_type(void *dev) { (void)dev; return 0; }
int rtlsdr_set_tuner_gain(void *dev, int gain) { (void)dev; (void)gain; return 0; }
int rtlsdr_set_sample_rate(void *dev, uint32_t rate) { (void)dev; (void)rate; return 0; }
uint32_t rtlsdr_get_sample_rate(void *dev) { (void)dev; return 250000; }
int rtlsdr_set_direct_sampling(void *dev, int on) { (void)dev; (void)on; return 0; }
int rtlsdr_set_offset_tuning(void *dev, int on) { (void)dev; (void)on; return 0; }
int rtlsdr_set_agc_mode(void *dev, int on) { (void)dev; (void)on; return 0; }
int rtlsdr_reset_buffer(void *dev) { (void)dev; return 0; }
int rtlsdr_read_async(void *dev, void *cb, void *ctx, uint32_t buf_num, uint32_t buf_len) { 
    (void)dev; (void)cb; (void)ctx; (void)buf_num; (void)buf_len; return -1; 
}
int rtlsdr_cancel_async(void *dev) { (void)dev; return 0; }
int rtlsdr_close(void *dev) { (void)dev; return 0; }

// SoapySDR stub functions (always available for client-only mode)
void* SoapySDRDevice_makeStrArgs(const char *args) { (void)args; return NULL; }
int SoapySDRDevice_getNativeStreamFormat(void *device, int direction, size_t channel, double *fullScale) { 
    (void)device; (void)direction; (void)channel; (void)fullScale; return 0; 
}
void SoapySDR_free(void *ptr) { (void)ptr; }
char* SoapySDRDevice_getHardwareInfo(void *device) { (void)device; return NULL; }
void SoapySDRKwargs_clear(void *kwargs) { (void)kwargs; }
void* SoapySDRDevice_setupStream(void *device, int direction, const char *format, size_t *channels, size_t numChans, void *args) {
    (void)device; (void)direction; (void)format; (void)channels; (void)numChans; (void)args; return NULL;
}
char* SoapySDRDevice_getHardwareKey(void *device) { (void)device; return NULL; }
char** SoapySDRDevice_listAntennas(void *device, int direction, size_t channel, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)length; return NULL;
}
void SoapySDRStrings_clear(char ***elems, size_t length) { (void)elems; (void)length; }
char** SoapySDRDevice_listGains(void *device, int direction, size_t channel, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)length; return NULL;
}
void* SoapySDRDevice_getGainRange(void *device, int direction, size_t channel, const char *name, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)name; (void)length; return NULL;
}
char** SoapySDRDevice_listFrequencies(void *device, int direction, size_t channel, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)length; return NULL;
}
void* SoapySDRDevice_getFrequencyRange(void *device, int direction, size_t channel, const char *name, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)name; (void)length; return NULL;
}
void* SoapySDRDevice_getSampleRateRange(void *device, int direction, size_t channel, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)length; return NULL;
}
void* SoapySDRDevice_getBandwidthRange(void *device, int direction, size_t channel, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)length; return NULL;
}
double SoapySDRDevice_getBandwidth(void *device, int direction, size_t channel) {
    (void)device; (void)direction; (void)channel; return 0;
}
char** SoapySDRDevice_getStreamFormats(void *device, int direction, size_t channel, size_t *length) {
    (void)device; (void)direction; (void)channel; (void)length; return NULL;
}
void SoapySDR_setLogLevel(int logLevel) { (void)logLevel; }
int SoapySDRDevice_setFrequency(void *device, int direction, size_t channel, double frequency, void *args) {
    (void)device; (void)direction; (void)channel; (void)frequency; (void)args; return 0;
}
double SoapySDRDevice_getFrequency(void *device, int direction, size_t channel) {
    (void)device; (void)direction; (void)channel; return 433920000;
}
int SoapySDRDevice_setFrequencyComponent(void *device, int direction, size_t channel, const char *name, double frequency, void *args) {
    (void)device; (void)direction; (void)channel; (void)name; (void)frequency; (void)args; return 0;
}
int SoapySDRDevice_hasGainMode(void *device, int direction, size_t channel) {
    (void)device; (void)direction; (void)channel; return 0;
}
char* SoapySDRDevice_getDriverKey(void *device) { (void)device; return NULL; }
int SoapySDRDevice_setGainMode(void *device, int direction, size_t channel, int automatic) {
    (void)device; (void)direction; (void)channel; (void)automatic; return 0;
}
int SoapySDRDevice_setGainElement(void *device, int direction, size_t channel, const char *name, double value) {
    (void)device; (void)direction; (void)channel; (void)name; (void)value; return 0;
}
int SoapySDRDevice_setGain(void *device, int direction, size_t channel, double value) {
    (void)device; (void)direction; (void)channel; (void)value; return 0;
}
double SoapySDRDevice_getGain(void *device, int direction, size_t channel, const char *name) {
    (void)device; (void)direction; (void)channel; (void)name; return 0;
}
int SoapySDRDevice_setAntenna(void *device, int direction, size_t channel, const char *name) {
    (void)device; (void)direction; (void)channel; (void)name; return 0;
}
char* SoapySDRDevice_getAntenna(void *device, int direction, size_t channel) {
    (void)device; (void)direction; (void)channel; return NULL;
}
int SoapySDRDevice_setSampleRate(void *device, int direction, size_t channel, double rate) {
    (void)device; (void)direction; (void)channel; (void)rate; return 0;
}
double SoapySDRDevice_getSampleRate(void *device, int direction, size_t channel) {
    (void)device; (void)direction; (void)channel; return 250000;
}
void* SoapySDRKwargs_fromString(const char *markup) { (void)markup; return NULL; }
int SoapySDRDevice_setBandwidth(void *device, int direction, size_t channel, double bw) {
    (void)device; (void)direction; (void)channel; (void)bw; return 0;
}
int SoapySDRDevice_writeSetting(void *device, const char *key, const char *value) {
    (void)device; (void)key; (void)value; return 0;
}
char* SoapySDRDevice_lastError(void) { return NULL; }
int SoapySDRDevice_activateStream(void *device, void *stream, int flags, long long timeNs, size_t numElems) {
    (void)device; (void)stream; (void)flags; (void)timeNs; (void)numElems; return 0;
}
int SoapySDRDevice_deactivateStream(void *device, void *stream, int flags, long long timeNs) {
    (void)device; (void)stream; (void)flags; (void)timeNs; return 0;
}
int SoapySDRDevice_closeStream(void *device, void *stream) {
    (void)device; (void)stream; return 0;
}
int SoapySDRDevice_readStream(void *device, void *stream, void **buffs, size_t numElems, int *flags, long long *timeNs, long timeoutUs) {
    (void)device; (void)stream; (void)buffs; (void)numElems; (void)flags; (void)timeNs; (void)timeoutUs; return -1;
}
int SoapySDRDevice_unmake(void *device) { (void)device; return 0; }
void SoapySDR_registerLogHandler(void *handler) { (void)handler; }
char* libusb_strerror(int error_code) { (void)error_code; return "stub error"; }
char* libusb_error_name(int error_code) { (void)error_code; return "STUB_ERROR"; }
