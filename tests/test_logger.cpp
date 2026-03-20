#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

import video_streaming.logger;
import video_streaming.interfaces;

using namespace video_streaming;

// C++26 тесты с использованием модулей и Catch2 v3
TEST_CASE("Logger Construction and Basic Operations", "[logger]") {
    
    SECTION("Basic Construction") {
        Logger logger("test_logger", LogLevel::INFO);
        
        // C++26: Используем [[nodiscard]] атрибут
        [[nodiscard]] auto& manager = LoggerManager::instance();
        
        auto* retrieved = manager.get_logger("test_logger");
        REQUIRE(retrieved != nullptr);
        
        // C++26: Используем structured bindings
        const auto [name, level] = std::pair{"test_logger", LogLevel::INFO};
        REQUIRE(retrieved->get_name() == name);
        REQUIRE(retrieved->get_level() == level);
    }
    
    SECTION("Singleton Pattern") {
        auto& manager1 = LoggerManager::instance();
        auto& manager2 = LoggerManager::instance();
        
        // C++26: Сравнение ссылок на один и тот же объект
        REQUIRE(&manager1 == &manager2);
        
        // C++26: Проверка thread-safe доступа
        std::vector<std::thread> threads;
        constexpr int num_threads = 10;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([i] {
                auto& mgr = LoggerManager::instance();
                auto* logger = mgr.get_logger(std::format("thread_{}", i));
                REQUIRE(logger != nullptr);
                logger->info(LogFormat("Thread {} message", i));
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    SECTION("Log Level Filtering") {
        Logger logger("level_test", LogLevel::WARN);
        
        // C++26: Используем perfect forwarding
        logger->info(LogFormat("This should not appear"));
        logger->warn(LogFormat("This should appear"));
        logger->error(LogFormat("This should appear"));
        
        // C++26: Изменение уровня во время выполнения
        logger->set_level(LogLevel::ERROR);
        logger->warn(LogFormat("This should not appear anymore"));
        logger->error(LogFormat("This should still appear"));
    }
}

TEST_CASE("Advanced C++26 Features", "[logger][c++26]") {
    
    SECTION("Perfect Forwarding") {
        Logger logger("forwarding_test", LogLevel::DEBUG);
        
        // C++26: Perfect forwarding для различных типов
        const int int_val = 42;
        const double double_val = 3.14159;
        const std::string string_val = "test";
        
        logger->info(LogFormat("Int: {}, Double: {}, String: {}", int_val, double_val, string_val));
    }
    
    SECTION("Range Logging") {
        Logger logger("range_test", LogLevel::INFO);
        
        // C++26: Логирование ranges
        std::vector<int> numbers = {1, 2, 3, 4, 5};
        logger->info_range(LogFormat("Numbers"), numbers);
        
        std::vector<std::string> strings = {"hello", "world", "c++26"};
        logger->info_range(LogFormat("Strings"), strings);
    }
    
    SECTION("Error Handling") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Проверка удаления логгера
        REQUIRE(manager.remove_logger("temporary_logger") == false);
        
        auto* temp_logger = manager.get_logger("temporary_logger");
        REQUIRE(temp_logger != nullptr);
        REQUIRE(manager.remove_logger("temporary_logger") == true);
        REQUIRE(manager.get_logger("temporary_logger") == nullptr);
    }
}

TEST_CASE("Performance and Thread Safety", "[logger][performance]") {
    
    SECTION("Multi-threaded Logging Performance") {
        Logger logger("performance_test", LogLevel::INFO);
        
        constexpr int num_threads = 8;
        constexpr int messages_per_thread = 1000;
        
        std::vector<std::thread> threads;
        std::atomic<int> total_messages{0};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&logger, &total_messages, i, messages_per_thread] {
                for (int j = 0; j < messages_per_thread; ++j) {
                    logger->info(LogFormat("Thread {} message {}", i, j));
                    ++total_messages;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // C++26: Используем structured bindings
        const auto [messages, time_us] = std::pair{total_messages.load(), duration.count()};
        
        INFO("Logged " << messages << " messages in " << time_us << " μs");
        INFO("Performance: " << (messages * 1000000.0 / time_us) << " messages/second");
        
        // C++26: Требование производительности
        REQUIRE(time_us < 100000); // Менее 100мс для 8000 сообщений
    }
    
    SECTION("Logger Manager Stress Test") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Создание множества логгеров
        constexpr int num_loggers = 100;
        
        std::vector<std::string> logger_names;
        for (int i = 0; i < num_loggers; ++i) {
            logger_names.push_back(std::format("stress_test_{}", i));
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // C++20/26: Используем ranges для итерации
        for (const auto& name : logger_names) {
            auto* logger = manager.get_logger(name);
            REQUIRE(logger != nullptr);
            logger->info(LogFormat("Created logger: {}", name));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        INFO("Created " << num_loggers << " loggers in " << creation_time.count() << " μs");
        REQUIRE(creation_time.count() < 10000); // Менее 10мс для 100 логгеров
        
        // C++26: Проверка всех имен
        auto all_names = manager.get_logger_names();
        REQUIRE(all_names.size() == num_loggers);
        
        // C++20/26: Проверка что все имена присутствуют
        for (const auto& name : logger_names) {
            REQUIRE(std::ranges::find(all_names, name) != all_names.end());
        }
    }
}

// C++26: Тестирование с использованием constexpr и compile-time вычислений
TEST_CASE("Compile-time Features", "[logger][constexpr]") {
    
    SECTION("Consteval LogFormat") {
        // C++26: consteval конструктор компилируется во время компиляции
        constexpr LogFormat fmt("Compile-time message");
        REQUIRE(fmt.fmt == "Compile-time message");
    }
    
    SECTION("Template Constraints") {
        // C++26: Проверка концептов во время компиляции
        static_assert(Formattable<int>);
        static_assert(Formattable<std::string>);
        static_assert(Formattable<double>);
        static_assert(Formattable<const char*>);
    }
}
