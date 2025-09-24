@echo off
REM Script for building split architecture rtl_433 on Windows

setlocal EnableDelayedExpansion

REM Параметры по умолчанию
set BUILD_DIR=build_split
set BUILD_TYPE=Release
set BUILD_CLIENT=ON
set BUILD_SERVER=ON
set BUILD_ORIGINAL=OFF
set ENABLE_MQTT=ON
set ENABLE_RABBITMQ=OFF
set INSTALL_PREFIX=C:\Program Files\rtl433_split
set JOBS=%NUMBER_OF_PROCESSORS%

REM Цвета для вывода (если поддерживается)
set "RED=[31m"
set "GREEN=[32m"
set "YELLOW=[33m"
set "BLUE=[34m"
set "NC=[0m"

:parse_args
if "%~1"=="" goto :main
if "%~1"=="-h" goto :show_help
if "%~1"=="--help" goto :show_help
if "%~1"=="-d" (
    set BUILD_DIR=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--build-dir" (
    set BUILD_DIR=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="-t" (
    set BUILD_TYPE=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--build-type" (
    set BUILD_TYPE=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="-j" (
    set JOBS=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--jobs" (
    set JOBS=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--no-client" (
    set BUILD_CLIENT=OFF
    shift
    goto :parse_args
)
if "%~1"=="--no-server" (
    set BUILD_SERVER=OFF
    shift
    shift
    goto :parse_args
)
if "%~1"=="--with-original" (
    set BUILD_ORIGINAL=ON
    shift
    goto :parse_args
)
if "%~1"=="--no-mqtt" (
    set ENABLE_MQTT=OFF
    shift
    goto :parse_args
)
if "%~1"=="--with-rabbitmq" (
    set ENABLE_RABBITMQ=ON
    shift
    goto :parse_args
)
if "%~1"=="--clean" (
    set CLEAN=1
    shift
    goto :parse_args
)
if "%~1"=="--install" (
    set INSTALL=1
    shift
    goto :parse_args
)
echo Unknown parameter: %~1
goto :show_help

:show_help
echo Использование: %~nx0 [опции]
echo.
echo Опции:
echo   -h, --help              Показать эту справку
echo   -d, --build-dir DIR     Директория для сборки (по умолчанию: %BUILD_DIR%)
echo   -t, --build-type TYPE   Тип сборки: Release, Debug, RelWithDebInfo (по умолчанию: %BUILD_TYPE%)
echo   -j, --jobs JOBS         Количество параллельных задач (по умолчанию: %JOBS%)
echo   --no-client             Не собирать клиент
echo   --no-server             Не собирать сервер
echo   --with-original         Собирать оригинальный rtl_433
echo   --no-mqtt               Отключить поддержку MQTT
echo   --with-rabbitmq         Включить поддержку RabbitMQ
echo   --clean                 Очистить директорию сборки
echo   --install               Установить после сборки
echo.
echo Примеры:
echo   %~nx0                      Базовая сборка
echo   %~nx0 --clean --install    Пересборка и установка
echo   %~nx0 --with-rabbitmq      Сборка с поддержкой RabbitMQ
echo   %~nx0 --no-server -j 8     Только клиент, 8 потоков
exit /b 0

:print_info
echo %BLUE%[INFO]%NC% %~1
exit /b 0

:print_success
echo %GREEN%[SUCCESS]%NC% %~1
exit /b 0

:print_warning
echo %YELLOW%[WARNING]%NC% %~1
exit /b 0

:print_error
echo %RED%[ERROR]%NC% %~1
exit /b 0

:check_dependencies
call :print_info "Проверка зависимостей..."

REM Проверка CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    call :print_error "CMake не найден"
    exit /b 1
)

REM Проверка компилятора
cl >nul 2>&1
if errorlevel 1 (
    gcc --version >nul 2>&1
    if errorlevel 1 (
        call :print_error "Компилятор не найден (ни MSVC, ни GCC)"
        exit /b 1
    )
)

call :print_success "Основные зависимости найдены"
exit /b 0

:clean_build
if exist "%BUILD_DIR%" (
    call :print_info "Очистка директории сборки: %BUILD_DIR%"
    rmdir /s /q "%BUILD_DIR%"
    call :print_success "Директория сборки очищена"
)
exit /b 0

:configure_project
call :print_info "Конфигурация проекта..."

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DBUILD_CLIENT=%BUILD_CLIENT% -DBUILD_SERVER=%BUILD_SERVER% -DBUILD_ORIGINAL=%BUILD_ORIGINAL% -DENABLE_MQTT=%ENABLE_MQTT% -DENABLE_RABBITMQ=%ENABLE_RABBITMQ% -f ..\CMakeLists_split.txt ..

call :print_info "Выполнение: cmake %CMAKE_ARGS%"

cmake %CMAKE_ARGS%
if errorlevel 1 (
    call :print_error "Ошибка конфигурации"
    cd ..
    exit /b 1
)

call :print_success "Конфигурация завершена успешно"
cd ..
exit /b 0

:build_project
call :print_info "Сборка проекта..."

cd "%BUILD_DIR%"

cmake --build . --config %BUILD_TYPE% -j %JOBS%
if errorlevel 1 (
    call :print_error "Ошибка сборки"
    cd ..
    exit /b 1
)

call :print_success "Сборка завершена успешно"
cd ..
exit /b 0

:install_project
call :print_info "Установка проекта..."

cd "%BUILD_DIR%"

cmake --install . --config %BUILD_TYPE%
if errorlevel 1 (
    call :print_error "Ошибка установки"
    cd ..
    exit /b 1
)

call :print_success "Установка завершена успешно"
cd ..
exit /b 0

:print_build_info
call :print_info "Конфигурация сборки:"
echo   Директория сборки: %BUILD_DIR%
echo   Тип сборки: %BUILD_TYPE%
echo   Префикс установки: %INSTALL_PREFIX%
echo   Параллельные задачи: %JOBS%
echo   Собирать клиент: %BUILD_CLIENT%
echo   Собирать сервер: %BUILD_SERVER%
echo   Собирать оригинальный rtl_433: %BUILD_ORIGINAL%
echo   Поддержка MQTT: %ENABLE_MQTT%
echo   Поддержка RabbitMQ: %ENABLE_RABBITMQ%
echo.
exit /b 0

:main
call :print_info "Начало сборки разделенной архитектуры rtl_433"

call :print_build_info

if defined CLEAN call :clean_build

call :check_dependencies
if errorlevel 1 exit /b 1

call :configure_project
if errorlevel 1 exit /b 1

call :build_project
if errorlevel 1 exit /b 1

if defined INSTALL (
    call :install_project
    if errorlevel 1 exit /b 1
)

call :print_success "Сборка завершена успешно!"

REM Вывод информации о созданных файлах
call :print_info "Созданные исполняемые файлы:"
if "%BUILD_CLIENT%"=="ON" (
    if exist "%BUILD_DIR%\rtl_433_client.exe" (
        echo   rtl_433_client: %CD%\%BUILD_DIR%\rtl_433_client.exe
    )
)
if "%BUILD_SERVER%"=="ON" (
    if exist "%BUILD_DIR%\rtl_433_server.exe" (
        echo   rtl_433_server: %CD%\%BUILD_DIR%\rtl_433_server.exe
    )
)
if "%BUILD_ORIGINAL%"=="ON" (
    if exist "%BUILD_DIR%\src\rtl_433.exe" (
        echo   rtl_433: %CD%\%BUILD_DIR%\src\rtl_433.exe
    )
)

call :print_info "Для запуска:"
if "%BUILD_CLIENT%"=="ON" (
    echo   Клиент: %BUILD_DIR%\rtl_433_client.exe --help
)
if "%BUILD_SERVER%"=="ON" (
    echo   Сервер: %BUILD_DIR%\rtl_433_server.exe --help
)

endlocal
exit /b 0



