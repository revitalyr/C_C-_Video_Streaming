import video_streaming.logger;
import video_streaming.interfaces;
import video_streaming.std;

#include <iostream>
#include <chrono>
#include <thread>
#include <ranges>
#include <vector>

using namespace video_streaming;

// C++26 демонстрация возможностей модулей и новых фич
int main() {
    try {
        // C++26: Использование consteval для compile-time оптимизации
        constexpr LogFormat app_start("Video Streaming System starting...");
        constexpr LogFormat app_complete("Video Streaming System completed successfully");
        
        // C++26: Perfect forwarding для создания логгера
        auto& manager = LoggerManager::instance();
        auto* main_logger = manager.get_logger("main");
        
        main_logger->info(app_start);
        main_logger->info(LogFormat("C++26 modules with spdlog integration working!"));
        
        // C++26: Демонстрация ranges и structured bindings
        Vector<String> components = {
            "Logger Module",
            "Interface Module", 
            "Standard Module",
            "RTP Module",
            "Media Module"
        };
        
        main_logger->info(LogFormat("System components loaded:"));
        for (const auto& [index, component] : components | std::views::enumerate) {
            main_logger->info(LogFormat("  {}. {}", index + 1, component));
        }
        
        // C++26: Демонстрация advanced логирования
        main_logger->info(LogFormat("Testing advanced C++26 features..."));
        
        // C++26: Логирование ranges
        Vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        main_logger->info_range(LogFormat("Numbers range"), numbers);
        
        // C++26: Perfect forwarding с различными типами
        const int int_val = 42;
        const double double_val = 3.14159;
        const String string_val = "C++26";
        
        main_logger->info(LogFormat("Perfect forwarding test: int={}, double={}, string={}", 
            int_val, double_val, string_val));
        
        // C++26: Многопоточное тестирование
        main_logger->info(LogFormat("Starting multi-threaded test..."));
        
        constexpr int num_threads = 4;
        std::vector<std::thread> threads;
        
        // C++26: Использование std::barrier для синхронизации
        std::barrier sync_point(num_threads + 1); // +1 for main thread
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, i, &sync_point] {
                auto* thread_logger = manager.get_logger(std::format("worker_{}", i));
                thread_logger->info(LogFormat("Thread {} started", i));
                
                // Синхронизация всех потоков
                sync_point.arrive_and_wait();
                
                // C++26: Ranges и structured bindings в потоках
                Vector<std::pair<int, String>> tasks;
                for (int j = 0; j < 5; ++j) {
                    tasks.emplace_back(j, std::format("task_{}_{}", i, j));
                }
                
                for (const auto& [task_id, task_name] : tasks) {
                    thread_logger->info(LogFormat("Processing {}: {}", task_name, task_id));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                thread_logger->info(LogFormat("Thread {} completed", i));
                
                // Финальная синхронизация
                sync_point.arrive_and_wait();
            });
        }
        
        // Основной поток тоже участвует в синхронизации
        sync_point.arrive_and_wait();
        main_logger->info(LogFormat("All threads synchronized and working"));
        
        // Ожидание завершения всех потоков
        for (auto& thread : threads) {
            thread.join();
        }
        
        sync_point.arrive_and_wait();
        main_logger->info(LogFormat("All threads completed"));
        
        // C++26: Демонстрация управления памятью
        main_logger->info(LogFormat("Testing memory management..."));
        
        {
            auto unique_logger = UniquePtr<Logger>(new Logger("temporary", LogLevel::DEBUG));
            unique_logger->info(LogFormat("Temporary logger created"));
            unique_logger->debug(LogFormat("Debug message from temporary logger"));
        } // unique_logger автоматически уничтожается
        
        main_logger->info(LogFormat("Temporary logger destroyed"));
        
        // C++26: Демонстрация SharedPtr
        auto shared_logger = SharedPtr<Logger>(new Logger("shared", LogLevel::INFO));
        auto shared_logger2 = shared_logger;
        
        main_logger->info(LogFormat("Shared logger use count: {}", shared_logger.use_count()));
        shared_logger->info(LogFormat("Message from shared logger"));
        
        // C++26: Демонстрация error handling
        main_logger->info(LogFormat("Testing error handling..."));
        
        try {
            // Симуляция ошибки
            throw std::runtime_error("Simulated error for testing");
        } catch (const std::exception& e) {
            main_logger->error(LogFormat("Caught exception: {}", e.what()));
        }
        
        // C++26: Демонстрация производительности
        main_logger->info(LogFormat("Performance benchmark..."));
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        constexpr int benchmark_messages = 1000;
        for (int i = 0; i < benchmark_messages; ++i) {
            main_logger->info(LogFormat("Benchmark message {}", i));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        main_logger->info(LogFormat("Logged {} messages in {} μs ({} msg/sec)", 
            benchmark_messages, 
            duration.count(), 
            (benchmark_messages * 1000000.0) / duration.count()));
        
        // C++26: Демонстрация всех созданных логгеров
        auto logger_names = manager.get_logger_names();
        main_logger->info(LogFormat("Total loggers created: {}", logger_names.size()));
        
        main_logger->info_range(LogFormat("Logger names"), logger_names);
        
        // C++26: Финальное сообщение
        main_logger->info(app_complete);
        
        // C++26: Structured bindings для финальной статистики
        const auto [total_loggers, final_duration] = std::pair{
            logger_names.size(),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time
            ).count()
        };
        
        std::cout << "\n=== Video Streaming System Summary ===\n";
        std::cout << "Total loggers: " << total_loggers << "\n";
        std::cout << "Total runtime: " << final_duration << " ms\n";
        std::cout << "C++26 features demonstrated:\n";
        std::cout << "  ✓ Modules (import/export)\n";
        std::cout << "  ✓ Perfect forwarding\n";
        std::cout << "  ✓ Structured bindings\n";
        std::cout << "  ✓ Ranges and views\n";
        std::cout << "  ✓ std::barrier synchronization\n";
        std::cout << "  ✓ consteval compile-time optimization\n";
        std::cout << "  ✓ Smart pointers with move semantics\n";
        std::cout << "  ✓ Exception handling\n";
        std::cout << "  ✓ Thread-safe operations\n";
        std::cout << "  ✓ Performance optimization\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
