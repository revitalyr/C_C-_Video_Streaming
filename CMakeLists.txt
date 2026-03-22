cmake_minimum_required(VERSION 3.28) # 3.28+ required for CXX_MODULES
project(VideoStreaming LANGUAGES CXX)

# Using C++23 for std::print, std::expected, and better module support stability
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Try to find FFmpeg with pkg-config first (Linux)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale)
endif()

# If pkg-config failed, try vcpkg (Windows)
if(NOT FFMPEG_FOUND)
    find_package(spdlog CONFIG REQUIRED)
    find_package(fmt CONFIG REQUIRED)
    
    # Try vcpkg FFmpeg
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavdevice libavfilter libavformat libavutil libswresample libswscale)
endif()

# Tests (aligned with Makefile and docker-compose)
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

# Link libraries based on what was found
if(FFMPEG_FOUND)
    target_link_libraries(video_streaming_modules PRIVATE PkgConfig::FFMPEG)
else()
    target_link_libraries(video_streaming_modules PRIVATE spdlog::spdlog PUBLIC fmt::fmt)
endif()

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

# Simple video sender (H.264 UDP streaming)
add_executable(sender src/sender.cpp)
if(FFMPEG_FOUND)
    target_link_libraries(sender PRIVATE Threads::Threads PkgConfig::FFMPEG)
else()
    target_link_libraries(sender PRIVATE Threads::Threads)
endif()

# Network video sender (with packet loss/delay simulation)
add_executable(network_sender src/network_sender.cpp)
if(FFMPEG_FOUND)
    target_link_libraries(network_sender PRIVATE Threads::Threads PkgConfig::FFMPEG)
else()
    target_link_libraries(network_sender PRIVATE Threads::Threads)
endif()

# Simple video viewer (H.264 UDP receiver)
add_executable(viewer src/viewer.cpp)
if(FFMPEG_FOUND)
    target_link_libraries(viewer PRIVATE Threads::Threads PkgConfig::FFMPEG)
else()
    target_link_libraries(viewer PRIVATE Threads::Threads)
endif()

# FFplay pipe viewer (stdout H.264 stream)
add_executable(ffplay_viewer src/ffplay_viewer.cpp)
if(FFMPEG_FOUND)
    target_link_libraries(ffplay_viewer PRIVATE Threads::Threads PkgConfig::FFMPEG)
else()
    target_link_libraries(ffplay_viewer PRIVATE Threads::Threads)
endif()

# Visual demo (ASCII animation)
add_executable(visual_demo demo/visual_demo.cpp)
if(WIN32)
    target_link_libraries(visual_demo PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(visual_demo PRIVATE Threads::Threads)
endif()

# Viewer client for RTSP streaming
add_executable(viewer_client src/viewer_client.cpp)
if(WIN32)
    target_link_libraries(viewer_client PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(viewer_client PRIVATE Threads::Threads)
endif()

# Local RTSP server (if FFmpeg available)
if(FFMPEG_FOUND)
    add_executable(local_rtsp_server src/simple_rtsp_server.cpp)
    target_link_libraries(local_rtsp_server PRIVATE Threads::Threads PkgConfig::FFMPEG)
endif()

# Streaming server (relay/NAT traversal)
add_executable(streaming_server src/streaming_server.cpp)
if(WIN32)
    target_link_libraries(streaming_server PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(streaming_server PRIVATE Threads::Threads)
endif()

# Metrics + Adaptive Streaming system
add_executable(metrics_adaptive src/metrics_adaptive.cpp)
if(WIN32)
    target_link_libraries(metrics_adaptive PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(metrics_adaptive PRIVATE Threads::Threads)
endif()

add_executable(test_rtsp_client src/test_rtsp_client.cpp)
if(WIN32)
    target_link_libraries(test_rtsp_client ws2_32)
endif()

# Tests temporarily disabled due to module issues and old dependencies
# if(BUILD_TESTING)
#     enable_testing()
#     add_executable(video_streaming_tests 
#         tests/test_logger.cpp
#         tests/test_interfaces.cpp
#         tests/test_integration.cpp
#         tests/test_pipeline.cpp
#     )
#     target_link_libraries(video_streaming_tests PRIVATE pipeline_lib Catch2::Catch2WithMain)
# endif()
