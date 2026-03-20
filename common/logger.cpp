module video_streaming.logger;

namespace video_streaming {

std::expected<void, LoggerError> Logger::initialize_default_sinks() {
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_logger = std::make_shared<spdlog::logger>(m_name, console_sink);
        
        // C++26: Используем structured bindings для настройки уровня
        const auto level = static_cast<spdlog::level::level_enum>(m_level);
        m_logger->set_level(level);
        
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l] %v");
        
        return std::expected<void, LoggerError>{};
    } catch (...) {
        return std::unexpected(LoggerError::SinkCreationFailed);
    }
}

void Logger::log_internal(LogLevel level, const LogFormat& msg, std::format_args args) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // C++26: Формируем сообщение с местом вызова
        std::string formatted = std::vformat(msg.fmt, args);
        std::string final_message = std::format("[{}:{}] {}", 
            msg.loc.file_name(), 
            msg.loc.line(), 
            formatted);
        
        const auto spd_level = static_cast<spdlog::level::level_enum>(level);
        m_logger->log(spd_level, final_message);
    } catch (const std::exception& e) {
        // C++26: Fallback с structured bindings
        const auto spd_level = static_cast<spdlog::level::level_enum>(level);
        m_logger->log(spd_level, std::format("Logging error: {}", e.what()));
    } catch (...) {
        const auto spd_level = static_cast<spdlog::level::level_enum>(level);
        m_logger->log(spd_level, "Unknown logging error");
    }
}

} // namespace video_streaming
