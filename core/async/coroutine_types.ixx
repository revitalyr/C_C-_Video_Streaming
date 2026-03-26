module;

#include <coroutine>
#include <memory>
#include <optional>
#include <iostream>

export module video_streaming.async.coroutine_types;

export namespace video_streaming::async {

// Generic Task wrapper for coroutines
template<typename T>
class Task {
public:
    struct promise_type {
        T value_;
        std::exception_ptr exception_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T value) {
            value_ = std::move(value);
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    std::coroutine_handle<promise_type> coro_;

    explicit Task(std::coroutine_handle<promise_type> h) : coro_(h) {}

    ~Task() {
        if (coro_) {
            coro_.destroy();
        }
    }

    // Move constructor and assignment
    Task(Task&& other) noexcept : coro_(other.coro_) {
        other.coro_ = {};
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (coro_) {
                coro_.destroy();
            }
            coro_ = other.coro_;
            other.coro_ = {};
        }
        return *this;
    }

    // Delete copy constructor and assignment
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Get result (blocking for now)
    T get() {
        if (!coro_) {
            throw std::runtime_error("Task has no coroutine handle");
        }

        // Resume until completion
        while (!coro_.done()) {
            coro_.resume();
        }

        if (coro_.promise().exception_) {
            std::rethrow_exception(coro_.promise().exception_);
        }

        return std::move(coro_.promise().value_);
    }

    // Check if completed
    bool is_ready() const {
        return coro_ && coro_.done();
    }
};

// Specialization for void
template<>
class Task<void> {
public:
    struct promise_type {
        std::exception_ptr exception_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() {}

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    std::coroutine_handle<promise_type> coro_;

    explicit Task(std::coroutine_handle<promise_type> h) : coro_(h) {}

    ~Task() {
        if (coro_) {
            coro_.destroy();
        }
    }

    // Move constructor and assignment
    Task(Task&& other) noexcept : coro_(other.coro_) {
        other.coro_ = {};
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (coro_) {
                coro_.destroy();
            }
            coro_ = other.coro_;
            other.coro_ = {};
        }
        return *this;
    }

    // Delete copy constructor and assignment
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Get result (blocking for now)
    void get() {
        if (!coro_) {
            throw std::runtime_error("Task has no coroutine handle");
        }

        // Resume until completion
        while (!coro_.done()) {
            coro_.resume();
        }

        if (coro_.promise().exception_) {
            std::rethrow_exception(coro_.promise().exception_);
        }
    }

    // Check if completed
    bool is_ready() const {
        return coro_ && coro_.done();
    }
};

} // namespace video_streaming::async
