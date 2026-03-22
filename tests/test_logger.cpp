#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

import video_streaming.logger;
import video_streaming.interfaces;

using namespace video_streaming;

// C++26 tests using modules and Catch2 v3
TEST_CASE("Logger Construction and Basic Operations", "[logger]") {
    
    SECTION("Basic Construction") {
        auto& manager = LoggerManager::instance();

        // Ensure logger does not exist initially
        manager.remove_logger("test_logger");

        // Create logger via manager
        REQUIRE(manager.create_logger("test_logger", LogLevel::INFO).is_success());
        
        auto* retrieved = manager.get_logger("test_logger");
        REQUIRE(retrieved != nullptr);
        
        // C++26: Use structured bindings
        const auto [name, level] = std::pair{"test_logger", LogLevel::INFO};
        REQUIRE(retrieved->get_name() == name);
        REQUIRE(retrieved->get_level() == level);
    }
    
    SECTION("Singleton Pattern") {
        auto& manager1 = LoggerManager::instance();
        auto& manager2 = LoggerManager::instance();
        
        // C++26: Reference comparison to the same object
        REQUIRE(&manager1 == &manager2);
        
        // C++26: Thread-safe access check
        std::vector<std::thread> threads;
        constexpr int num_threads = 10;
        
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([i] {
                auto& mgr = LoggerManager::instance();
                auto* logger = mgr.get_logger(std::format("thread_{}", i));
                REQUIRE(logger != nullptr);
                logger->info("Thread {} message", i);
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    SECTION("Log Level Filtering") {
        Logger logger("level_test", LogLevel::WARN);
        
        // C++26: Use perfect forwarding
        logger.info("This should not appear");
        logger.warn("This should appear");
        logger.error("This should appear");
        
        // C++26: Change level at runtime
        logger.set_level(LogLevel::ERROR);
        logger.warn("This should not appear anymore");
        logger.error("This should still appear");
    }
}

TEST_CASE("Advanced C++26 Features", "[logger][c++26]") {
    
    SECTION("Perfect Forwarding") {
        Logger logger("forwarding_test", LogLevel::DEBUG);
        
        // C++26: Perfect forwarding for different types
        const int int_val = 42;
        const double double_val = 3.14159;
        const std::string string_val = "test";
        
        logger.info("Int: {}, Double: {}, String: {}" , int_val, double_val, string_val);
    }
    
    SECTION("Range Logging") {
        Logger logger("range_test", LogLevel::INFO);
        
        // C++26: Logging ranges
        std::vector<int> numbers = {1, 2, 3, 4, 5};
        logger.info_range("Numbers", numbers);
        
        std::vector<std::string> strings = {"hello", "world", "c++26"};
        logger.info_range("Strings", strings);
    }
    
    SECTION("Error Handling") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Check logger removal
        manager.remove_logger("temporary_logger"); // Ensure clean state
        REQUIRE(manager.remove_logger("temporary_logger") == false);
        
        // get_logger now auto-creates the logger
        auto* temp_logger = manager.get_logger("temporary_logger");
        REQUIRE(temp_logger != nullptr);
        
        REQUIRE(manager.remove_logger("temporary_logger") == true);
        
        // Verify removal by checking existence without re-creating
        auto names = manager.get_logger_names();
        REQUIRE(std::ranges::find(names, "temporary_logger") == names.end());
    }
    
    SECTION("UDP Sink Configuration") {
        Logger logger("udp_test", LogLevel::INFO);
        
        // Check UDP sink addition API (functional test without real network)
        REQUIRE_NOTHROW(logger.add_udp_sink("127.0.0.1", 9000));
        
        // Ensure logging to UDP sink does not throw exceptions
        REQUIRE_NOTHROW(logger.info("UDP test message"));
        REQUIRE_NOTHROW(logger.warn("UDP warning message"));
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
                    logger.info("Thread {} message {}", i, j);
                    ++total_messages;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // C++26: Use structured bindings
        const auto [messages, time_us] = std::pair{total_messages.load(), duration.count()};
        
        INFO("Logged " << messages << " messages in " << time_us << " μs");
        INFO("Performance: " << (messages * 1000000.0 / time_us) << " messages/second");
        
        // C++26: Performance requirement
        REQUIRE(time_us < 200000); // Less than 200ms for 8000 messages
    }
    
    SECTION("Logger Manager Stress Test") {
        LoggerManager& manager = LoggerManager::instance();
        
        // C++26: Create multiple loggers
        constexpr int num_loggers = 100;
        
        std::vector<std::string> logger_names;
        for (int i = 0; i < num_loggers; ++i) {
            logger_names.push_back(std::format("stress_test_{}", i));
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // C++20/26: Use ranges for iteration
        for (const auto& name : logger_names) {
            auto* logger = manager.get_logger(name);
            REQUIRE(logger != nullptr);
            logger->info("Created logger: {}", name);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        INFO("Created " << num_loggers << " loggers in " << creation_time.count() << " μs");
        REQUIRE(creation_time.count() < 150000); // Less than 150ms for 100 loggers
        
        // C++26: Check all names
        auto all_names = manager.get_logger_names();
        
        // C++20/26: Check that all names are present
        for (const auto& name : logger_names) {
            REQUIRE(std::ranges::find(all_names, name) != all_names.end());
        }
    }
}

// C++26: Testing using constexpr and compile-time calculations
TEST_CASE("Compile-time Features", "[logger][constexpr]") {
    
    SECTION("Consteval LogFormat") {
        // C++26: format string is checked at compile time
        REQUIRE_NOTHROW([]{ video_streaming::Logger("consteval_test").info("This is a valid format string: {}", 42); }());
    }
    
    SECTION("Template Constraints") {
        // C++26: Check concepts at compile time
        static_assert(Formattable<int>);
        static_assert(Formattable<std::string>);
        static_assert(Formattable<double>);
        static_assert(Formattable<const char*>);
    }
}
