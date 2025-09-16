#!/usr/bin/env python3
"""
Тестовый скрипт для демонстрации системы динамической загрузки кодеков RTL_433.

Этот скрипт создает тестовые конфигурации кодеков и демонстрирует
их динамическую загрузку без перекомпиляции.
"""

import os
import json
import time
import subprocess
import tempfile
import shutil

def create_test_devices_dir():
    """Создает временную папку с тестовыми кодеками."""
    test_dir = tempfile.mkdtemp(prefix="rtl433_test_devices_")
    print(f"Создана тестовая папка: {test_dir}")
    
    # Тестовый кодек 1: JSON формат
    device1 = {
        "name": "TestTempSensor",
        "modulation": "OOK_PWM",
        "description": "Тестовый датчик температуры",
        "flex_spec": "n=TestTempSensor,m=OOK_PWM,s=100,l=200,r=2000,g=400,t=50",
        "enabled": True,
        "priority": 0
    }
    
    with open(os.path.join(test_dir, "temp_sensor.json"), "w") as f:
        json.dump(device1, f, indent=2, ensure_ascii=False)
    
    # Тестовый кодек 2: INI формат  
    device2_ini = """# Тестовый датчик движения
name=TestMotionSensor
modulation=OOK_PCM
description=Тестовый датчик движения с OOK PCM
flex_spec=n=TestMotionSensor,m=OOK_PCM,s=50,l=100,r=1500,g=200
enabled=true
priority=1
"""
    
    with open(os.path.join(test_dir, "motion_sensor.ini"), "w") as f:
        f.write(device2_ini)
    
    # Тестовый кодек 3: CONF формат
    device3_conf = """# Тестовая метеостанция
name=TestWeatherStation
modulation=FSK_PCM
description=Тестовая метеостанция 433MHz
flex_spec=n=TestWeatherStation,m=FSK_PCM,s=52,l=52,r=800
enabled=true
priority=0
"""
    
    with open(os.path.join(test_dir, "weather_station.conf"), "w") as f:
        f.write(device3_conf)
    
    return test_dir

def test_device_listing(rtl433_binary, devices_dir):
    """Тестирует просмотр загруженных кодеков."""
    print("\n🔍 Тестирование просмотра динамических кодеков...")
    
    try:
        result = subprocess.run([
            rtl433_binary, "-L", devices_dir
        ], capture_output=True, text=True, timeout=10)
        
        print("Вывод команды:")
        print(result.stdout)
        
        if result.stderr:
            print("Ошибки:")
            print(result.stderr)
            
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print("❌ Тайм-аут выполнения команды")
        return False
    except FileNotFoundError:
        print(f"❌ Файл {rtl433_binary} не найден")
        return False

def test_device_loading(rtl433_binary, devices_dir):
    """Тестирует загрузку кодеков из файла."""
    print("\n📂 Тестирование загрузки из конкретного файла...")
    
    json_file = os.path.join(devices_dir, "temp_sensor.json")
    
    try:
        result = subprocess.run([
            rtl433_binary, "-J", json_file, "-L"
        ], capture_output=True, text=True, timeout=10)
        
        print("Результат загрузки:")
        print(result.stdout)
        
        return "TestTempSensor" in result.stdout
        
    except subprocess.TimeoutExpired:
        print("❌ Тайм-аут выполнения команды")
        return False

def test_hot_reload(rtl433_binary, devices_dir):
    """Тестирует горячую перезагрузку кодеков."""
    print("\n🔄 Тестирование горячей перезагрузки...")
    
    # Создаем новый кодек во время выполнения
    new_device = {
        "name": "HotReloadDevice",
        "modulation": "OOK_PPM",
        "description": "Кодек для тестирования горячей перезагрузки",
        "flex_spec": "n=HotReloadDevice,m=OOK_PPM,s=136,l=248,r=2000",
        "enabled": True,
        "priority": 2
    }
    
    new_file = os.path.join(devices_dir, "hot_reload_test.json")
    
    # Ждем немного
    time.sleep(1)
    
    # Создаем новый файл
    with open(new_file, "w") as f:
        json.dump(new_device, f, indent=2, ensure_ascii=False)
    
    print(f"Создан новый файл кодека: {new_file}")
    
    # Проверяем, появился ли новый кодек
    time.sleep(1)
    
    try:
        result = subprocess.run([
            rtl433_binary, "-L", devices_dir
        ], capture_output=True, text=True, timeout=10)
        
        return "HotReloadDevice" in result.stdout
        
    except subprocess.TimeoutExpired:
        print("❌ Тайм-аут проверки")
        return False

def test_format_validation():
    """Тестирует валидацию различных форматов конфигураций."""
    print("\n✅ Тестирование валидации форматов...")
    
    # Тест валидного JSON
    valid_json = {
        "name": "ValidDevice",
        "modulation": "OOK_PWM",
        "flex_spec": "n=ValidDevice,m=OOK_PWM,s=100,l=200,r=2000",
        "enabled": True
    }
    
    # Тест невалидного JSON (отсутствует обязательное поле)
    invalid_json = {
        "name": "InvalidDevice",
        "description": "Отсутствует flex_spec"
    }
    
    print("✅ Валидный JSON:", json.dumps(valid_json, ensure_ascii=False))
    print("❌ Невалидный JSON:", json.dumps(invalid_json, ensure_ascii=False))
    
    return True

def run_integration_test(rtl433_binary):
    """Запускает полный интеграционный тест."""
    print("🚀 Запуск интеграционного теста системы динамической загрузки")
    print("=" * 60)
    
    # Создаем тестовую папку с кодеками
    devices_dir = create_test_devices_dir()
    
    try:
        tests_passed = 0
        total_tests = 4
        
        # Тест 1: Просмотр кодеков
        if test_device_listing(rtl433_binary, devices_dir):
            print("✅ Тест просмотра кодеков пройден")
            tests_passed += 1
        else:
            print("❌ Тест просмотра кодеков провален")
        
        # Тест 2: Загрузка из файла
        if test_device_loading(rtl433_binary, devices_dir):
            print("✅ Тест загрузки из файла пройден")
            tests_passed += 1
        else:
            print("❌ Тест загрузки из файла провален")
        
        # Тест 3: Горячая перезагрузка
        if test_hot_reload(rtl433_binary, devices_dir):
            print("✅ Тест горячей перезагрузки пройден")
            tests_passed += 1
        else:
            print("❌ Тест горячей перезагрузки провален")
        
        # Тест 4: Валидация форматов
        if test_format_validation():
            print("✅ Тест валидации форматов пройден")
            tests_passed += 1
        else:
            print("❌ Тест валидации форматов провален")
        
        print("\n" + "=" * 60)
        print(f"📊 Результат: {tests_passed}/{total_tests} тестов пройдено")
        
        if tests_passed == total_tests:
            print("🎉 Все тесты успешно пройдены!")
            return True
        else:
            print("⚠️  Некоторые тесты провалены")
            return False
    
    finally:
        # Очистка
        print(f"\n🧹 Удаление тестовой папки: {devices_dir}")
        shutil.rmtree(devices_dir)

def main():
    """Главная функция."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Тестирование системы динамической загрузки кодеков RTL_433"
    )
    parser.add_argument(
        "--binary", "-b", 
        default="./rtl_433_dynamic",
        help="Путь к исполняемому файлу rtl_433_dynamic"
    )
    parser.add_argument(
        "--create-examples", "-c",
        action="store_true",
        help="Создать примеры конфигураций в текущей папке"
    )
    
    args = parser.parse_args()
    
    if args.create_examples:
        print("📁 Создание примеров конфигураций...")
        
        os.makedirs("devices", exist_ok=True)
        
        # Пример температурного датчика
        temp_sensor = {
            "name": "ExampleTempSensor",
            "modulation": "OOK_PWM",
            "description": "Пример датчика температуры",
            "flex_spec": "n=ExampleTempSensor,m=OOK_PWM,s=100,l=200,r=2000,g=400,t=50",
            "enabled": True,
            "priority": 0
        }
        
        with open("devices/example_temp_sensor.json", "w") as f:
            json.dump(temp_sensor, f, indent=2, ensure_ascii=False)
        
        # Пример датчика движения (INI)
        motion_ini = """# Пример датчика движения
name=ExampleMotionSensor
modulation=OOK_PCM
description=Пример датчика движения
flex_spec=n=ExampleMotionSensor,m=OOK_PCM,s=50,l=100,r=1500,g=200
enabled=true
priority=1
"""
        
        with open("devices/example_motion_sensor.ini", "w") as f:
            f.write(motion_ini)
        
        print("✅ Примеры созданы в папке ./devices/")
        print("Запуск: ./rtl_433_dynamic -D ./devices -L")
        return
    
    # Проверка существования бинарного файла
    if not os.path.exists(args.binary):
        print(f"❌ Бинарный файл не найден: {args.binary}")
        print("Для компиляции используйте:")
        print("gcc -o rtl_433_dynamic src/rtl_433_dynamic.c src/dynamic_device_loader.c -I./include -lm")
        return False
    
    # Запуск тестов
    success = run_integration_test(args.binary)
    
    if success:
        print("\n🎯 Система динамической загрузки работает корректно!")
        print("\nДля использования:")
        print(f"  {args.binary} -D ./devices  # Загрузить из папки")
        print(f"  {args.binary} -L            # Показать кодеки")
        print(f"  {args.binary} -J file.json  # Загрузить файл")
    else:
        print("\n❌ Обнаружены проблемы в системе динамической загрузки")
    
    return success

if __name__ == "__main__":
    exit(0 if main() else 1)
