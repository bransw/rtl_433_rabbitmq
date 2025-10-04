# Makefile для тестирования функций общей библиотеки RTL433

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -D_GNU_SOURCE
INCLUDES = -I../include -I../../include -I../../src
LIBS = -ljson-c -lm

# Исходные файлы общей библиотеки (для статической сборки)
SHARED_SOURCES = ../src/rtl433_signal.c ../src/rtl433_transport.c ../src/rtl433_config.c

# ASN.1 support detection
ASN1_BUILD_DIR = ../../build/shared/asn1-generated
ASN1_LIB_DIR = ../../build/shared
ASN1_AVAILABLE := $(shell [ -d "$(ASN1_BUILD_DIR)" ] && [ -f "$(ASN1_LIB_DIR)/librtl433_shared.a" ] && echo yes || echo no)

# Основные цели
.PHONY: all clean test test-signal test-simple test-transport demo test-asn1

all: test_signal_simple test_signal_reconstruction simple_transport_example demo_real_data
ifeq ($(ASN1_AVAILABLE),yes)
	@echo "🔧 ASN.1 support detected, also building ASN.1 test..."
	$(MAKE) test_asn1_encode_decode_pulses
endif

# Простой тест реконструкции (без зависимостей)
test_signal_simple: test_signal_simple.c
	@echo "🔨 Сборка простого теста реконструкции..."
	$(CC) $(CFLAGS) -o $@ $< -ljson-c

# Тест реконструкции сигналов
test_signal_reconstruction: test_signal_reconstruction.c
	@echo "🔨 Сборка теста реконструкции сигналов..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(SHARED_SOURCES) $(LIBS)

# Простой пример транспорта
simple_transport_example: simple_transport_example.c
	@echo "🔨 Сборка примера транспорта..."
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(SHARED_SOURCES) $(LIBS) -lrabbitmq

# Демонстрация с реальными данными
demo_real_data: demo_real_data.c
	@echo "🔨 Сборка демо с реальными данными..."
	$(CC) $(CFLAGS) -o $@ $< -ljson-c

# Запуск тестов
test: test-simple

test-simple: test_signal_simple
	@echo "🧪 Запуск простых тестов реконструкции..."
	./test_signal_simple

test-signal: test_signal_reconstruction
	@echo "🧪 Запуск тестов реконструкции сигналов..."
	./test_signal_reconstruction

test-transport: simple_transport_example
	@echo "🧪 Запуск теста транспорта (требует RabbitMQ)..."
	./simple_transport_example

demo: demo_real_data
	@echo "🎬 Запуск демонстрации с реальными данными..."
	./demo_real_data

# ASN.1 test (requires shared library with ASN.1 support)
test_asn1_encode_decode_pulses: ../../test_asn1_encode_decode_pulses.c
ifeq ($(ASN1_AVAILABLE),yes)
	@echo "🔨 Сборка теста ASN.1 кодирования/декодирования..."
	$(CC) $(CFLAGS) -DENABLE_ASN1 \
		-I../include -I../../include -I$(ASN1_BUILD_DIR) \
		-o $@ $< \
		-L$(ASN1_LIB_DIR) -lrtl433_shared -L../../build/src -ldata $(LIBS)
else
	@echo "⚠️  ASN.1 support not available. Build project with: cd ../../build && cmake -DENABLE_ASN1=ON .. && make"
endif

# Run ASN.1 test
test-asn1: test_asn1_encode_decode_pulses
ifeq ($(ASN1_AVAILABLE),yes)
	@echo "🧪 Запуск тестов ASN.1 кодирования/декодирования..."
	./test_asn1_encode_decode_pulses
else
	@echo "⚠️  ASN.1 test not available."
endif

# Очистка
clean:
	@echo "🧹 Очистка..."
	rm -f test_signal_simple test_signal_reconstruction simple_transport_example demo_real_data test_asn1_encode_decode_pulses

# Справка
help:
	@echo "Доступные команды:"
	@echo "  make all           - собрать все тесты и демо"
	@echo "  make test          - запустить простые тесты"
	@echo "  make demo          - запустить демо с реальными данными"
	@echo "  make test-signal   - запустить тесты реконструкции"
	@echo "  make test-transport - запустить тест транспорта"
	@echo "  make test-asn1     - запустить тест ASN.1 (требует ASN.1 поддержку)"
	@echo "  make clean         - очистить собранные файлы"
	@echo ""
	@echo "ASN.1 Support Status: $(ASN1_AVAILABLE)"
ifeq ($(ASN1_AVAILABLE),no)
	@echo "  To enable ASN.1: cd ../../build && cmake -DENABLE_ASN1=ON .. && make"
endif
