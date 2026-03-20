module;

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

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

} // namespace video_streaming
