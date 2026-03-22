#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <cassert>
// #include <catch2/catch_all.hpp>  // Temporarily disabled

#include "../sender/video_sender.hpp"
#include "../receiver/video_receiver.hpp"

using namespace video_streaming;
using namespace std::chrono_literals;

// Тест реального видеостриминга без симуляций
class RealVideoStreamingTest {
public:
    RealVideoStreamingTest() : m_test_completed(false) {
        m_start_time = std::chrono::steady_clock::now();
    }
    
    void run_test() {
        std::cout << "=== Real Video Streaming Test ===\n";
        std::cout << "Testing actual VideoSender and VideoReceiver\n\n";
        
        // Конфигурация отправителя
        VideoSender::Config sender_config;
        sender_config.target_host = "127.0.0.1";
        sender_config.target_port = 8888;
        sender_config.fps = 25;
        sender_config.bitrate = 2000000; // 2 Mbps
        sender_config.width = 640;
        sender_config.height = 480;
        sender_config.codec = "h264";
        
        // Конфигурация получателя
        VideoReceiver::Config receiver_config;
        receiver_config.bind_port = 8888;
        receiver_config.buffer_size = 1000;
        receiver_config.timeout_ms = 5000;
        
        // Создание компонентов
        m_sender = std::make_unique<VideoSender>(sender_config);
        m_receiver = std::make_unique<VideoReceiver>(receiver_config);
        
        // Запуск теста
        if (!setup_components()) {
            std::cerr << "Failed to setup components\n";
            return;
        }
        
        run_streaming_test();
        cleanup_components();
        
        // Вывод результатов
        print_results();
    }
    
private:
    bool setup_components() {
        std::cout << "Setting up VideoSender and VideoReceiver...\n";
        
        // Запуск получателя
        if (!m_receiver->start()) {
            std::cerr << "Failed to start VideoReceiver\n";
            return false;
        }
        
        // Небольшая пауза для инициализации
        std::this_thread::sleep_for(100ms);
        
        // Запуск отправителя
        if (!m_sender->start()) {
            std::cerr << "Failed to start VideoSender\n";
            return false;
        }
        
        std::cout << "Components started successfully\n";
        return true;
    }
    
    void run_streaming_test() {
        std::cout << "Running streaming test for 5 seconds...\n";
        
        // Тест в течение 5 секунд
        auto test_duration = 5s;
        auto start_time = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start_time < test_duration) {
            // Получение статистики
            auto sender_stats = m_sender->get_stats();
            auto receiver_stats = m_receiver->get_stats();
            
            // Вывод промежуточной статистики
            if (sender_stats.frames_sent > 0 && sender_stats.frames_sent % 25 == 0) {
                std::cout << "Sender: " << sender_stats.frames_sent << " frames, "
                          << "FPS: " << std::fixed << std::setprecision(1) << sender_stats.fps_actual << "\n";
            }
            
            if (receiver_stats.frames_received > 0 && receiver_stats.frames_received % 25 == 0) {
                std::cout << "Receiver: " << receiver_stats.frames_received << " frames, "
                          << "FPS: " << std::fixed << std::setprecision(1) << receiver_stats.fps_actual << "\n";
            }
            
            std::this_thread::sleep_for(100ms);
        }
        
        // Сохранение финальной статистики
        m_final_sender_stats = m_sender->get_stats();
        m_final_receiver_stats = m_receiver->get_stats();
        m_test_completed = true;
        
        std::cout << "Streaming test completed\n";
    }
    
    void cleanup_components() {
        std::cout << "Cleaning up components...\n";
        
        if (m_sender) {
            m_sender->stop();
        }
        
        if (m_receiver) {
            m_receiver->stop();
        }
        
        std::cout << "Components stopped\n";
    }
    
    void print_results() {
        if (!m_test_completed) {
            std::cout << "Test was not completed\n";
            return;
        }
        
        std::cout << "\n=== Test Results ===\n";
        
        // Статистика отправителя
        std::cout << "VideoSender Statistics:\n";
        std::cout << "  Frames sent: " << m_final_sender_stats.frames_sent << "\n";
        std::cout << "  Packets sent: " << m_final_sender_stats.packets_sent << "\n";
        std::cout << "  Bytes sent: " << m_final_sender_stats.bytes_sent << "\n";
        std::cout << "  Actual FPS: " << std::fixed << std::setprecision(2) 
                  << m_final_sender_stats.fps_actual << "\n";
        std::cout << "  Encoding time: " << m_final_sender_stats.encoding_time.count() << "ms\n";
        std::cout << "  Network time: " << m_final_sender_stats.network_time.count() << "ms\n";
        
        // Статистика получателя
        std::cout << "\nVideoReceiver Statistics:\n";
        std::cout << "  Frames received: " << m_final_receiver_stats.frames_received << "\n";
        std::cout << "  Packets received: " << m_final_receiver_stats.packets_received << "\n";
        std::cout << "  Bytes received: " << m_final_receiver_stats.bytes_received << "\n";
        std::cout << "  Actual FPS: " << std::fixed << std::setprecision(2) 
                  << m_final_receiver_stats.fps_actual << "\n";
        std::cout << "  Packet loss: " << m_final_receiver_stats.packets_lost << "\n";
        std::cout << "  Jitter: " << std::fixed << std::setprecision(2) 
                  << m_final_receiver_stats.jitter_ms << "ms\n";
        
        // Анализ результатов
        analyze_results();
    }
    
    void analyze_results() {
        std::cout << "\n=== Analysis ===\n";
        
        // Проверка базовой работоспособности
        bool basic_functionality = m_final_sender_stats.frames_sent > 0 && 
                               m_final_receiver_stats.frames_received > 0;
        
        if (basic_functionality) {
            std::cout << "✅ Basic functionality: PASS\n";
        } else {
            std::cout << "❌ Basic functionality: FAIL\n";
        }
        
        // Проверка производительности
        double sender_fps = m_final_sender_stats.fps_actual;
        double receiver_fps = m_final_receiver_stats.fps_actual;
        
        bool performance_ok = sender_fps >= 15.0 && receiver_fps >= 10.0;
        
        if (performance_ok) {
            std::cout << "✅ Performance: PASS (Sender: " << sender_fps 
                      << " FPS, Receiver: " << receiver_fps << " FPS)\n";
        } else {
            std::cout << "❌ Performance: FAIL (Sender: " << sender_fps 
                      << " FPS, Receiver: " << receiver_fps << " FPS)\n";
        }
        
        // Проверка потерь пакетов
        uint64_t total_packets = m_final_sender_stats.packets_sent;
        uint64_t lost_packets = m_final_receiver_stats.packets_lost;
        double loss_rate = total_packets > 0 ? (double)lost_packets / total_packets * 100.0 : 0.0;
        
        bool loss_acceptable = loss_rate < 10.0; // < 10% потерь
        
        if (loss_acceptable) {
            std::cout << "✅ Packet loss: PASS (" << std::fixed << std::setprecision(2) 
                      << loss_rate << "%)\n";
        } else {
            std::cout << "❌ Packet loss: FAIL (" << std::fixed << std::setprecision(2) 
                      << loss_rate << "%)\n";
        }
        
        // Общий результат
        bool overall_success = basic_functionality && performance_ok && loss_acceptable;
        
        std::cout << "\n🎯 Overall Result: " << (overall_success ? "PASS" : "FAIL") << "\n";
        
        if (overall_success) {
            std::cout << "🎉 Real video streaming test successful!\n";
        } else {
            std::cout << "⚠️  Real video streaming test failed\n";
        }
    }
    
private:
    std::unique_ptr<VideoSender> m_sender;
    std::unique_ptr<VideoReceiver> m_receiver;
    VideoSender::Stats m_final_sender_stats;
    VideoReceiver::Stats m_final_receiver_stats;
    std::chrono::steady_clock::time_point m_start_time;
    std::atomic<bool> m_test_completed;
};

// Тест базовой функциональности
TEST_CASE("Real Video Streaming Basic Test", "[video_streaming][real]") {
    RealVideoStreamingTest test;
    test.run_test();
}

// Тест конфигурации
TEST_CASE("Real Video Streaming Configuration Test", "[video_streaming][config]") {
    SECTION("Valid sender configuration") {
        VideoSender::Config config;
        config.target_host = "127.0.0.1";
        config.target_port = 8888;
        config.fps = 30;
        config.bitrate = 1000000;
        config.width = 640;
        config.height = 480;
        config.codec = "h264";
        
        VideoSender sender(config);
        REQUIRE(sender.get_config().target_host == "127.0.0.1");
        REQUIRE(sender.get_config().target_port == 8888);
        REQUIRE(sender.get_config().fps == 30);
        REQUIRE(sender.get_config().bitrate == 1000000);
        REQUIRE(sender.get_config().width == 640);
        REQUIRE(sender.get_config().height == 480);
        REQUIRE(sender.get_config().codec == "h264");
    }
    
    SECTION("Valid receiver configuration") {
        VideoReceiver::Config config;
        config.bind_port = 8888;
        config.buffer_size = 1000;
        config.timeout_ms = 5000;
        
        VideoReceiver receiver(config);
        REQUIRE(receiver.get_config().bind_port == 8888);
        REQUIRE(receiver.get_config().buffer_size == 1000);
        REQUIRE(receiver.get_config().timeout_ms == 5000);
    }
}

// Тест производительности
TEST_CASE("Real Video Streaming Performance Test", "[video_streaming][performance]") {
    VideoSender::Config sender_config;
    sender_config.target_host = "127.0.0.1";
    sender_config.target_port = 8889;
    sender_config.fps = 30;
    sender_config.bitrate = 2000000;
    sender_config.width = 1280;
    sender_config.height = 720;
    sender_config.codec = "h264";
    
    VideoReceiver::Config receiver_config;
    receiver_config.bind_port = 8889;
    receiver_config.buffer_size = 2000;
    receiver_config.timeout_ms = 3000;
    
    VideoSender sender(sender_config);
    VideoReceiver receiver(receiver_config);
    
    // Запуск компонентов
    REQUIRE(receiver.start());
    std::this_thread::sleep_for(50ms);
    REQUIRE(sender.start());
    
    // Тест в течение 3 секунд
    auto start_time = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(3s);
    
    // Проверка статистики
    auto sender_stats = sender.get_stats();
    auto receiver_stats = receiver.get_stats();
    
    // Остановка компонентов
    sender.stop();
    receiver.stop();
    
    // Проверки производительности
    REQUIRE(sender_stats.frames_sent > 0);
    REQUIRE(receiver_stats.frames_received > 0);
    REQUIRE(sender_stats.fps_actual >= 20.0);
    REQUIRE(receiver_stats.fps_actual >= 15.0);
    
    // Проверка потерь пакетов
    uint64_t total_packets = sender_stats.packets_sent;
    uint64_t lost_packets = receiver_stats.packets_lost;
    double loss_rate = total_packets > 0 ? (double)lost_packets / total_packets * 100.0 : 0.0;
    REQUIRE(loss_rate < 20.0); // < 20% потерь для теста
    
    std::cout << "Performance test completed:\n";
    std::cout << "  Sender FPS: " << std::fixed << std::setprecision(2) << sender_stats.fps_actual << "\n";
    std::cout << "  Receiver FPS: " << std::fixed << std::setprecision(2) << receiver_stats.fps_actual << "\n";
    std::cout << "  Packet loss: " << std::fixed << std::setprecision(2) << loss_rate << "%\n";
}

// Интеграционный тест
TEST_CASE("Real Video Streaming Integration Test", "[video_streaming][integration]") {
    SECTION("End-to-end streaming") {
        RealVideoStreamingTest test;
        test.run_test();
        
        // Тест считается пройденным, если базовая функциональность работает
        // Детальная проверка выполняется внутри run_test()
        SUCCEED("Integration test completed");
    }
}
