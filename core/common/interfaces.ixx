module;

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <barrier>
#include <chrono>
#include <stdexcept>
#include <exception>
#include <optional>

export module video_streaming.interfaces;

import video_streaming.common.types;
import video_streaming.media.frame;

export namespace video_streaming {

// Basic type aliases are imported from common.types,
// but some local aliases might be needed if not fully covered.
// common.types exports UniquePtr, SharedPtr, WeakPtr, String, etc.
template<typename T> using Vector = std::vector<T>;

// Semantic Aliases (Modern C++ Best Practices)
using Integers = std::vector<int>;
using ThreadPool = std::vector<std::thread>;
using Barrier = std::barrier<>;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using HighResClock = std::chrono::high_resolution_clock;
using RuntimeError = std::runtime_error;
using Exception = std::exception;

// Interface for components that can be started/stopped
class IRunnable {
public:
    virtual ~IRunnable() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
};

// Interface for components that process frames
template<typename T>
class IProcessor {
public:
    virtual ~IProcessor() = default;
    virtual void process(T& data) = 0;
};

// Interface for video frame receivers
class IReceiver : public IRunnable {
public:
    virtual ~IReceiver() = default;
    virtual std::optional<Frame> receive_frame() = 0;
};

} // namespace video_streaming
