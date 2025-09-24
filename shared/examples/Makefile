# Makefile для тестирования функций общей библиотеки RTL433

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -D_GNU_SOURCE
INCLUDES = -I../include -I../../include -I../../src
LIBS = -ljson-c -lm

# Исходные файлы общей библиотеки (для статической сборки)
SHARED_SOURCES = ../src/rtl433_signal.c ../src/rtl433_transport.c ../src/rtl433_config.c

# Основные цели
.PHONY: all clean test test-signal test-simple test-transport demo

all: test_signal_simple test_signal_reconstruction simple_transport_example demo_real_data

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

# Очистка
clean:
	@echo "🧹 Очистка..."
	rm -f test_signal_simple test_signal_reconstruction simple_transport_example demo_real_data

# Справка
help:
	@echo "Доступные команды:"
	@echo "  make all           - собрать все тесты и демо"
	@echo "  make test          - запустить простые тесты"
	@echo "  make demo          - запустить демо с реальными данными"
	@echo "  make test-signal   - запустить тесты реконструкции"
	@echo "  make test-transport - запустить тест транспорта" 
	@echo "  make clean         - очистить собранные файлы"
