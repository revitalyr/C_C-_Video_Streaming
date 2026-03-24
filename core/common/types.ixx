module;

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

export module video_streaming.common.types;

namespace video_streaming {

// Type aliases following modern C++ conventions
export using u8  = std::uint8_t;
export using u16 = std::uint16_t;
export using u32 = std::uint32_t;
export using u64 = std::uint64_t;

export using i8  = std::int8_t;
export using i16 = std::int16_t;
export using i32 = std::int32_t;
export using i64 = std::int64_t;

// Semantic type aliases
export using Timestamp = u64;
export using SequenceNumber = u16;
export using Ssrc = u32;

// String type aliases
export using String = std::string;
export using Bytes = std::vector<std::uint8_t>;
export using Strings = std::vector<std::string>;

// Time-related aliases
export using Milliseconds = std::chrono::milliseconds;
export using Seconds = std::chrono::seconds;
export using TimePoint = std::chrono::steady_clock::time_point;
export using Duration = std::chrono::duration<u64, std::micro>;

// Network-related aliases
export using Port = u16;
export using IpAddress = std::string;

// Smart pointer aliases
export template<typename T>
using UniquePtr = std::unique_ptr<T>;

export template<typename T>
using SharedPtr = std::shared_ptr<T>;

export template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Thread safety aliases
export template<typename T>
using Atomic = std::atomic<T>;

export using Mutex = std::mutex;
export using LockGuard = std::lock_guard<Mutex>;

// Common constants
export constexpr std::uint32_t RTP_VERSION = 2;
export constexpr std::uint8_t RTP_PAYLOAD_TYPE_H264 = 96;
export constexpr size_t DEFAULT_MTU = 1200;
export constexpr size_t RTP_HEADER_SIZE = 12;
export constexpr Port DEFAULT_RTP_PORT = 5004;
export constexpr Port DEFAULT_RTCP_PORT = 5005;

// Error handling
export enum class ErrorCode {
    None = 0,
    NetworkError,
    EncodingError,
    DecodingError,
    InvalidPacket,
    BufferOverflow,
    Timeout,
    Unknown
};

export struct Result {
    ErrorCode m_error_code{ErrorCode::None};
    String m_error_message;
    
    bool is_success() const noexcept { return m_error_code == ErrorCode::None; }
    bool is_error() const noexcept { return m_error_code != ErrorCode::None; }
    
    static Result success() { return Result{ErrorCode::None, ""}; }
    static Result error(ErrorCode code, const std::string& message) {
        return Result{code, message};
    }
};

} // namespace video_streaming