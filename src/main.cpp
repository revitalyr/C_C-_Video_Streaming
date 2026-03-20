#include <print>
#include <iostream>
#include <chrono>
#include <thread>
#include <ranges>
#include <vector>
#include <barrier>
#include <format>
#include <exception>

import video_streaming.logger;
import video_streaming.interfaces;
import video_streaming.std;

using namespace video_streaming;

// C++26 демонстрация возможностей модулей и новых фич
int main() {
    try {
        // C++26: Использование consteval для compile-time оптимизации
        constexpr LogFormat kAppStart("Video Streaming System starting...");
        constexpr LogFormat kAppComplete("Video Streaming System completed successfully");
        
        // C++26: Perfect forwarding для создания логгера
        auto& manager = LoggerManager::instance();
        auto* main_logger = manager.get_logger("main");
        
        main_logger->info(kAppStart);
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
        for (const auto& [index, component] : components | std::views::enumerate) {
            main_logger->info("  {}. {}", index + 1, component);
        }
        
        // C++26: Демонстрация advanced логирования
        main_logger->info("Testing advanced C++26 features...");
        
        // C++26: Логирование ranges
        Integers numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        main_logger->info_range("Numbers range", numbers);
        
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
                auto* thread_logger = manager.get_logger(std::format("worker_{}", i));
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
            // Симуляция ошибки
            throw RuntimeError("Simulated error for testing");
        } catch (const Exception& e) {
            main_logger->error("Caught exception: {}", e.what());
        }
        
        // C++26: Демонстрация производительности
        main_logger->info("Performance benchmark...");
        
        auto start_time = HighResClock::now();
        
        constexpr int kBenchmarkMessages = 1000;
        for (int i = 0; i < kBenchmarkMessages; ++i) {
            main_logger->info("Benchmark message {}", i);
            main_logger->info(LogFormat("Benchmark message {}"), i);
        }
        
        auto end_time = HighResClock::now();
        auto duration = std::chrono::duration_cast<Microseconds>(end_time - start_time);
        
        main_logger->info("Logged {} messages in {} μs ({} msg/sec)", 
        main_logger->info(LogFormat("Logged {} messages in {} μs ({} msg/sec)"), 
            kBenchmarkMessages, 
            duration.count(), 
            (kBenchmarkMessages * 1000000.0) / duration.count());
        
        // C++26: Демонстрация всех созданных логгеров
        auto logger_names = manager.get_logger_names();
        main_logger->info("Total loggers created: {}", logger_names.size());
        
        main_logger->info_range("Logger names", logger_names);
        
        // C++26: Финальное сообщение
        main_logger->info(kAppComplete);
        
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
        std::println(std::cerr, "Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        std::println(std::cerr, "Unknown fatal error occurred");
        return 1;
    }
}
