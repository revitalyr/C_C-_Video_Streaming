module;

// Third-party library imports
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

// Standard library imports
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <cstddef>
#include <chrono>
#include <ranges>          // Added for std::ranges::range
#include <source_location> // Добавлено
#include <format>          // Добавлено для удобства форматирования
#include <concepts>
#include <string_view>
#include "../common/types.hpp"
#undef ERROR

export module video_streaming.logger;

// C++23 module imports
import video_streaming.interfaces;
import video_streaming.std;

export namespace video_streaming {

export struct LogFormat { // Exported so tests can use it
    std::string_view fmt;
    std::source_location loc;

    // Конструктор помечен consteval, чтобы захват происходил в момент компиляции в месте вызова
    template <typename T>
    requires std::convertible_to<T, std::string_view>
    constexpr LogFormat(const T& s, std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
};

// Export Formattable concept for tests
export template<typename T>
concept Formattable = requires(T t) {
    { std::format("{}", t) } -> std::convertible_to<std::string>;
};


export enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5
};

// Helper types for logging
export using SessionId = std::string;
export using ErrorMessage = std::string;
export using FileName = std::string;
export using Result = ::Result; // Re-export global Result type

export enum class SecurityEventType {
    LOGIN_ATTEMPT,
    ACCESS_DENIED,
    VIOLATION,
    LOGOUT
};

export struct SecurityEvent {
    SecurityEventType m_type;
    String m_username;
    String m_client_ip;
    String m_details;
};

// Forward declarations
export class Logger;
export class LoggerManager;

export class Logger {
public:
    Logger(const String& name, LogLevel level = LogLevel::INFO);
    Logger(const String& name, LogLevel level, const String& file_path);
    ~Logger();
    
    static std::shared_ptr<Logger> get(const String& name) {
        // Simple factory for now, would typically use LoggerManager
        return std::make_shared<Logger>(name);
    }

    // Modern logging methods with semantic types
    void log_session_event(const SessionId& session_id, const String& event_type, const String& details);
    void log_security_event(const SecurityEvent& event);

    // Generic log method with source location and formatting support
    template<typename... Args>
    void log(LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        if (level < m_level) return;
        log_impl(level, loc, fmt.get(), std::make_format_args(args...));
    }

    // Helper methods for specific levels using source_location
    template<typename... Args>
    void trace(LogFormat target, Args&&... args) {
        log_impl(LogLevel::TRACE, target.loc, target.fmt, std::make_format_args(args...));
    }

    template<typename... Args>
    void debug(LogFormat target, Args&&... args) {
        log_impl(LogLevel::DEBUG, target.loc, target.fmt, std::make_format_args(args...));
    }

    template<typename... Args>
    void info(LogFormat target, Args&&... args) {
       log_impl(LogLevel::INFO, target.loc, target.fmt, std::make_format_args(args...));
    }

    template<typename... Args>
    void warn(LogFormat target, Args&&... args) {
        log_impl(LogLevel::WARN, target.loc, target.fmt, std::make_format_args(args...));
    }

    template<typename... Args>
    void error(LogFormat target, Args&&... args) {
        log_impl(LogLevel::ERROR, target.loc, target.fmt, std::make_format_args(args...));
    }

    template<typename... Args>
    void critical(LogFormat target, Args&&... args) {
       log_impl(LogLevel::CRITICAL, target.loc, target.fmt, std::make_format_args(args...));
    }

    // Range logging
    template<std::ranges::range R>
    void info_range(LogFormat msg, R&& range) {
        std::string formatted = "[";
        for (const auto& item : range) {
            if (formatted.length() > 1) formatted += ", ";
            formatted += std::format("{}", item);
        }
        formatted += "]";
        info(LogFormat("{} {}"), msg.fmt, formatted);
    }
    
    // Additional logging methods used in implementation
    void log_with_fields(LogLevel level, const String& component, const String& message, const std::vector<std::pair<String, String>>& fields);
    void log_authentication_attempt(const String& username, const String& client_ip, bool success, const String& reason);
    void log_session_creation(const String& session_id, const String& username, const String& client_ip, const String& target_service);
    void log_session_termination(const String& session_id, const String& reason);
    void log_access_denied(const String& username, const String& client_ip, const String& resource, const String& reason);
    void log_security_violation(const String& client_ip, const String& violation_type, const String& details);
    void log_security_event(const String& event_type, const String& details);
    void log_performance_metric(const String& operation, double duration_ms, const String& unit);
    void log_connection_stats(size_t active_connections, size_t total_connections);
    void log_throughput(size_t bytes_transferred, const String& direction);

    // Sink management
    void add_file_sink(const String& filename, size_t max_file_size, size_t max_files);
    void add_daily_file_sink(const String& filename, int hour, int minute);
    void enable_console_output(bool enable);
    
    // Static helpers
    static spdlog::level::level_enum convert_log_level(LogLevel level);
    static String format_fields(const std::vector<std::pair<String, String>>& fields);

    // Configuration methods
    void set_level(LogLevel level);
    LogLevel get_level() const noexcept;
    const String& get_name() const noexcept { return m_name; }
    
    // Formatting options
    void format_timestamp(bool enable) noexcept;
    void format_security_event(bool enable) noexcept;
    void set_pattern(const String& pattern);

private:
    void log_impl(LogLevel level, const std::source_location& loc, std::string_view fmt, std::format_args args);
    void initialize_default_sinks(const String& name, LogLevel level, const String& log_file);

    String m_name;
    LogLevel m_level;
    std::shared_ptr<spdlog::logger> m_logger;
    bool m_format_timestamp{true};
    bool m_format_security_event{true};
    String m_format_fields{"default"};
    mutable std::mutex m_mutex;
};

export class LoggerManager {
public:
    static LoggerManager& instance();
    
    // Modern logger management with semantic types
    Result create_logger(const String& name, LogLevel level = LogLevel::INFO);
    bool remove_logger(const String& name);
    Logger* get_logger(const String& name);
    
    // Global configuration
    void set_global_level(LogLevel level);
    void set_output_file(const FileName& filename);
    void set_max_file_size(std::size_t size_bytes);
    void set_max_files(std::size_t count);
    
    // Flush all loggers
        void flush_all();
    
    // Statistics
    std::size_t get_logger_count() const noexcept;
    Strings get_logger_names() const;

private:
    LoggerManager() = default;
    ~LoggerManager() = default;
    
    UnorderedMap<String, std::unique_ptr<Logger>> m_loggers;
    mutable std::mutex m_loggers_mutex;
    LogLevel m_global_level{LogLevel::INFO};
    FileName m_output_file{"video_streaming.log"};
    std::size_t m_max_file_size{10 * 1024 * 1024}; // 10MB
    std::size_t m_max_files{5};
};

// Convenience macros for logging
// Updated macros to utilize std::source_location implicitly via new template methods
#define LOG_TRACE(logger, ...) logger.trace(__VA_ARGS__)
#define LOG_DEBUG(logger, ...) logger.debug(__VA_ARGS__)
#define LOG_INFO(logger, ...) logger.info(__VA_ARGS__)
#define LOG_WARN(logger, ...) logger.warn(__VA_ARGS__)
#define LOG_ERROR(logger, ...) logger.error(__VA_ARGS__)
#define LOG_CRITICAL(logger, ...) logger.critical(__VA_ARGS__)

// Global logger access
#define LOG_SESSION_EVENT(session_id, event_type, details) \
    video_streaming::LoggerManager::instance().get_logger("session")->log_session_event(session_id, event_type, details)
#define LOG_SECURITY_EVENT(event) \
    video_streaming::LoggerManager::instance().get_logger("security")->log_security_event(event)

} // namespace video_streaming
