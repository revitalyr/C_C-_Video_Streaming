module;

#include <coroutine>
#include <memory>
#include <optional>
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>

export module video_streaming.async.coroutine_frame_generator;

import video_streaming.media.frame;
import video_streaming.common.types;

export namespace video_streaming::async {

// Coroutine generator for frames
template<typename T>
class Generator {
public:
    struct promise_type {
        T current_value;
        
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        std::suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }
        
        void return_void() {}
        void unhandled_exception() {
            std::rethrow_exception(std::current_exception());
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
private:
    handle_type coro_;
    
public:
    explicit Generator(handle_type coro) : coro_(coro) {}
    
    ~Generator() {
        if (coro_) {
            coro_.destroy();
        }
    }
    
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    Generator(Generator&& other) noexcept : coro_(other.coro_) {
        other.coro_ = {};
    }
    
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (coro_) {
                coro_.destroy();
            }
            coro_ = other.coro_;
            other.coro_ = {};
        }
        return *this;
    }
    
    // Iterator interface
    class iterator {
    private:
        handle_type coro_;
        
    public:
        explicit iterator(handle_type coro) : coro_(coro) {}
        
        iterator& operator++() {
            coro_.resume();
            if (coro_.done()) {
                coro_ = {};
            }
            return *this;
        }
        
        const T& operator*() const {
            return coro_.promise().current_value;
        }
        
        bool operator!=(const iterator& other) const {
            return coro_ != other.coro_;
        }
    };
    
    iterator begin() {
        if (coro_) {
            coro_.resume();
            if (coro_.done()) {
                return iterator{{}};
            }
        }
        return iterator{coro_};
    }
    
    iterator end() {
        return iterator{{}};
    }
    
    // Get next frame (non-iterator interface)
    std::optional<T> next() {
        if (!coro_ || coro_.done()) {
            return std::nullopt;
        }
        
        coro_.resume();
        if (coro_.done()) {
            return std::nullopt;
        }
        
        return coro_.promise().current_value;
    }
};

// Frame generator coroutine
class CoroutineFrameGenerator {
public:
    struct Config {
        int width = 1920;
        int height = 1080;
        int fps = 30;
        std::chrono::milliseconds frame_interval{1000 / 30};
    };
    
private:
    Config config_;
    std::chrono::steady_clock::time_point start_time_;
    
public:
    explicit CoroutineFrameGenerator(const Config& config) 
        : config_(config), start_time_(std::chrono::steady_clock::now()) {}
    
    Generator<std::unique_ptr<Frame>> generate_frames() {
        std::cout << "🎬 Starting coroutine frame generator..." << std::endl;
        
        while (true) {
            auto frame = std::make_unique<Frame>();
            
            // Fill frame metadata
            frame->width = config_.width;
            frame->height = config_.height;
            frame->format = PixelFormat::YUV420P;
            frame->timestamp = static_cast<Timestamp>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time_
                ).count()
            );
            
            // Generate animated gradient content
            const size_t y_size = frame->width * frame->height;
            const size_t uv_size = y_size / 4;
            const size_t total_size = y_size + uv_size * 2;
            
            frame->data.resize(total_size);
            
            // Animated gradient based on time
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
            float phase = (elapsed % 4000) / 4000.0f * 2.0f * 3.14159265359f;
            
            // Create animated gradient
            uint8_t* y_plane = frame->data.data();
            uint8_t* u_plane = y_plane + y_size;
            uint8_t* v_plane = u_plane + uv_size;
            
            for (int y = 0; y < frame->height; ++y) {
                for (int x = 0; x < frame->width; ++x) {
                    // Animated gradient with color transitions
                    float gradient_factor = (float)x / frame->width;
                    float time_factor = (sin(phase + y * 0.01f) + 1.0f) * 0.5f;
                    
                    // Y component: animated gradient
                    uint8_t r = static_cast<uint8_t>((sin(phase) * 0.5f + 0.5f) * 100 + 100);
                    uint8_t g = static_cast<uint8_t>((sin(phase + 2.0f) * 0.5f + 0.5f) * 100 + 100);
                    uint8_t b = static_cast<uint8_t>((sin(phase + 4.0f) * 0.5f + 0.5f) * 100 + 100);
                    
                    // Convert RGB to Y for gradient
                    y_plane[y * frame->width + x] = static_cast<uint8_t>(
                        0.299f * r + 0.587f * g + 0.114f * b
                    );
                }
            }
            
            // UV components: color information for gradient
            for (int i = 0; i < frame->height; ++i) {
                for (int j = 0; j < frame->width / 2; ++j) {
                    int idx = i * frame->width / 2 + j;
                    float gradient_factor = (float)j / (frame->width / 2);
                    
                    // Animated UV components
                    u_plane[idx] = static_cast<uint8_t>((sin(phase + 2.0f) * 0.5f + 0.5f) * 50 + 128);
                    v_plane[idx] = static_cast<uint8_t>((sin(phase + 4.0f) * 0.5f + 0.5f) * 50 + 128);
                }
            }
            
            std::cout << "🎨 Coroutine generated frame: " << frame->width << "x" << frame->height 
                      << " size: " << frame->data.size() << " bytes" << std::endl;
            
            // Yield the frame
            co_yield std::move(frame);
            
            // Wait for next frame interval
            std::this_thread::sleep_for(config_.frame_interval);
        }
    }
};

} // namespace video_streaming::async
