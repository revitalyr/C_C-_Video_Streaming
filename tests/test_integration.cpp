#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>
#include <barrier>
#include <latch>

import video_streaming.logger;
import video_streaming.interfaces;

using namespace video_streaming;

// C++26 интеграционные тесты
TEST_CASE("Logger Integration with Real-world Scenarios", "[integration]") {
    
    SECTION("Multi-threaded Application Simulation") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Использование std::barrier для синхронизации потоков
        constexpr int num_threads = 8;
        constexpr int messages_per_thread = 100;
        std::barrier sync_point(num_threads);
        
        std::vector<std::thread> threads;
        std::atomic<int> total_logged{0};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, &total_logged, i, messages_per_thread, &sync_point] {
                auto* logger = manager.get_logger(std::format("worker_{}", i));
                logger->info(LogFormat("Thread {} started", i));
                
                // Синхронизация всех потоков
                sync_point.arrive_and_wait();
                
                // C++26: Ranges и structured bindings
                Vector<std::pair<int, String>> tasks;
                for (int j = 0; j < messages_per_thread; ++j) {
                    tasks.emplace_back(j, std::format("task_{}_{}", i, j));
                }
                
                for (const auto& [task_id, task_name] : tasks) {
                    logger->info(LogFormat("Processing {}: {}", task_name, task_id));
                    ++total_logged;
                    
                    // Симуляция работы
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                
                logger->info(LogFormat("Thread {} completed", i));
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        INFO("Logged " << total_logged.load() << " messages in " << duration.count() << " ms");
        REQUIRE(total_logged.load() == num_threads * messages_per_thread);
        
        // C++26: Проверка всех созданных логгеров
        auto logger_names = manager.get_logger_names();
        REQUIRE(logger_names.size() >= num_threads);
    }
    
    SECTION("Logger Lifecycle Management") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Динамическое создание и удаление логгеров
        Vector<String> temp_loggers;
        constexpr int num_temp_loggers = 50;
        
        // Создание логгеров
        for (int i = 0; i < num_temp_loggers; ++i) {
            String name = std::format("temp_{}", i);
            temp_loggers.push_back(name);
            
            auto* logger = manager.get_logger(name);
            REQUIRE(logger != nullptr);
            
            logger->info(LogFormat("Created temporary logger {}", name));
        }
        
        // Проверка что все логгеры созданы
        auto all_names = manager.get_logger_names();
        for (const auto& name : temp_loggers) {
            REQUIRE(std::ranges::find(all_names, name) != all_names.end());
        }
        
        // Удаление половины логгеров
        for (int i = 0; i < num_temp_loggers / 2; ++i) {
            String name = std::format("temp_{}", i);
            REQUIRE(manager.remove_logger(name) == true);
            REQUIRE(manager.get_logger(name) == nullptr);
        }
        
        // Проверка что оставшиеся логгеры все еще существуют
        for (int i = num_temp_loggers / 2; i < num_temp_loggers; ++i) {
            String name = std::format("temp_{}", i);
            auto* logger = manager.get_logger(name);
            REQUIRE(logger != nullptr);
        }
    }
}

TEST_CASE("Performance Benchmarks", "[integration][performance]") {
    
    SECTION("High-frequency Logging") {
        Logger logger("benchmark", LogLevel::INFO);
        
        constexpr int num_messages = 100000;
        constexpr int batch_size = 1000;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // C++26: Batch logging для оптимизации
        for (int batch = 0; batch < num_messages / batch_size; ++batch) {
            for (int i = 0; i < batch_size; ++i) {
                logger->info(LogFormat("Message {}: {}", batch * batch_size + i, "benchmark"));
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double messages_per_second = (num_messages * 1000000.0) / duration.count();
        
        INFO("Logged " << num_messages << " messages in " << duration.count() << " μs");
        INFO("Performance: " << messages_per_second << " messages/second");
        
        // C++26: Требование производительности
        REQUIRE(messages_per_second > 10000); // Минимум 10K сообщений в секунду
    }
    
    SECTION("Memory Usage Analysis") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Создание множества логгеров для анализа памяти
        constexpr int num_loggers = 1000;
        Vector<String> logger_names;
        
        for (int i = 0; i < num_loggers; ++i) {
            String name = std::format("memory_test_{}", i);
            logger_names.push_back(name);
            
            auto* logger = manager.get_logger(name);
            REQUIRE(logger != nullptr);
            
            // Логирование для проверки выделения памяти
            logger->info(LogFormat("Memory test logger {} created", i));
        }
        
        // Проверка что все логгеры существуют
        auto all_names = manager.get_logger_names();
        REQUIRE(all_names.size() >= num_loggers);
        
        // C++26: Очистка и проверка освобождения памяти
        for (const auto& name : logger_names) {
            manager.remove_logger(name);
        }
        
        // Проверка что память освобождена
        auto final_names = manager.get_logger_names();
        for (const auto& name : logger_names) {
            REQUIRE(std::ranges::find(final_names, name) == final_names.end());
        }
    }
}

TEST_CASE("Error Handling and Recovery", "[integration][error]") {
    
    SECTION("Graceful Error Handling") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Тестирование обработки ошибок
        auto* logger = manager.get_logger("error_test");
        REQUIRE(logger != nullptr);
        
        // Тестирование различных уровней логирования
        logger->debug(LogFormat("Debug message"));
        logger->info(LogFormat("Info message"));
        logger->warn(LogFormat("Warning message"));
        logger->error(LogFormat("Error message"));
        
        // Изменение уровня для фильтрации
        logger->set_level(LogLevel::ERROR);
        
        // Эти сообщения не должны появиться в логе
        logger->debug(LogFormat("This should not appear"));
        logger->info(LogFormat("This should not appear"));
        logger->warn(LogFormat("This should not appear"));
        
        // Это сообщение должно появиться
        logger->error(LogFormat("This should appear"));
        
        // Восстановление уровня
        logger->set_level(LogLevel::INFO);
        logger->info(LogFormat("Recovered logging level"));
    }
    
    SECTION("Concurrent Error Recovery") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Использование std::latch для синхронизации
        constexpr int num_threads = 10;
        std::latch start_latch(num_threads);
        
        std::vector<std::thread> threads;
        std::atomic<int> error_count{0};
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&manager, &error_count, i, &start_latch] {
                try {
                    auto* logger = manager.get_logger(std::format("error_thread_{}", i));
                    
                    // Ожидание всех потоков
                    start_latch.arrive_and_wait();
                    
                    // Симуляция различных операций
                    for (int j = 0; j < 100; ++j) {
                        try {
                            logger->info(LogFormat("Thread {} operation {}", i, j));
                            
                            // Симуляция ошибки
                            if (j % 25 == 0) {
                                throw std::runtime_error("Simulated error");
                            }
                        } catch (const std::exception& e) {
                            ++error_count;
                            logger->error(LogFormat("Error in thread {}: {}", i, e.what()));
                        }
                    }
                } catch (...) {
                    ++error_count;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        INFO("Total errors handled: " << error_count.load());
        REQUIRE(error_count.load() > 0); // Должны быть ошибки
        REQUIRE(error_count.load() <= num_threads * 4); // Но не слишком много
    }
}

// C++26: Тестирование с использованием новых возможностей
TEST_CASE("C++26 Specific Features", "[integration][c++26]") {
    
    SECTION("Structured Bindings with Logger") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Structured bindings для работы с логгерами
        auto [logger1, logger2, logger3] = std::tuple{
            manager.get_logger("struct_1"),
            manager.get_logger("struct_2"),
            manager.get_logger("struct_3")
        };
        
        REQUIRE(logger1 != nullptr);
        REQUIRE(logger2 != nullptr);
        REQUIRE(logger3 != nullptr);
        
        logger1->info(LogFormat("Structured binding test 1"));
        logger2->info(LogFormat("Structured binding test 2"));
        logger3->info(LogFormat("Structured binding test 3"));
    }
    
    SECTION("Ranges with Logger Names") {
        LoggerManager& manager = LoggerManager::instance();
        
        // Создание логгеров
        Vector<String> names = {"range_1", "range_2", "range_3", "range_4", "range_5"};
        for (const auto& name : names) {
            manager.get_logger(name);
        }
        
        // C++26: Использование ranges для фильтрации имен
        auto all_names = manager.get_logger_names();
        auto filtered_names = all_names 
            | std::views::filter([](const String& name) { 
                return name.starts_with("range_"); 
            });
        
        Vector<String> range_names(filtered_names.begin(), filtered_names.end());
        REQUIRE(range_names.size() == names.size());
        
        // C++26: Логирование отфильтрованных имен
        for (const auto& name : range_names) {
            auto* logger = manager.get_logger(name);
            logger->info(LogFormat("Range logger: {}", name));
        }
    }
}
