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

# Find SDL2 for GUI viewer
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()
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
    core/common/logger.ixx
    core/common/interfaces.ixx
    core/common/std.ixx
    core/session/sender/video_sender.ixx
    core/session/receiver/video_receiver.ixx
    core/transport/rtp/h264_packetizer.ixx
    core/transport/rtp/h264_depacketizer.ixx
    core/video/jitter/jitter_buffer.ixx
    PRIVATE
    core/common/logger.cpp # Module implementation
    core/session/sender/video_sender.cpp
    core/session/receiver/video_receiver.cpp
    core/transport/rtp/h264_packetizer.cpp
    core/transport/rtp/h264_depacketizer.cpp
    core/video/jitter/jitter_buffer.cpp
)

# Link libraries based on what was found
if(FFMPEG_FOUND)
    target_link_libraries(video_streaming_modules PRIVATE PkgConfig::FFMPEG)
else()
    target_link_libraries(video_streaming_modules PRIVATE spdlog::spdlog PUBLIC fmt::fmt)
endif()
# Add include directories for modules so they can find core headers (e.g. logger.cpp needs udp_socket.hpp)
target_include_directories(video_streaming_modules PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)

# Library for traditional C++ source files
add_library(video_streaming_impl
    core/transport/network/sender.cpp
    core/transport/network/receiver.cpp
    core/transport/network/udp_socket.cpp
    core/video/media/frame.cpp
    core/video/media/synthetic_encoder.cpp
    core/transport/rtp/rtp_packet.cpp
    core/common/time.cpp
    core/transport/network/endpoint.cpp
    core/transport/rtp/rtsp_client.cpp
    # High-level components
)
# The implementation depends on modules and pthreads
target_link_libraries(video_streaming_impl PUBLIC video_streaming_modules PRIVATE Threads::Threads)
# Allow including headers from consolidated paths (preserves #include "network/..." etc.)
target_include_directories(video_streaming_impl PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)

# Library for pipeline module, which sits on top of implementation
add_library(pipeline_lib)
target_sources(pipeline_lib
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
    core/pipeline/pipeline.ixx
)
target_link_libraries(pipeline_lib PUBLIC video_streaming_impl)

# Simple video sender (H.264 UDP streaming)
add_executable(sender apps/basic/sender.cpp)
target_include_directories(sender PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(FFMPEG_FOUND)
    target_link_libraries(sender PRIVATE Threads::Threads PkgConfig::FFMPEG)
else()
    target_link_libraries(sender PRIVATE Threads::Threads)
endif()

# Network video sender (with packet loss/delay simulation)
add_executable(network_sender apps/basic/network_sender.cpp)
target_include_directories(network_sender PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(FFMPEG_FOUND)
    target_link_libraries(network_sender PRIVATE video_streaming_impl PkgConfig::FFMPEG)
else()
    target_link_libraries(network_sender PRIVATE video_streaming_impl)
endif()

# Simple video viewer (H.264 UDP receiver)
add_executable(viewer apps/basic/viewer.cpp)
target_include_directories(viewer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(FFMPEG_FOUND)
    target_link_libraries(viewer PRIVATE video_streaming_impl PkgConfig::FFMPEG)
else()
    target_link_libraries(viewer PRIVATE video_streaming_impl)
endif()

# FFplay pipe viewer (stdout H.264 stream)
add_executable(ffplay_viewer apps/vis/ffplay_viewer.cpp)
target_include_directories(ffplay_viewer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(FFMPEG_FOUND)
    target_link_libraries(ffplay_viewer PRIVATE Threads::Threads PkgConfig::FFMPEG)
else()
    target_link_libraries(ffplay_viewer PRIVATE Threads::Threads)
endif()

# Visual demo (ASCII animation)
add_executable(visual_demo apps/vis/visual_demo.cpp)
# Add core include path for demos that might need it
target_include_directories(visual_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(WIN32)
    target_link_libraries(visual_demo PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(visual_demo PRIVATE Threads::Threads)
endif()

# SDL2 GUI Viewer
if(FFMPEG_FOUND AND SDL2_FOUND)
    add_executable(sdl_viewer apps/vis/sdl_viewer.cpp)
    target_include_directories(sdl_viewer PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
        ${CMAKE_CURRENT_SOURCE_DIR}/core/video
        ${CMAKE_CURRENT_SOURCE_DIR}/core/session
    )
    target_link_libraries(sdl_viewer PRIVATE video_streaming_impl PkgConfig::FFMPEG)
    if(TARGET SDL2::SDL2)
        target_link_libraries(sdl_viewer PRIVATE SDL2::SDL2)
    elseif(TARGET PkgConfig::SDL2)
        target_link_libraries(sdl_viewer PRIVATE PkgConfig::SDL2)
    endif()
endif()

# Viewer client for RTSP streaming
add_executable(viewer_client apps/rtsp/viewer_client.cpp)
target_include_directories(viewer_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(WIN32)
    target_link_libraries(viewer_client PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(viewer_client PRIVATE Threads::Threads)
endif()

# Local RTSP server (if FFmpeg available)
if(FFMPEG_FOUND)
    add_executable(local_rtsp_server apps/rtsp/simple_rtsp_server.cpp)
    target_include_directories(local_rtsp_server PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
        ${CMAKE_CURRENT_SOURCE_DIR}/core/video
        ${CMAKE_CURRENT_SOURCE_DIR}/core/session
    )
    target_link_libraries(local_rtsp_server PRIVATE Threads::Threads PkgConfig::FFMPEG)
endif()

# Streaming server (relay/NAT traversal)
add_executable(streaming_server apps/exp/streaming_server.cpp)
target_include_directories(streaming_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(WIN32)
    target_link_libraries(streaming_server PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(streaming_server PRIVATE Threads::Threads)
endif()

# Metrics + Adaptive Streaming system
add_executable(metrics_adaptive apps/exp/metrics_adaptive.cpp)
target_include_directories(metrics_adaptive PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
if(WIN32)
    target_link_libraries(metrics_adaptive PRIVATE Threads::Threads ws2_32)
else()
    target_link_libraries(metrics_adaptive PRIVATE Threads::Threads)
endif()

add_executable(test_rtsp_client apps/rtsp/test_rtsp_client.cpp)
target_include_directories(test_rtsp_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core/video
    ${CMAKE_CURRENT_SOURCE_DIR}/core/session
)
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
