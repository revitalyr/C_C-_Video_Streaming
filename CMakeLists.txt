cmake_minimum_required(VERSION 3.20)
project(VideoStreaming VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Compiler flags
include(compiler_flags)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)

# Common library
add_library(video_streaming_common STATIC
    common/types.cpp
    common/logging.cpp
    common/time.cpp
)

target_include_directories(video_streaming_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Network library
add_library(video_streaming_network STATIC
    network/udp_socket.cpp
    network/endpoint.cpp
)

target_link_libraries(video_streaming_network PUBLIC video_streaming_common)

# RTP library
add_library(video_streaming_rtp STATIC
    rtp/rtp_packet.cpp
    rtp/h264_packetizer.cpp
    rtp/h264_depacketizer.cpp
    rtp/rtcp.cpp
)

target_link_libraries(video_streaming_rtp PUBLIC 
    video_streaming_network
    video_streaming_common
)

# Jitter buffer library
add_library(video_streaming_jitter STATIC
    jitter/jitter_buffer.cpp
)

target_link_libraries(video_streaming_jitter PUBLIC video_streaming_rtp)

# Media library
add_library(video_streaming_media STATIC
    media/frame.cpp
    media/ffmpeg_h264_encoder.cpp
    media/synthetic_decoder.cpp
)

target_link_libraries(video_streaming_media PUBLIC
    video_streaming_common
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWSCALE_LIBRARIES}
)

target_include_directories(video_streaming_media PRIVATE
    ${AVCODEC_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWSCALE_INCLUDE_DIRS}
)

# Pipeline library
add_library(video_streaming_pipeline STATIC
    pipeline/stage.cpp
    pipeline/pipeline.cpp
)

target_link_libraries(video_streaming_pipeline PUBLIC
    video_streaming_media
    video_streaming_rtp
    video_streaming_jitter
)

# Sender executable
add_executable(video_sender
    sender/camera_source.cpp
    sender/sender_pipeline.cpp
    sender/main.cpp
)

target_link_libraries(video_sender PRIVATE
    video_streaming_pipeline
    video_streaming_network
)

# Receiver executable
add_executable(video_receiver
    receiver/receiver_pipeline.cpp
    receiver/sdl_renderer.cpp
    receiver/main.cpp
)

target_link_libraries(video_receiver PRIVATE
    video_streaming_pipeline
    video_streaming_network
)

# Testing
enable_testing()
add_subdirectory(tests)

# Installation
include(GNUInstallDirs)

install(TARGETS video_streaming_common video_streaming_network video_streaming_rtp
    video_streaming_jitter video_streaming_media video_streaming_pipeline
    video_sender video_receiver
    EXPORT VideoStreamingTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY common/ network/ rtp/ jitter/ media/ pipeline/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/video_streaming
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT VideoStreamingTargets
    FILE VideoStreamingTargets.cmake
    NAMESPACE VideoStreaming::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VideoStreaming
)
