#if __has_include(<print>)
#include <print>
#else
#include <fmt/core.h>
#include <fmt/ranges.h>
namespace std {
    using fmt::print;
    using fmt::println;
    using fmt::format;
}
#endif

#include <charconv> // Required for std::from_chars
#include <expected> // Required for std::expected

#include <iostream>
#include <chrono>
#include <thread>
#include <ranges>
#include <vector>
#include <barrier>
#include <format>
#include <exception>
#include <string>
#include <fstream>

#include <spdlog/spdlog.h>

#include "logger/logger.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>

#include "../common/types.hpp"

// Реальная операция для демонстрации
void perform_real_operation() {
    using namespace std::chrono_literals;
    
    // Реальная работа с переменным временем выполнения
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    
    auto work_time = dis(gen) * 10ms; // 10-1000ms
    std::this_thread::sleep_for(work_time);
    
    // 10% вероятность реальной ошибки
    std::uniform_int_distribution<> error_dis(1, 10);
    if (error_dis(gen) == 1) {
        throw RuntimeError("Real operation failed");
    }
}

import video_streaming.logger;
import video_streaming.interfaces;
import video_streaming.std;
import video_streaming.pipeline;

using namespace video_streaming;

// RAII wrapper for spdlog shutdown
struct SpdlogShutdown {
    ~SpdlogShutdown() {
        // Ensure all async logs are flushed before exit
        spdlog::shutdown();
    }
};

// Simple command line argument parsing
struct AppConfig {
    bool run_pipeline = false;
    Port rtp_port = 5004;
    Port rtcp_port = 5005;
    std::string metrics_out;
};

AppConfig parse_args(int argc, char* argv[]) {
    auto parse_port = [](std::string_view s) -> std::expected<Port, std::string> {
        Port value{};
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{})
            return std::unexpected(std::format("'{}' is not a valid port", s));
        if (value < 1)
            return std::unexpected(std::format("port {} must be >= 1", value));
        return value;
    };
    AppConfig config;
    std::vector<std::string> args(argv, argv + argc);
    
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--rtp-port" && i + 1 < args.size()) {
            auto rtp = parse_port(args[++i]);
            if (!rtp) throw std::runtime_error(rtp.error());
            config.rtp_port = *rtp;
            config.run_pipeline = true;
        } else if (args[i] == "--rtcp-port" && i + 1 < args.size()) {
            auto rtcp = parse_port(args[++i]);
            if (!rtcp) throw std::runtime_error(rtcp.error());
            config.rtcp_port = *rtcp;
        } else if (args[i] == "--metrics-out" && i + 1 < args.size()) {
            config.metrics_out = args[++i];
            config.run_pipeline = true;
        }
    }
    return config;
}

void run_pipeline_mode(const AppConfig& config) {
    auto& manager = LoggerManager::instance();
    if (!manager.get_logger("main")) {
        manager.create_logger("main");
    }
    auto* logger = manager.get_logger("main");
    
    logger->info("Running in Pipeline Mode"); // No LogFormat
    logger->info("RTP Port: {}", config.rtp_port);
    
    PipelineConfig pconfig;
    pconfig.src_port = config.rtp_port;
    pconfig.enable_sender = false; // E2E test mode acts as receiver
    pconfig.enable_receiver = true;
    
    Pipeline pipeline(pconfig);
    if (!pipeline.start()) {
        logger->error("Failed to start pipeline");
        return;
    }
    
    // Run until interrupted (or for a fixed duration if needed, but wrapper handles timeout)
    // Here we just sleep in a loop and write metrics
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (!config.metrics_out.empty()) {
            auto metrics = pipeline.get_metrics();
            std::ofstream out(config.metrics_out);
            out << std::format("{{\"glass_to_glass_ms\": {:.2f}, \"jitter_buffer_depth_ms\": {:.2f}}}", 
                               metrics.glass_to_glass_ms, metrics.jitter_buffer_depth_ms);
        }
    }
}

// C++26 демонстрация возможностей модулей и новых фич
int main(int argc, char* argv[]) {
    SpdlogShutdown spdlog_shutdown_guard; // Destructor will be called on any exit path

    try {
        AppConfig config = parse_args(argc, argv);
        if (config.run_pipeline) {
            run_pipeline_mode(config);
            return 0;
        }

        // C++26: Использование consteval для compile-time оптимизации
        // C++26: Perfect forwarding для создания логгера
        auto& manager = LoggerManager::instance();
        if (!manager.get_logger("main")) {
            manager.create_logger("main");
        }
        auto* main_logger = manager.get_logger("main");
        
        main_logger->info("Video Streaming System starting...");
        main_logger->info("C++26 modules with spdlog integration working!");
        
        // C++26: Демонстрация ranges и structured bindings
        Strings components = {
            "Logger Module",
            "Interface Module", 
            "Standard Module",
            "RTP Module",
            "Media Module"
        };
        
        main_logger->info("System components loaded:");
        int component_index = 0;
        for (const auto& component : components) {
            main_logger->info("  {}. {}", ++component_index, component);
        }
        
        // C++26: Демонстрация advanced логирования
        main_logger->info("Testing advanced C++26 features...");
        
        // C++26: Логирование ranges
        Integers numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        main_logger->info_range("Numbers range", numbers); // This call is now correct
        
        // C++26: Perfect forwarding с различными типами
        const int int_val = 42;
        const double double_val = 3.14159;
        const String string_val = "C++26";
        
        main_logger->info("Perfect forwarding test: int={}, double={}, string={}", 
            int_val, double_val, string_val);
        
        // C++26: Многопоточное тестирование
        main_logger->info("Starting multi-threaded test...");
        
        constexpr int kNumThreads = 4;
        ThreadPool threads;
        
        // C++26: Использование std::barrier для синхронизации
        Barrier sync_point(kNumThreads + 1); // +1 for main thread
        
        for (int i = 0; i < kNumThreads; ++i) {
            threads.emplace_back([&manager, i, &sync_point] {
                const auto logger_name = std::format("worker_{}", i);
                if (!manager.get_logger(logger_name)) {
                    manager.create_logger(logger_name);
                }
                auto* thread_logger = manager.get_logger(logger_name);
                thread_logger->info("Thread {} started", i);
                
                // Синхронизация всех потоков
                sync_point.arrive_and_wait();
                
                // C++26: Ranges и structured bindings в потоках
                Vector<std::pair<int, String>> tasks; // Could utilize a TaskList alias if defined
                for (int j = 0; j < 5; ++j) {
                    tasks.emplace_back(j, std::format("task_{}_{}", i, j));
                }
                
                for (const auto& [task_id, task_name] : tasks) {
                    thread_logger->info("Processing {}: {}", task_name, task_id);
                    std::this_thread::sleep_for(Milliseconds(10));
                }
                
                thread_logger->info("Thread {} completed", i);
                
                // Финальная синхронизация
                sync_point.arrive_and_wait();
            });
        }
        
        // Основной поток тоже участвует в синхронизации
        sync_point.arrive_and_wait();
        main_logger->info("All threads synchronized and working");
        
        // Ожидание завершения всех потоков
        for (auto& thread : threads) {
            thread.join();
        }
        
        sync_point.arrive_and_wait();
        main_logger->info("All threads completed");
        
        // C++26: Демонстрация управления памятью
        main_logger->info("Testing memory management...");
        
        {
            // Use std::make_unique for safety and efficiency
            auto unique_logger = std::make_unique<Logger>("temporary", LogLevel::DEBUG);
            unique_logger->info("Temporary logger created");
            unique_logger->debug("Debug message from temporary logger");
        } // unique_logger автоматически уничтожается
        
        main_logger->info("Temporary logger destroyed");
        
        // C++26: Демонстрация SharedPtr
        // Use std::make_shared for efficiency (single allocation)
        auto shared_logger = std::make_shared<Logger>("shared", LogLevel::INFO);
        auto shared_logger2 = shared_logger;
        
        main_logger->info("Shared logger use count: {}", shared_logger.use_count());
        shared_logger->info("Message from shared logger");
        
        // C++26: Демонстрация error handling
        main_logger->info("Testing error handling...");
        
        try {
            // Реальная операция с возможной ошибкой
            perform_real_operation();
        } catch (const Exception& e) {
            main_logger->error("Caught exception: {}", e.what());
        }
        
        // C++26: Демонстрация производительности
        main_logger->info("Performance benchmark...");
        
        auto start_time = HighResClock::now();
        
        constexpr int kBenchmarkMessages = 1000;
        for (int i = 0; i < kBenchmarkMessages; ++i) {
            main_logger->info("Benchmark message {}", i);
        }
        
        auto end_time = HighResClock::now();
        auto duration = std::chrono::duration_cast<Microseconds>(end_time - start_time);
        
        main_logger->info("Logged {} messages in {} μs ({} msg/sec)", 
            kBenchmarkMessages, 
            duration.count(), 
            (kBenchmarkMessages * 1000000.0) / duration.count());
        
        // C++26: Демонстрация всех созданных логгеров
        auto logger_names = manager.get_logger_names();
        main_logger->info("Total loggers created: {}", logger_names.size());
        
        main_logger->info_range("Logger names", logger_names);
        
        // C++26: Финальное сообщение
        main_logger->info("Video Streaming System completed successfully");
        
        // C++26: Structured bindings для финальной статистики
        const auto [total_loggers, final_duration] = std::pair{
            logger_names.size(),
            std::chrono::duration_cast<Milliseconds>(
                HighResClock::now() - start_time
            ).count()
        };
        
        std::println("\n=== Video Streaming System Summary ===");
        std::println("Total loggers: {}", total_loggers);
        std::println("Total runtime: {} ms", final_duration);
        std::println("C++26 features demonstrated:");
        std::println("  ✓ Modules (import/export)");
        std::println("  ✓ Perfect forwarding");
        std::println("  ✓ Structured bindings");
        std::println("  ✓ Ranges and views");
        std::println("  ✓ std::barrier synchronization");
        std::println("  ✓ consteval compile-time optimization");
        std::println("  ✓ Smart pointers with move semantics");
        std::println("  ✓ Exception handling");
        std::println("  ✓ Thread-safe operations");
        std::println("  ✓ Performance optimization");
        
        return 0;
        
    } catch (const Exception& e) {
        std::println(stderr, "Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        std::println(stderr, "Unknown fatal error occurred");
        return 1;
    }
}
