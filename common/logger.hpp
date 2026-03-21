#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <source_location>
#include <format>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace video_streaming {

enum class LogLevel : uint8_t {
    TRACE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4, CRITICAL = 5
};

struct LogFormat {
    std::string fmt;
    std::source_location loc;
    
    template <typename T>
    requires std::convertible_to<T, std::string>
    consteval LogFormat(const T& s, std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
        
    consteval LogFormat(const char* s, std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
};

class Logger {
public:
    explicit Logger(const std::string& name, LogLevel level = LogLevel::INFO);
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) noexcept;
    Logger& operator=(Logger&&) noexcept;
    
    template <typename... Args>
    requires (std::is_arithmetic_v<Args> || ...)
    void info(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::INFO, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    requires (std::is_arithmetic_v<Args> || ...)
    void error(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::ERROR, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    requires (std::is_arithmetic_v<Args> || ...)
    void debug(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::DEBUG, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    template <typename... Args>
    requires (std::is_arithmetic_v<Args> || ...)
    void warn(LogFormat msg, Args&&... args) {
        log_internal(LogLevel::WARN, msg, std::make_format_args(std::forward<Args>(args)...));
    }
    
    void set_level(LogLevel level) noexcept;
    LogLevel get_level() const noexcept;
    const std::string& get_name() const noexcept;

private:
    bool initialize_default_sinks();
    void log_internal(LogLevel level, const LogFormat& msg, std::format_args args);
    
    std::mutex m_mutex;
    std::shared_ptr<spdlog::logger> m_logger;
    std::string m_name;
    LogLevel m_level;
};

class LoggerManager {
public:
    static LoggerManager& instance() noexcept;
    
    Logger* get_logger(const std::string& name);
    bool remove_logger(const std::string& name);
    std::vector<std::string> get_logger_names() const;

private:
    LoggerManager() = default;
    ~LoggerManager() = default;
    
    std::unordered_map<std::string, std::unique_ptr<Logger>> m_loggers;
    mutable std::mutex m_mutex;
};

} // namespace video_streaming
