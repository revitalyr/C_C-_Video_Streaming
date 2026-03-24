# Base image with build tools and FFmpeg/SDL dependencies
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    pkg-config \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libsdl2-dev \
    libsdl2-ttf-dev \
    wget \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Install newer CMake (3.28+)
RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-linux-x86_64.sh -O cmake.sh && chmod +x cmake.sh && ./cmake.sh --skip-license --prefix=/usr/local && rm cmake.sh

# Copy project source code
WORKDIR /app
COPY . .

# Build the project
RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build .