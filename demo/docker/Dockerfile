# Stage 1: Build environment (cached dependencies)
FROM ubuntu:24.04 AS build-env

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    clang-18 \
    gcc-14 \
    g++-14 \
    git \
    pkg-config \
    libavcodec-dev \
    libavdevice-dev \
    libavfilter-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libsdl2-dev \
    libsdl2-ttf-dev \
    wget \
    ffmpeg \
    libspdlog-dev \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

# Install newer CMake (3.28+)
RUN wget https://github.com/Kitware/CMake/releases/download/v3.30.0/cmake-3.30.0-linux-x86_64.sh -O cmake.sh && chmod +x cmake.sh && ./cmake.sh --skip-license --prefix=/usr/local && rm cmake.sh

# Use GCC for better pthread support
ENV CC=gcc-14
ENV CXX=g++-14

# Stage 2: Build the project
FROM build-env AS builder
WORKDIR /app
COPY . .

# Build the project
RUN mkdir build && cd build && \
    cmake -G Ninja .. && \
    cmake --build .

# Stage 3: Runtime environment
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libspdlog-dev \
    libsdl2-2.0-0 \
    libsdl2-ttf-2.0-0 \
    libstdc++-14-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/coroutine_visual_demo .

# Expose network port
EXPOSE 5000

# Run the coroutine visual demo
CMD ["./coroutine_visual_demo", "--width", "640", "--height", "480", "--fps", "30"]