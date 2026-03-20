cmake_minimum_required(VERSION 3.30)

project(VideoStreaming VERSION 1.0.0 LANGUAGES CXX)

# --- 1. Настройки стандарта ---
# CMake 3.30 не умеет маппить CXX_STANDARD=26 на флаги MSVC —
# выставляем 23 как базу, а C++26 включаем вручную через флаги компилятора.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Убираем этот флаг — он не существует в CMake и ничего не делает
# set(CMAKE_CXX_FLAGS_EXPERIMENTAL ON)

if(MSVC)
    # /std:c++latest — единственный способ получить C++26 на MSVC сейчас.
    # /Zc:__cplusplus — без этого MSVC врёт в __cplusplus (возвращает 199711L).
    # /experimental:module — нужен для C++20 модулей на старых тулчейнах;
    #   на VS 2022 17.5+ модули работают без него, но с ним не сломается.
    add_compile_options(
        /std:c++latest
        /Zc:__cplusplus
        /utf-8
        /W4
        /permissive-
        /EHsc
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601
    )

    # Отключаем CMake-овский автоматический выбор флага стандарта для MSVC,
    # чтобы он не добавлял /std:c++23 поверх нашего /std:c++latest.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
    # Сбрасываем переменную, которую CMake использует для вставки /std:c++NN
    set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "")
    set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC 15+ имеет частичную поддержку C++26
    add_compile_options(-std=c++26 -Wall -Wextra -pedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang 18+ — используем c++2c (публичное имя C++26 до финализации)
    add_compile_options(-std=c++2c -Wall -Wextra -pedantic)
endif()

# --- 2. Зависимости ---
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)

# --- 3. Библиотека ---
add_library(video_streaming_lib STATIC)

target_sources(video_streaming_lib
    PUBLIC
        FILE_SET CXX_MODULES FILES
            "common/logger.ixx"
            "common/interfaces.ixx"
            "common/std.ixx"
    PRIVATE
        "common/logger.cpp"
)

target_include_directories(video_streaming_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(video_streaming_lib
    PUBLIC
        spdlog::spdlog
        Threads::Threads
)

# --- 4. Приложение ---
add_executable(video_streaming_app "src/main.cpp")
target_link_libraries(video_streaming_app PRIVATE video_streaming_lib)

# --- 5. Тесты ---
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()

    find_package(Catch2 3.0 REQUIRED)

    add_executable(video_streaming_tests
        tests/test_logger.cpp
        tests/test_interfaces.cpp
        tests/test_integration.cpp
    )

    # CXX_SCAN_FOR_MODULES наследуется от глобального CMAKE_CXX_SCAN_FOR_MODULES,
    # явно дублировать set_target_properties не обязательно, но не вредит.
    target_link_libraries(video_streaming_tests
        PRIVATE
            video_streaming_lib
            Catch2::Catch2WithMain
    )

    add_test(NAME AllTests COMMAND video_streaming_tests)
endif()