# 🛠️ Advanced Technical Documentation

This document covers the internal architecture, build system details, and advanced C++ usage.

## 🚀 Tech Stack & C++26 Features

This project serves as a testbed for modern C++ standards (C++23/26) applied to low-latency systems.

### Key Features
- **Modules**: Replaces headers for faster compilation (`import video_streaming.logger`).
- **`std::expected`**: Robust error handling without exceptions.
- **`std::print`**: Type-safe, high-performance formatted output.
- **`consteval`**: Compile-time format string validation.
- **Concurrency**: `std::barrier` and lock-free structures for thread synchronization.

## 📝 Logging System

A custom, zero-allocation logging system designed for the hot path of video processing.

```cpp
import video_streaming.logger;

// Zero-allocation during runtime
logger->info(LogFormat("Packet seq={} size={}B", seq, size));
```

## 📺 RTSP Server Details

The `local_rtsp_server` implements a subset of RTSP 1.0 (RFC 2326).

### Supported Methods
- `OPTIONS`: Capability negotiation.
- `DESCRIBE`: SDP generation (H.264 profile-level-id).
- `SETUP`: Transport negotiation (RTP/AVP/TCP).
- `PLAY`: Stream start.
- `TEARDOWN`: Session cleanup.

### Usage
```bash
# Custom port and interface
./build/local_rtsp_server --port 9000 --interface 0.0.0.0
```

## 🧪 Testing (Catch2)

The project uses Catch2 v3 for unit and integration testing.

### Suites
- **[jitter]**: Logic verification for out-of-order packets.
- **[rtp]**: Packetization/Depacketization correctness.
- **[integration]**: Loopback streaming tests.

### Running Tests
```bash
ctest --test-dir build
# or specific tags
./build/video_streaming_tests "[jitter]"
```

## 🔧 Build Configuration

### CMake Options
```cmake
-DBUILD_TESTING=ON       # Enable Catch2 tests
-DCMAKE_BUILD_TYPE=Debug # For symbols and sanitizers
```

### Dependency Management (vcpkg)
Dependencies are managed via `vcpkg`.
- **Linux**: `libavcodec-dev`, `libsdl2-dev` (or via vcpkg).
- **Windows**: Fully via `vcpkg`.

## 🐛 Debugging & Profiling

### Memory Leaks
```bash
valgrind --leak-check=full ./build/sender
```

### Performance Profiling
```bash
perf record -g ./build/sender
perf report
```