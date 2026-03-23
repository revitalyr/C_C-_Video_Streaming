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

export module video_streaming.interfaces;

export namespace video_streaming {

// Basic type aliases
using String = std::string;
template<typename T>
using Vector = std::vector<T>;
template<typename K, typename V>
using UnorderedMap = std::unordered_map<K, V>;
template<typename T>
using UniquePtr = std::unique_ptr<T>;
template<typename T>
using SharedPtr = std::shared_ptr<T>;
template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Semantic Aliases (Modern C++ Best Practices)
using Strings = std::vector<std::string>;
using Integers = std::vector<int>;
using ThreadPool = std::vector<std::thread>;
using Barrier = std::barrier<>;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using HighResClock = std::chrono::high_resolution_clock;
using RuntimeError = std::runtime_error;
using Exception = std::exception;

} // namespace video_streaming
