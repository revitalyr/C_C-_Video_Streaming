#pragma once

#include "types.hpp"
#include <chrono>

class TimeUtils {
public:
    static Timestamp now() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
    
    static Milliseconds to_milliseconds(Timestamp timestamp) noexcept {
        return Milliseconds(timestamp);
    }
    
    static Timestamp from_milliseconds(Milliseconds ms) noexcept {
        return ms.count();
    }
    
    static Duration elapsed_since(Timestamp start) noexcept {
        auto now_time = std::chrono::steady_clock::now();
        auto start_time = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(start)
        );
        return std::chrono::duration_cast<Duration>(now_time - start_time);
    }
};
