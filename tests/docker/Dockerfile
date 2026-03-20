# ============================================================
# Stage 1: builder
# Clang 18 + CMake 3.30 + vcpkg — собирает приложение и тесты
# ============================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"
ENV CC=clang-18
ENV CXX=clang++-18

# --- системные зависимости ---
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-18 \
    lld-18 \
    libc++-18-dev \
    libc++abi-18-dev \
    cmake \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    ca-certificates \
    python3 \
    python3-pip \
  && ln -sf /usr/bin/clang-18   /usr/local/bin/clang \
  && ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++ \
  && rm -rf /var/lib/apt/lists/*

# --- CMake 3.30 (apt даёт более старый) ---
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.sh \
      -o /tmp/cmake.sh \
  && bash /tmp/cmake.sh --skip-license --prefix=/usr/local \
  && rm /tmp/cmake.sh

# --- vcpkg ---
RUN git clone --depth=1 https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
  && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# --- кэшируем установку пакетов отдельным слоем ---
COPY vcpkg.json /src/vcpkg.json
WORKDIR /src
RUN ${VCPKG_ROOT}/vcpkg install \
      --triplet=x64-linux \
      --feature-flags=manifests \
  && rm -rf ${VCPKG_ROOT}/buildtrees \
             ${VCPKG_ROOT}/downloads \
             ${VCPKG_ROOT}/packages

# --- исходники ---
COPY . /src
WORKDIR /src/build

RUN cmake .. \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DBUILD_TESTING=ON \
  && ninja -j$(nproc)

# ============================================================
# Stage 2: test-runner
# Минимальный образ только для прогона тестов + сетевых скриптов
# ============================================================
FROM ubuntu:24.04 AS test-runner

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # сетевые инструменты для e2e тестов
    iproute2 \
    iptables \
    tcpdump \
    tshark \
    ffmpeg \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    python3 \
    python3-pip \
    python3-scapy \
    # утилиты
    bc \
    jq \
    curl \
  && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir \
    pyshark \
    numpy \
    matplotlib

# бинарники из builder
COPY --from=builder /src/build/video_streaming_app      /app/video_streaming_app
COPY --from=builder /src/build/video_streaming_tests    /app/video_streaming_tests

# скрипты тестирования
COPY tests/scripts/ /app/tests/scripts/
RUN chmod +x /app/tests/scripts/*.sh

WORKDIR /app

# entrypoint запускает unit-тесты, затем e2e скрипты
ENTRYPOINT ["/app/tests/scripts/run_all.sh"]

# ============================================================
# Stage 3: runtime (только приложение, без тестов)
# ============================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/video_streaming_app /app/video_streaming_app

EXPOSE 5004/udp 5005/udp

ENTRYPOINT ["/app/video_streaming_app"]
