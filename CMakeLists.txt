cmake_minimum_required(VERSION 3.28) # 3.28+ необходим для CXX_MODULES
project(VideoStreaming LANGUAGES CXX)

# Using C++23 for std::print, std::expected, and better module support stability
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Принудительный libc++ для Clang (согласовано с Dockerfile)
# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     add_compile_options(-stdlib=libc++)
#     add_link_options(-stdlib=libc++)
# endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Тесты (согласовано с Makefile и docker-compose)
if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)
endif()

# Library for C++ modules
add_library(video_streaming_modules)
target_sources(video_streaming_modules
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
    common/logger.ixx
    common/interfaces.ixx
    common/std.ixx
    PRIVATE
    common/logger.cpp # Module implementation
)
target_link_libraries(video_streaming_modules PRIVATE spdlog::spdlog PUBLIC fmt::fmt)

# Library for traditional C++ source files
add_library(video_streaming_impl
    network/sender.cpp
    network/receiver.cpp
    network/udp_socket.cpp
    media/frame.cpp
    media/synthetic_encoder.cpp
    rtp/h264_packetizer.cpp
    rtp/h264_depacketizer.cpp
    rtp/rtp_packet.cpp
    jitter/jitter_buffer.cpp
    common/time.cpp
    network/endpoint.cpp
    rtp/rtsp_client.cpp
)
# The implementation depends on modules and pthreads
target_link_libraries(video_streaming_impl PUBLIC video_streaming_modules PRIVATE Threads::Threads)

# Library for pipeline module, which sits on top of implementation
add_library(pipeline_lib)
target_sources(pipeline_lib
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
    pipeline/pipeline.ixx
)
target_link_libraries(pipeline_lib PUBLIC video_streaming_impl)

# Настоящий тест для sender/receiver (без симуляций)
add_executable(test_real_video_streaming tests/test_real_video_streaming.cpp)
target_link_libraries(test_real_video_streaming PRIVATE Threads::Threads video_streaming_impl)

# Тестовый RTSP клиент
add_executable(test_rtsp_client tests/test_rtsp_client.cpp)
target_link_libraries(test_rtsp_client PRIVATE video_streaming_impl)
if(WIN32)
    target_link_libraries(test_rtsp_client PRIVATE ws2_32)
    target_link_libraries(video_streaming_impl PRIVATE ws2_32)
endif()

# Главное приложение
add_executable(video_streaming_app src/main.cpp)
target_link_libraries(video_streaming_app PRIVATE pipeline_lib fmt::fmt)

# Тесты (согласовано с Makefile и docker-compose)
if(BUILD_TESTING)
    enable_testing()
    add_executable(video_streaming_tests 
        tests/test_logger.cpp
        tests/test_interfaces.cpp
        tests/test_integration.cpp
        tests/test_pipeline.cpp
    )
    target_link_libraries(video_streaming_tests PRIVATE pipeline_lib Catch2::Catch2WithMain)
    
    # Добавление тестов в CTest
    add_test(NAME SenderReceiverTest COMMAND test_sender_receiver)
endif()
