module;

#include <source_location>

// Workaround for Clang modules consteval issue with SPDLOG_FMT_STRING
#include <spdlog/common.h>
#ifdef SPDLOG_FMT_STRING
#undef SPDLOG_FMT_STRING
#endif
#define SPDLOG_FMT_STRING(x) (x)

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>
#include <chrono>
#include <iomanip>
#include <format>
#include <iterator>

#include "types.hpp"
#include "network/udp_socket.hpp"

#undef ERROR // Fix collision with Windows ERROR macro
module video_streaming.logger;

import video_streaming.interfaces;
import video_streaming.std;

// Logger class implementation
namespace video_streaming {

// Custom formatter for Hex Thread ID (%H)
class ThreadIdHexFormatter : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        fmt::format_to(std::back_inserter(dest), "{:x}", msg.thread_id);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<ThreadIdHexFormatter>();
    }
};

// Custom formatter for Relative Time since start (%R)
class RelativeTimeFormatter : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        // Capture start time on first use
        static const auto start_time = spdlog::log_clock::now();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time - start_time);
        auto seconds = elapsed.count() / 1000;
        auto milliseconds = elapsed.count() % 1000;
        
        fmt::format_to(std::back_inserter(dest), "+{}.{:03}s", seconds, milliseconds);
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<RelativeTimeFormatter>();
    }
};

// Custom UDP Sink
template<typename Mutex>
class CustomUdpSink : public spdlog::sinks::base_sink<Mutex> {
public:
    CustomUdpSink(const String& ip, Port port) : m_ip(ip), m_port(port) {
        if (m_socket.open()) {
            // Optionally set non-blocking or buffer sizes here
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        // Convert formatted log to Bytes and send
        // Note: formatted.data() might not be null-terminated if treated as raw bytes, 
        // but Bytes constructor handles the range correctly.
        Bytes data(formatted.data(), formatted.data() + formatted.size());
        m_socket.send_to(data, m_ip, m_port);
    }

    void flush_() override {}

private:
    UdpSocket m_socket;
    String m_ip;
    Port m_port;
};

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

Logger::Logger(const String& name, LogLevel level) : m_logger(nullptr) {
    initialize_default_sinks(name, level, DEFAULT_LOG_FILE);
}

Logger::Logger(const String& name, LogLevel level, const String& file_path) : m_logger(nullptr) {
    initialize_default_sinks(name, level, file_path);
}

Logger::~Logger() = default;

// Private helper methods
void Logger::initialize_default_sinks(const String& name, LogLevel level, const String& log_file) {
    // Initialize global thread pool for async logging
    static bool tp_initialized = [] {
        spdlog::init_thread_pool(8192, 1); // Queue size: 8k, 1 backing thread
        return true;
    }();
    (void)tp_initialized;

    // Optimization: Reuse sinks for the same file to avoid opening multiple file descriptors
    // and improve logger creation performance.
    std::shared_ptr<spdlog::sinks::sink> file_sink;
    {
        static std::mutex s_sink_cache_mutex;
        static UnorderedMap<String, std::shared_ptr<spdlog::sinks::sink>> s_sink_cache;
        std::lock_guard<std::mutex> lock(s_sink_cache_mutex);
        
        if (auto it = s_sink_cache.find(log_file); it != s_sink_cache.end()) {
            file_sink = it->second;
        } else {
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file, 1024*1024, 3);
            file_sink->set_level(spdlog::level::info);
            s_sink_cache[log_file] = file_sink;
        }
    }
    
    // Use async_logger to push logs to the thread pool, decoupling formatting from I/O latency
    m_logger = std::make_shared<spdlog::async_logger>(name, file_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    m_logger->set_level(convert_log_level(level));
    set_pattern("[%R] [%H] [%s:%#] %v");
    m_logger->flush_on(spdlog::level::warn);
    
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
    String formatted_event = format_fields({
        {"event_type", event_type},
        {"details", details}
    });
    m_logger->info("SECURITY_EVENT: {}", formatted_event);
}

void Logger::log_session_event(const String& session_id, const String& event_type, const String& details) {
    m_logger->info("SESSION_EVENT: session_id={}, event_type={}, details={}", 
                   session_id, event_type, details);
}

void video_streaming::Logger::set_level(video_streaming::LogLevel level) {
    m_logger->set_level(video_streaming::Logger::convert_log_level(level));
}

void video_streaming::Logger::set_pattern(const video_streaming::String& pattern) {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ThreadIdHexFormatter>('H');
    formatter->add_flag<RelativeTimeFormatter>('R');
    formatter->set_pattern(pattern);
    m_logger->set_formatter(std::move(formatter));
}

void video_streaming::Logger::add_file_sink(const video_streaming::String& filename, std::size_t max_file_size, std::size_t max_files) {
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, max_file_size, max_files);
    m_logger->sinks().push_back(file_sink);
}

void video_streaming::Logger::add_daily_file_sink(const video_streaming::String& filename, int hour, int minute) {
    auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filename, hour, minute, false, static_cast<uint16_t>(5));
    m_logger->sinks().push_back(daily_sink);
}

void video_streaming::Logger::add_udp_sink(const String& ip, Port port) {
    auto udp_sink = std::make_shared<CustomUdpSink<std::mutex>>(ip, port);
    m_logger->sinks().push_back(udp_sink);
}

void video_streaming::Logger::enable_console_output(bool enable) {
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
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    video_streaming::String full_message = message;
    if (!fields_str.empty()) {
        full_message += " | " + fields_str;
    }
    
    m_logger->log(video_streaming::Logger::convert_log_level(level), "[{}] {}", component, full_message);
}

void video_streaming::Logger::log_authentication_attempt(const video_streaming::String& username, const video_streaming::String& client_ip, bool success, const video_streaming::String& reason) {
    video_streaming::String status = success ? "SUCCESS" : "FAILED";
    video_streaming::String event_type = "AUTH_ATTEMPT";
    
    video_streaming::Logger::log_security_event(event_type, "username=" + username + ", client_ip=" + client_ip + ", status=" + status + ", reason=" + reason);
}

void video_streaming::Logger::log_session_creation(const video_streaming::String& session_id, const video_streaming::String& username, 
                                 const video_streaming::String& client_ip, const video_streaming::String& target_service) {
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
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"session_id", session_id},
        {"reason", reason}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("SESSION_TERMINATION: {}", fields_str);
}

void video_streaming::Logger::log_access_denied(const video_streaming::String& username, const video_streaming::String& client_ip, 
                              const video_streaming::String& resource, const video_streaming::String& reason) {
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
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"operation", operation},
        {"duration", std::to_string(duration_ms) + " " + unit}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("PERFORMANCE_METRIC: {}", fields_str);
}

void video_streaming::Logger::log_connection_stats(size_t active_connections, size_t total_connections) {
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"active_connections", std::to_string(active_connections)},
        {"total_connections", std::to_string(total_connections)}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("CONNECTION_STATS: {}", fields_str);
}

void video_streaming::Logger::log_throughput(size_t bytes_transferred, const video_streaming::String& direction) {
    std::vector<std::pair<video_streaming::String, video_streaming::String>> fields = {
        {"bytes_transferred", std::to_string(bytes_transferred)},
        {"direction", direction}
    };
    
    video_streaming::String fields_str = video_streaming::Logger::format_fields(fields);
    m_logger->info("THROUGHPUT: {}", fields_str);
}

LogLevel video_streaming::Logger::get_level() const noexcept {
    return m_level;
}

void video_streaming::Logger::format_timestamp(bool enable) noexcept {
    m_format_timestamp = enable;
}

void video_streaming::Logger::format_security_event(bool enable) noexcept {
    m_format_security_event = enable;
}

void video_streaming::Logger::log_security_event(const SecurityEvent& event) {
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
    // Auto-create logger if it doesn't exist
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
