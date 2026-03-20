module;

#include <filesystem>

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <iomanip>
#include <format>

#undef ERROR // Fix collision with Windows ERROR macro
module video_streaming.logger;

import video_streaming.interfaces;
import video_streaming.std;

// Logger class implementation
namespace video_streaming {

constexpr auto DEFAULT_LOG_FILE = "video_streaming.log";

static String security_event_type_to_string(SecurityEventType type) {
    switch (type) {
        case SecurityEventType::LOGIN_ATTEMPT: return "LOGIN_ATTEMPT";
        case SecurityEventType::ACCESS_DENIED: return "ACCESS_DENIED";
        case SecurityEventType::VIOLATION: return "VIOLATION";
        case SecurityEventType::LOGOUT: return "LOGOUT";
        default: return "UNKNOWN";
    }
}

Logger::Logger(const String& name, LogLevel level) : m_mutex(), m_logger(nullptr) {
    initialize_default_sinks(name, level, DEFAULT_LOG_FILE);
}

Logger::Logger(const String& name, LogLevel level, const String& file_path) : m_mutex(), m_logger(nullptr) {
    initialize_default_sinks(name, level, file_path);
}

Logger::~Logger() = default;

// Private helper methods
void Logger::initialize_default_sinks(const String& name, LogLevel level, const String& log_file) {
    // Create default file sink
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, 1024*1024, 3);
    file_sink->set_level(spdlog::level::info);
    
    m_logger = std::make_shared<spdlog::logger>(name, file_sink);
    m_logger->set_level(convert_log_level(level));
    m_logger->flush_on(spdlog::level::info);
    
    m_name = name;
    m_level = level;
}

String Logger::format_fields(const std::vector<std::pair<String, String>>& fields) {
    std::stringstream ss;
    for (const auto& field : fields) {
        ss << field.first << "=" << field.second;
        if (&field != &fields.back()) {
            ss << ", ";
        }
    }
    return ss.str();
}

spdlog::level::level_enum Logger::convert_log_level(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARN: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        default: return spdlog::level::info;
    }
}

void Logger::log_security_event(const String& event_type, const String& details) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    String formatted_event = format_fields({
        {"event_type", event_type},
        {"details", details}
    });
    
    m_logger->info("SECURITY_EVENT: {}", formatted_event);
}

void Logger::log_session_event(const String& session_id, const String& event_type, const String& details) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_logger->info("SESSION_EVENT: session_id={}, event_type={}, details={}", 
                   session_id, event_type, details);
}

void Logger::log_impl(LogLevel level, const std::source_location& loc, std::string_view fmt, std::format_args args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Format the message using vformat
    String message = std::vformat(fmt, args);

    // Extract filename from path
    std::filesystem::path path(loc.file_name());
    String filename = path.filename().string();
    
    // Format with source location info
    String detailed_msg = std::format("[{}:{}] {}", filename, loc.line(), message);

    m_logger->log(convert_log_level(level), "{}", detailed_msg);
}

void video_streaming::Logger::set_level(video_streaming::LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logger->set_level(video_streaming::Logger::convert_log_level(level));
}

void video_streaming::Logger::set_pattern(const video_streaming::String& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_logger->set_pattern(pattern);
}

void video_streaming::Logger::add_file_sink(const video_streaming::String& filename, std::size_t max_file_size, std::size_t max_files) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, max_file_size, max_files);
    m_logger->sinks().push_back(file_sink);
}

void video_streaming::Logger::add_daily_file_sink(const video_streaming::String& filename, int hour, int minute) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filename, hour, minute, false, static_cast<uint16_t>(5));
    m_logger->sinks().push_back(daily_sink);
}

void video_streaming::Logger::enable_console_output(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (enable) {
        // Check if console sink already exists
        bool has_console_sink = false;
        for (const auto& sink : m_logger->sinks()) {
            if (std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(sink)) {
                has_console_sink = true;
                break;
            }
        }
        
        if (!has_console_sink) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            m_logger->sinks().insert(m_logger->sinks().begin(), console_sink);
        }
    } else {
        // Remove console sinks
        m_logger->sinks().erase(
            std::remove_if(m_logger->sinks().begin(), m_logger->sinks().end(),
                [](const std::shared_ptr<spdlog::sinks::sink>& sink) -> bool {
                    return std::dynamic_pointer_cast<spdlog::sinks::stdout_color_sink_mt>(sink) != nullptr;
                }),
            m_logger->sinks().end());
    }
}

void video_streaming::Logger::log_with_fields(video_streaming::LogLevel level, const video_streaming::String& component, const video_streaming::String& message,
                            const std::vector<std::pair<video_streaming::String, video_streaming::String>>& fields) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    video_streaming::String full_message = message;
    if (!fields_str.empty()) {
        full_message += " | " + fields_str;
    }
    
    m_logger->log(video_streaming::Logger::convert_log_level(level), "[{}] {}", component, full_message);
}

void video_streaming::Logger::log_authentication_attempt(const video_streaming::String& username, const video_streaming::String& client_ip, bool success, const video_streaming::String& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    video_streaming::String status = success ? "SUCCESS" : "FAILED";
    video_streaming::String event_type = "AUTH_ATTEMPT";
    
    video_streaming::Logger::log_security_event(event_type, "username=" + username + ", client_ip=" + client_ip + ", status=" + status + ", reason=" + reason);
}

void video_streaming::Logger::log_session_creation(const video_streaming::String& session_id, const video_streaming::String& username, 
                                 const video_streaming::String& client_ip, const video_streaming::String& target_service) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"session_id", session_id},
        {"username", username},
        {"client_ip", client_ip},
        {"target_service", target_service}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("SESSION_CREATION: {}", fields_str);
}

void video_streaming::Logger::log_session_termination(const video_streaming::String& session_id, const video_streaming::String& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"session_id", session_id},
        {"reason", reason}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("SESSION_TERMINATION: {}", fields_str);
}

void video_streaming::Logger::log_access_denied(const video_streaming::String& username, const video_streaming::String& client_ip, 
                              const video_streaming::String& resource, const video_streaming::String& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"username", username},
        {"client_ip", client_ip},
        {"resource", resource}
    };
    
    if (!reason.empty()) {
        fields.emplace_back("reason", reason);
    }
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->warn("ACCESS_DENIED: {}", fields_str);
}

void video_streaming::Logger::log_security_violation(const video_streaming::String& client_ip, const video_streaming::String& violation_type, const video_streaming::String& details) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"client_ip", client_ip},
        {"violation_type", violation_type}
    };
    
    if (!details.empty()) {
        fields.emplace_back("details", details);
    }
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->error("SECURITY_VIOLATION: {}", fields_str);
}

void video_streaming::Logger::log_performance_metric(const video_streaming::String& operation, double duration_ms, const video_streaming::String& unit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"operation", operation},
        {"duration", std::to_string(duration_ms) + " " + unit}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("PERFORMANCE_METRIC: {}", fields_str);
}

void video_streaming::Logger::log_connection_stats(size_t active_connections, size_t total_connections) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"active_connections", std::to_string(active_connections)},
        {"total_connections", std::to_string(total_connections)}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("CONNECTION_STATS: {}", fields_str);
}

void video_streaming::Logger::log_throughput(size_t bytes_transferred, const video_streaming::String& direction) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"bytes_transferred", std::to_string(bytes_transferred)},
        {"direction", direction}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("THROUGHPUT: {}", fields_str);
}

LogLevel video_streaming::Logger::get_level() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

void video_streaming::Logger::format_timestamp(bool enable) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format_timestamp = enable;
}

void video_streaming::Logger::format_security_event(bool enable) noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format_security_event = enable;
}

void video_streaming::Logger::log_security_event(const SecurityEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::pair<String, String>> fields;
    fields.emplace_back("type", security_event_type_to_string(event.m_type));
    fields.emplace_back("username", event.m_username);
    fields.emplace_back("client_ip", event.m_client_ip);
    fields.emplace_back("details", event.m_details);
    
    String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->warn("SECURITY_EVENT: {}", fields_str);
}

// LoggerManager implementation
LoggerManager& LoggerManager::instance() {
    static LoggerManager instance;
    return instance;
}

Result LoggerManager::create_logger(const String& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(m_loggers_mutex);
    if (m_loggers.find(name) != m_loggers.end()) {
        return Result::error(ErrorCode::Unknown, "Logger already exists");
    }
    m_loggers[name] = std::make_unique<Logger>(name, level);
    return Result::success();
}

bool LoggerManager::remove_logger(const String& name) {
    std::lock_guard<std::mutex> lock(m_loggers_mutex);
    return m_loggers.erase(name) > 0;
}

Logger* LoggerManager::get_logger(const String& name) {
    std::lock_guard<std::mutex> lock(m_loggers_mutex);
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        return it->second.get();
    }
    // Auto-create for convenience in tests/examples, or return nullptr
    // Given the test cases, it seems we might expect auto-creation or nullptr depending on context.
    // But based on `create_logger` existing, explicit creation is preferred. 
    // However, `main.cpp` calls `get_logger("main")` without create. So we auto-create.
    m_loggers[name] = std::make_unique<Logger>(name, m_global_level);
    return m_loggers[name].get();
}

std::size_t LoggerManager::get_logger_count() const noexcept {
    std::lock_guard<std::mutex> lock(m_loggers_mutex);
    return m_loggers.size();
}

Strings LoggerManager::get_logger_names() const {
    std::lock_guard<std::mutex> lock(m_loggers_mutex);
    Strings names;
    for (const auto& pair : m_loggers) {
        names.push_back(pair.first);
    }
    return names;
}

} // namespace video_streaming
