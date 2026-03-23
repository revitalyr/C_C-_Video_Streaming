#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

// Type aliases following modern C++ conventions
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Semantic type aliases
using Timestamp = u64;
using SequenceNumber = u16;
using Ssrc = u32;

// String type aliases
using String = std::string;
using Bytes = std::vector<std::uint8_t>;

// Time-related aliases
using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::duration<u64, std::micro>;

// Network-related aliases
using Port = u16;
using IpAddress = std::string;

// Smart pointer aliases
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Thread safety aliases
template<typename T>
using Atomic = std::atomic<T>;

using Mutex = std::mutex;
using LockGuard = std::lock_guard<Mutex>;

// Common constants
constexpr std::uint32_t RTP_VERSION = 2;
constexpr std::uint8_t RTP_PAYLOAD_TYPE_H264 = 96;
constexpr size_t DEFAULT_MTU = 1200;
constexpr size_t RTP_HEADER_SIZE = 12;
constexpr Port DEFAULT_RTP_PORT = 5004;
constexpr Port DEFAULT_RTCP_PORT = 5005;

// Frame-related constants
constexpr int DEFAULT_FPS = 30;
constexpr int DEFAULT_WIDTH = 640;
constexpr int DEFAULT_HEIGHT = 480;
constexpr int DEFAULT_BITRATE = 1000000; // 1 Mbps

// Jitter buffer constants
constexpr size_t DEFAULT_JITTER_BUFFER_SIZE = 128;
constexpr Milliseconds DEFAULT_JITTER_DELAY{100};

// Error handling
enum class ErrorCode {
    None = 0,
    NetworkError,
    EncodingError,
    DecodingError,
    InvalidPacket,
    BufferOverflow,
    Timeout,
    Unknown
};

struct Result {
    ErrorCode m_error_code{ErrorCode::None};
    String m_error_message;
    
    bool is_success() const noexcept { return m_error_code == ErrorCode::None; }
    bool is_error() const noexcept { return m_error_code != ErrorCode::None; }
    
    static Result success() { return Result{ErrorCode::None, ""}; }
    static Result error(ErrorCode code, const std::string& message) {
        return Result{code, message};
    }
};
