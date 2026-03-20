#pragma once

#include "types.hpp"
#include <iostream>
#include <fstream>
#include <mutex>

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3
};

class Logger {
public:
    static Logger& instance();
    
    void set_level(LogLevel level) noexcept;
    void set_file(const String& filename);
    
    void log(LogLevel level, StringView tag, StringView message);
    
    template<typename... Args>
    void debug(StringView tag, StringView format, Args&&... args);
    
    template<typename... Args>
    void info(StringView tag, StringView format, Args&&... args);
    
    template<typename... Args>
    void warning(StringView tag, StringView format, Args&&... args);
    
    template<typename... Args>
    void error(StringView tag, StringView format, Args&&... args);

private:
    Logger() = default;
    ~Logger();
    
    String format_message(StringView tag, StringView message);
    String level_to_string(LogLevel level) const noexcept;
    
    LogLevel m_level{LogLevel::Info};
    std::ofstream m_file_stream;
    Mutex m_mutex;
    bool m_use_file{false};
};

// Convenience macros
#define LOG_DEBUG(tag, ...) Logger::instance().debug(tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) Logger::instance().info(tag, __VA_ARGS__)
#define LOG_WARNING(tag, ...) Logger::instance().warning(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::instance().error(tag, __VA_ARGS__)

// Template implementations
template<typename... Args>
void Logger::debug(StringView tag, StringView format, Args&&... args) {
    if (m_level <= LogLevel::Debug) {
        // Simple formatting - in production, consider using fmt library
        log(LogLevel::Debug, tag, format);
    }
}

template<typename... Args>
void Logger::info(StringView tag, StringView format, Args&&... args) {
    if (m_level <= LogLevel::Info) {
        log(LogLevel::Info, tag, format);
    }
}

template<typename... Args>
void Logger::warning(StringView tag, StringView format, Args&&... args) {
    if (m_level <= LogLevel::Warning) {
        log(LogLevel::Warning, tag, format);
    }
}

template<typename... Args>
void Logger::error(StringView tag, StringView format, Args&&... args) {
    if (m_level <= LogLevel::Error) {
        log(LogLevel::Error, tag, format);
    }
}
