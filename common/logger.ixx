module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <source_location>
#include <format>
#include <string_view>
#include <memory>
#include <mutex>
#include <ranges>
#include <concepts>
#include <expected>

export module video_streaming.logger;

import video_streaming.interfaces;
import video_streaming.std;

export namespace video_streaming {

// C++26 concepts для логирования
template<typename T>
concept Formattable = requires(T t) {
    { std::format("{}", t) } -> std::string;
};

template<typename T>
concept Loggable = requires(T t) {
    { t.to_string() } -> std::string;
} || Formattable<T>;

// C++26 структура для автоматического захвата места вызова с улучшенной типизацией
export struct LogFormat {
    std::string_view fmt;
    std::source_location loc;
    
    // C++26 consteval конструктор с улучшенными constraintами
    template <typename T>
    requires std::convertible_to<T, std::string_view>
    consteval LogFormat(const T& s, std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
        
    // C++26 explicit конструктор для string literals
    consteval LogFormat(const char* s, std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
};

export enum class LogLevel : uint8_t {
    TRACE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4, CRITICAL = 5
};

// C++26 expected type для обработки ошибок логирования
export enum class LoggerError {
    NoError,
    InitializationFailed,
    SinkCreationFailed,
    FormatError
};

// C++26 класс Logger с улучшенной функциональностью
export class Logger {
public:
    // C++26 constructor с perfect forwarding
    template <typename Name>
    requires std::convertible_to<Name, std::string>
    explicit Logger(Name&& name, LogLevel level = LogLevel::INFO) 
        : m_name(std::forward<Name>(name)), m_level(level) {
        auto result = initialize_default_sinks();
        if (!result) {
            throw std::runtime_error("Failed to initialize logger: " + result.error());
        }
    }
    
    ~Logger() = default;
    
    // Non-copyable, но movable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) noexcept = default;
    Logger& operator=(Logger&&) noexcept = default;
    
    // C++26 perfect forwarding методы логирования
    template <typename... Args>
    requires (Formattable<Args> && ...)
    void info(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::INFO, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    requires (Formattable<Args> && ...)
    void error(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::ERROR, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    requires (Formattable<Args> && ...)
    void debug(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::DEBUG, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    requires (Formattable<Args> && ...)
    void warn(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::WARN, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    // C++26 метод для логирования ranges
    template <std::ranges::range R>
    requires Formattable<std::ranges::range_value_t<R>>
    void info_range(LogFormat msg, R&& range) {
        std::string formatted;
        for (const auto& item : range) {
            if (!formatted.empty()) formatted += ", ";
            formatted += std::format("{}", item);
        }
        info(LogFormat("{}: {}", msg.fmt, formatted));
    }
    
    // C++26 structured binding поддержка
    void set_level(LogLevel level) noexcept { m_level = level; }
    [[nodiscard]] LogLevel get_level() const noexcept { return m_level; }
    [[nodiscard]] const String& get_name() const noexcept { return m_name; }

private:
    std::expected<void, LoggerError> initialize_default_sinks();
    void log_internal(LogLevel level, const LogFormat& msg, std::format_args args);
    
    // C++26 std::atomic для thread-safe операций
    std::mutex m_mutex;
    std::shared_ptr<spdlog::logger> m_logger;
    String m_name;
    LogLevel m_level;
};

// C++26 Singleton с thread-safe initialization
export class LoggerManager {
public:
    static LoggerManager& instance() noexcept {
        static LoggerManager instance;
        return instance;
    }
    
    // C++26 метод с perfect forwarding
    template <typename Name>
    requires std::convertible_to<Name, std::string>
    Logger* get_logger(Name&& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_loggers.find(std::string_view(name));
        if (it != m_loggers.end()) {
            return it->second.get();
        }
        
        // C++26 structured bindings и emplace
        auto [it, inserted] = m_loggers.emplace(
            std::string(name),
            std::make_unique<Logger>(std::forward<Name>(name))
        );
        
        return it->second.get();
    }
    
    // C++26 метод для удаления логгера
    bool remove_logger(const String& name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_loggers.erase(name) > 0;
    }
    
    // C++26 метод для получения всех логгеров
    [[nodiscard]] std::vector<String> get_logger_names() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<String> names;
        names.reserve(m_loggers.size());
        
        for (const auto& [name, logger] : m_loggers) {
            names.push_back(name);
        }
        
        return names;
    }

private:
    LoggerManager() = default;
    ~LoggerManager() = default;
    
    UnorderedMap<String, std::unique_ptr<Logger>> m_loggers;
    mutable std::mutex m_mutex;
};

} // namespace video_streaming