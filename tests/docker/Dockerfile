# ============================================================
# Stage 1: builder — compiles application and tests
# ============================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    VCPKG_ROOT=/opt/vcpkg \
    PATH="/opt/vcpkg:/usr/local/bin:${PATH}" \
    CC=clang-20 \
    CXX=clang++-20 \
    CXXFLAGS="-stdlib=libc++" \
    LDFLAGS="-stdlib=libc++ -lc++abi -lpthread"

RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-20 lld-20 libc++-20-dev libc++abi-20-dev \
    clang-tools-20 libpthread-stubs0-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
    ninja-build git curl zip unzip tar pkg-config ca-certificates \
    python3 python3-pip \
    && ln -sf /usr/bin/clang-20 /usr/bin/clang \
    && ln -sf /usr/bin/clang++-20 /usr/bin/clang++ \
    && ln -sf /usr/bin/clang-scan-deps-20 /usr/bin/clang-scan-deps \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.sh \
        -o /tmp/cmake.sh \
    && bash /tmp/cmake.sh --skip-license --prefix=/usr/local \
    && rm /tmp/cmake.sh

# Full clone — without --depth=1, otherwise vcpkg cannot do
# git read-tree for versioned ports (catch2, spdlog)
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics \
    && mkdir -p ${VCPKG_ROOT}/triplets/community

RUN printf 'set(VCPKG_TARGET_ARCHITECTURE x64)\n\
set(VCPKG_CRT_LINKAGE dynamic)\n\
set(VCPKG_LIBRARY_LINKAGE static)\n\
set(VCPKG_CMAKE_SYSTEM_NAME Linux)\n\
set(VCPKG_CXX_FLAGS "-stdlib=libc++")\n\
set(VCPKG_C_FLAGS "")\n\
set(VCPKG_LINKER_FLAGS "-stdlib=libc++ -lc++abi")\n' \
    > ${VCPKG_ROOT}/triplets/community/x64-linux-clang.cmake

# vcpkg.json as a separate layer — cached until manifest changes
COPY vcpkg.json /src/vcpkg.json
RUN ${VCPKG_ROOT}/vcpkg install \
    --triplet=x64-linux-clang \
    --overlay-triplets=${VCPKG_ROOT}/triplets/community \
    --x-manifest-root=/src \
    && rm -rf ${VCPKG_ROOT}/buildtrees ${VCPKG_ROOT}/downloads

# Sources after dependencies — code changes do not invalidate vcpkg layer
COPY . /src
WORKDIR /src/build

RUN cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-linux-clang \
    -DVCPKG_OVERLAY_TRIPLETS=${VCPKG_ROOT}/triplets/community \
    -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
    -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -lc++abi -lpthread" \
    -DCMAKE_THREAD_LIBS_INIT="-lpthread" \
    -DCMAKE_HAVE_THREADS_LIBRARY=1 \
    -DCMAKE_USE_PTHREADS_INIT=1 \
    -DTHREADS_PREFER_PTHREAD_FLAG=OFF \
    -DBUILD_TESTING=ON \
    && ninja -j$(nproc)

# ============================================================
# Stage 2: test-runner — runs tests and network scenarios
# ============================================================
FROM ubuntu:24.04 AS test-runner

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libc++1-20 libc++abi1-20 \
    iproute2 iptables \
    tcpdump tshark \
    ffmpeg \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    python3 python3-pip \
    bc jq curl \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir --break-system-packages pyshark numpy

COPY --from=builder /src/build/video_streaming_app   /app/video_streaming_app
COPY --from=builder /src/build/video_streaming_tests /app/video_streaming_tests

COPY tests/scripts/ /app/tests/scripts/
RUN chmod +x /app/tests/scripts/*.sh

WORKDIR /app
ENTRYPOINT ["/app/tests/scripts/run_all.sh"]

# ============================================================
# Stage 3: runtime — minimal image with application only
# ============================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libc++1-20 libc++abi1-20 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/video_streaming_app /app/video_streaming_app

EXPOSE 5004/udp 5005/udp
ENTRYPOINT ["/app/video_streaming_app"]
