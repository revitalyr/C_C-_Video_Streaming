cmake_minimum_required(VERSION 3.30) # Minimum for C++26 support

project(VideoStreaming VERSION 1.0.0 LANGUAGES CXX)

# --- 1. Настройки стандарта и модулей ---
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Включаем сканирование исходников на наличие модулей
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Включаем все экспериментальные возможности C++26
set(CMAKE_CXX_FLAGS_EXPERIMENTAL ON)

# Для Windows/MSVC: UTF-8 и подавление лишних предупреждений
if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /std:c++latest /experimental:c++26)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0601)
endif()

# Для GCC/Clang: включаем C++26
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-std=c++26 -Wall -Wextra -pedantic)
endif()

# --- 2. Поиск зависимостей ---
# Используем vcpkg или системный менеджер пакетов
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)

# --- 3. Создание библиотеки с логгером (video_streaming_lib) ---
# Используем C++26 модули для максимальной производительности
add_library(video_streaming_lib STATIC)

# Добавляем файлы модулей и реализации
target_sources(video_streaming_lib
    PUBLIC
        FILE_SET CXX_MODULES FILES
            "common/logger.ixx"          # Интерфейс модуля логгера
            "common/interfaces.ixx"      # Интерфейс модуля типов
            "common/std.ixx"             # Интерфейс модуля std
    PRIVATE
        "common/logger.cpp"             # Реализация логгера
)

# Настройка путей инклюдов
target_include_directories(video_streaming_lib PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Линковка зависимостей для библиотеки
target_link_libraries(video_streaming_lib 
    PUBLIC 
        spdlog::spdlog
        Threads::Threads
)

# --- 4. Основное приложение ---
add_executable(video_streaming_app "src/main.cpp")

# Линкуем нашу библиотеку (модули подтянутся автоматически)
target_link_libraries(video_streaming_app PRIVATE video_streaming_lib)

# --- 5. Тесты (Catch2 v3) ---
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    
    # Ищем Catch2 v3
    find_package(Catch2 3.0 REQUIRED)
    
    # Создаем исполняемый файл для тестов - используем модули C++26
    add_executable(video_streaming_tests 
        tests/test_logger.cpp
        tests/test_interfaces.cpp  
        tests/test_integration.cpp
    )
    
    # Включаем сканирование модулей для тестов
    set_target_properties(video_streaming_tests PROPERTIES CXX_SCAN_FOR_MODULES ON)
    
    # Линкуем зависимости
    target_link_libraries(video_streaming_tests 
        PRIVATE 
            video_streaming_lib 
            Catch2::Catch2WithMain
    )
    
    add_test(NAME AllTests COMMAND video_streaming_tests)
endif()