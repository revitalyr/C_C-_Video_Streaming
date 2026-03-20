#include "logging.hpp"
#include <iomanip>
#include <sstream>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
}

void Logger::set_level(LogLevel level) noexcept {
    LockGuard lock(m_mutex);
    m_level = level;
}

void Logger::set_file(const String& filename) {
    LockGuard lock(m_mutex);
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
    
    m_file_stream.open(filename, std::ios::app);
    m_use_file = m_file_stream.is_open();
}

void Logger::log(LogLevel level, StringView tag, StringView message) {
    if (level < m_level) {
        return;
    }
    
    LockGuard lock(m_mutex);
    
    String formatted = format_message(tag, message);
    
    // Always log to console
    std::cout << formatted << std::endl;
    
    // Also log to file if configured
    if (m_use_file && m_file_stream.is_open()) {
        m_file_stream << formatted << std::endl;
        m_file_stream.flush();
    }
}

String Logger::format_message(StringView tag, StringView message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    oss << " [" << tag << "] " << message;
    
    return oss.str();
}

String Logger::level_to_string(LogLevel level) const noexcept {
    switch (level) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error:   return "ERROR";
        default:                return "UNKNOWN";
    }
}
