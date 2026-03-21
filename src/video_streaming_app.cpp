#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#include "../sender/video_sender.hpp"
#include "../receiver/video_receiver.hpp"

using namespace video_streaming;

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down...\n";
    g_running = false;
}

void print_stats(const VideoSender::Stats& sender_stats, const VideoReceiver::Stats& receiver_stats) {
    std::cout << "\n=== Video Streaming Statistics ===\n";
    std::cout << "Sender:\n";
    std::cout << "  Frames sent: " << sender_stats.frames_sent << "\n";
    std::cout << "  Packets sent: " << sender_stats.packets_sent << "\n";
    std::cout << "  Bytes sent: " << sender_stats.bytes_sent << "\n";
    std::cout << "  Actual FPS: " << std::fixed << std::setprecision(2) << sender_stats.fps_actual << "\n";
    std::cout << "  Encoding time: " << sender_stats.encoding_time.count() << "ms\n";
    std::cout << "  Network time: " << sender_stats.network_time.count() << "ms\n";
    
    std::cout << "Receiver:\n";
    std::cout << "  Frames received: " << receiver_stats.frames_received << "\n";
    std::cout << "  Packets received: " << receiver_stats.packets_received << "\n";
    std::cout << "  Packets lost: " << receiver_stats.packets_lost << "\n";
    std::cout << "  Packet loss rate: " << std::fixed << std::setprecision(2) 
              << (receiver_stats.packet_loss_rate * 100) << "%\n";
    std::cout << "  Actual FPS: " << receiver_stats.fps_actual << "\n";
    std::cout << "  Jitter buffer delay: " << receiver_stats.jitter_buffer_delay.count() << "ms\n";
    std::cout << "================================\n\n";
}

int main(int argc, char* argv[]) {
    // Установка обработчиков сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "=== Video Streaming Test Application ===\n";
    std::cout << "Press Ctrl+C to stop\n\n";
    
    try {
        // Конфигурация sender
        VideoSender::Config sender_config;
        sender_config.port = 5000;
        sender_config.destination_ip = "127.0.0.1";
        sender_config.fps = 30;
        sender_config.width = 1920;
        sender_config.height = 1080;
        sender_config.bitrate = 4000000; // 4 Mbps
        
        // Конфигурация receiver
        VideoReceiver::Config receiver_config;
        receiver_config.port = 5000;
        receiver_config.bind_ip = "0.0.0.0";
        receiver_config.jitter_buffer_size = 50;
        receiver_config.max_frame_size = 1024 * 1024; // 1MB
        receiver_config.enable_reordering = true;
        
        // Создание sender и receiver
        VideoSender sender(sender_config);
        VideoReceiver receiver(receiver_config);
        
        std::cout << "Starting video streaming...\n";
        
        // Запуск receiver (сначала должен быть готов принимать)
        if (!receiver.start()) {
            std::cerr << "Failed to start receiver\n";
            return 1;
        }
        
        // Небольшая задержка для гарантии запуска receiver
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Запуск sender
        if (!sender.start()) {
            std::cerr << "Failed to start sender\n";
            receiver.stop();
            return 1;
        }
        
        std::cout << "Video streaming started successfully!\n";
        
        // Основной цикл приложения
        auto last_stats_time = std::chrono::steady_clock::now();
        const auto stats_interval = std::chrono::seconds(5);
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Периодический вывод статистики
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_time >= stats_interval) {
                auto sender_stats = sender.get_stats();
                auto receiver_stats = receiver.get_stats();
                print_stats(sender_stats, receiver_stats);
                last_stats_time = now;
            }
            
            // Получение и обработка кадров от receiver
            auto frame = receiver.get_frame(10); // 10ms timeout
            if (frame) {
                // Здесь можно обрабатывать принятые кадры
                // Например, отображение, сохранение и т.д.
                static int frame_count = 0;
                frame_count++;
                
                if (frame_count % 30 == 0) { // Каждые 30 кадров
                    std::cout << "Received frame #" << frame_count 
                              << " (size: " << frame->data_size << " bytes)\n";
                }
            }
        }
        
        // Остановка sender и receiver
        std::cout << "Stopping video streaming...\n";
        sender.stop();
        receiver.stop();
        
        // Финальная статистика
        auto final_sender_stats = sender.get_stats();
        auto final_receiver_stats = receiver.get_stats();
        print_stats(final_sender_stats, final_receiver_stats);
        
        std::cout << "Video streaming stopped successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
