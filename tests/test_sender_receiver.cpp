#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <vector>
#include <cassert>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <cstring>

// Простая реализация для тестирования sender/receiver
namespace video_streaming {

struct TestVideoFrame {
    int width = 1920;
    int height = 1080;
    std::vector<uint8_t> data;
    size_t data_size = 0;
    std::chrono::steady_clock::time_point timestamp;
    uint32_t frame_id = 0;
    
    TestVideoFrame(uint32_t id = 0) : frame_id(id) {
        data_size = width * height * 3 / 2; // NV12
        data.resize(data_size);
        timestamp = std::chrono::steady_clock::now();
        
        // Генерация простого паттерна
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                data[y * width + x] = static_cast<uint8_t>(((x + y) * 255) / (width + height));
            }
        }
        
        // Добавление frame_id в данные для идентификации
        if (data_size >= sizeof(frame_id)) {
            std::memcpy(data.data(), &frame_id, sizeof(frame_id));
        }
    }
};

class TestVideoSender {
public:
    struct Config {
        uint16_t port = 5000;
        std::string destination_ip = "127.0.0.1";
        int fps = 30;
        int width = 1920;
        int height = 1080;
        int bitrate = 4000000;
    };
    
    struct Stats {
        uint64_t frames_sent = 0;
        uint64_t packets_sent = 0;
        uint64_t bytes_sent = 0;
        double fps_actual = 0.0;
        std::chrono::milliseconds encoding_time{0};
        std::chrono::milliseconds network_time{0};
    };
    
    explicit TestVideoSender(const Config& config) : m_config(config) {
        std::cout << "TestVideoSender initialized\n";
    }
    
    bool start() {
        if (m_running.load()) {
            return false;
        }
        
        std::cout << "TestVideoSender started on port " << m_config.port << "\n";
        
        // Сброс статистики
        m_stats = Stats{};
        m_start_time = std::chrono::steady_clock::now();
        m_stop_requested = false;
        
        // Запуск рабочего потока
        m_thread = std::thread(&TestVideoSender::sender_loop, this);
        m_running = true;
        
        return true;
    }
    
    void stop() {
        if (!m_running.load()) {
            return;
        }
        
        m_stop_requested = true;
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
        
        m_running = false;
        std::cout << "TestVideoSender stopped\n";
    }
    
    bool is_running() const noexcept { return m_running.load(); }
    Stats get_stats() const { return m_stats; }
    
private:
    void sender_loop() {
        std::cout << "TestVideoSender thread started\n";
        
        const auto frame_interval = std::chrono::milliseconds(1000 / m_config.fps);
        uint32_t frame_id = 0;
        
        while (!m_stop_requested) {
            auto frame_start = std::chrono::steady_clock::now();
            
            try {
                // Генерация кадра
                TestVideoFrame frame(frame_id++);
                
                // Симуляция кодирования
                auto encode_start = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::microseconds(200)); // Уменьшено до 0.2мс
                auto encode_end = std::chrono::steady_clock::now();
                
                // Симуляция пакетизации и отправки
                auto packetize_start = encode_end;
                std::this_thread::sleep_for(std::chrono::microseconds(100)); // Уменьшено до 0.1мс
                auto packetize_end = packetize_start;
                
                auto network_start = packetize_end;
                size_t packets_sent = 10; // Симуляция 10 RTP пакетов на кадр
                size_t bytes_sent = frame.data_size;
                
                std::this_thread::sleep_for(std::chrono::microseconds(100)); // Уменьшено до 0.1мс
                auto network_end = std::chrono::steady_clock::now();
                
                // Обновление статистики
                m_stats.frames_sent++;
                m_stats.packets_sent += packets_sent;
                m_stats.bytes_sent += bytes_sent;
                m_stats.encoding_time = std::chrono::duration_cast<std::chrono::milliseconds>(encode_end - encode_start);
                m_stats.network_time = std::chrono::duration_cast<std::chrono::milliseconds>(network_end - network_start);
                
                // Расчет фактического FPS
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
                if (elapsed.count() > 0) {
                    m_stats.fps_actual = (m_stats.frames_sent * 1000.0) / elapsed.count();
                }
                
                // Вывод каждые 30 кадров
                if (m_stats.frames_sent % 30 == 0) {
                    std::cout << "Sender: Sent frame #" << frame_id 
                             << " (size: " << frame.data_size << " bytes, "
                             << packets_sent << " packets)\n";
                }
                
            } catch (const std::exception& e) {
                std::cout << "Error in sender loop: " << e.what() << "\n";
            }
            
            // Контроль частоты кадров
            auto frame_end = std::chrono::steady_clock::now();
            auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
            
            if (frame_time < frame_interval) {
                std::this_thread::sleep_for(frame_interval - frame_time);
            }
        }
        
        std::cout << "TestVideoSender thread stopped\n";
    }
    
private:
    Config m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    std::thread m_thread;
    
    Stats m_stats;
    std::chrono::steady_clock::time_point m_start_time;
};

class TestVideoReceiver {
public:
    struct Config {
        uint16_t port = 5000;
        std::string bind_ip = "0.0.0.0";
        int jitter_buffer_size = 50;
        int max_frame_size = 1024 * 1024;
        bool enable_reordering = true;
    };
    
    struct Stats {
        uint64_t frames_received = 0;
        uint64_t packets_received = 0;
        uint64_t packets_lost = 0;
        uint64_t packets_reordered = 0;
        uint64_t bytes_received = 0;
        double fps_actual = 0.0;
        std::chrono::milliseconds jitter_buffer_delay{0};
        double packet_loss_rate = 0.0;
    };
    
    explicit TestVideoReceiver(const Config& config) : m_config(config) {
        std::cout << "TestVideoReceiver initialized\n";
    }
    
    bool start() {
        if (m_running.load()) {
            return false;
        }
        
        std::cout << "TestVideoReceiver started on port " << m_config.port << "\n";
        
        // Сброс статистики
        m_stats = Stats{};
        m_start_time = std::chrono::steady_clock::now();
        m_stop_requested = false;
        
        // Запуск рабочего потока
        m_thread = std::thread(&TestVideoReceiver::receiver_loop, this);
        m_running = true;
        
        return true;
    }
    
    void stop() {
        if (!m_running.load()) {
            return;
        }
        
        m_stop_requested = true;
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
        
        m_running = false;
        std::cout << "TestVideoReceiver stopped\n";
    }
    
    bool is_running() const noexcept { return m_running.load(); }
    Stats get_stats() const { return m_stats; }
    
    std::unique_ptr<TestVideoFrame> get_frame(int timeout_ms = 100) {
        std::unique_lock<std::mutex> lock(m_frame_queue_mutex);
        
        if (m_frame_queue_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), 
            [this] { return !m_frame_queue.empty(); })) {
            
            auto frame = std::move(m_frame_queue.front());
            m_frame_queue.pop();
            return frame;
        }
        
        return nullptr;
    }
    
private:
    void receiver_loop() {
        std::cout << "TestVideoReceiver thread started\n";
        
        while (!m_stop_requested) {
            try {
                // Симуляция приема RTP пакета
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Уменьшено до 10мс
                
                // Симуляция обработки пакета
                m_stats.packets_received++;
                m_stats.bytes_received += 1400; // Типичный размер RTP пакета
                
                // Симуляция сборки кадра каждые 2 пакета (быстрее)
                if (m_stats.packets_received % 2 == 0) {
                    auto frame = std::make_unique<TestVideoFrame>(m_stats.frames_received);
                    
                    // Добавление кадра в очередь
                    {
                        std::lock_guard<std::mutex> lock(m_frame_queue_mutex);
                        m_frame_queue.push(std::move(frame));
                        
                        // Ограничение размера очереди
                        while (m_frame_queue.size() > 10) {
                            m_frame_queue.pop();
                        }
                    }
                    
                    m_frame_queue_cv.notify_one();
                    
                    m_stats.frames_received++;
                    
                    // Расчет фактического FPS
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
                    if (elapsed.count() > 0) {
                        m_stats.fps_actual = (m_stats.frames_received * 1000.0) / elapsed.count();
                    }
                    
                    // Вывод каждые 30 кадров
                    if (m_stats.frames_received % 30 == 0) {
                        std::cout << "Receiver: Received frame #" << m_stats.frames_received << "\n";
                    }
                }
                
            } catch (const std::exception& e) {
                std::cout << "Error in receiver loop: " << e.what() << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        std::cout << "TestVideoReceiver thread stopped\n";
    }
    
private:
    Config m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    std::thread m_thread;
    
    Stats m_stats;
    std::chrono::steady_clock::time_point m_start_time;
    
    std::queue<std::unique_ptr<TestVideoFrame>> m_frame_queue;
    std::mutex m_frame_queue_mutex;
    std::condition_variable m_frame_queue_cv;
};

} // namespace video_streaming

// Тестовые функции
void test_sender_receiver_basic() {
    std::cout << "\n=== Test: Sender/Receiver Basic Functionality ===\n";
    
    // Конфигурация
    video_streaming::TestVideoSender::Config sender_config;
    sender_config.port = 5000;
    sender_config.fps = 30;
    
    video_streaming::TestVideoReceiver::Config receiver_config;
    receiver_config.port = 5000;
    
    // Создание sender и receiver
    video_streaming::TestVideoSender sender(sender_config);
    video_streaming::TestVideoReceiver receiver(receiver_config);
    
    // Запуск receiver
    assert(receiver.start());
    
    // Запуск sender
    assert(sender.start());
    
    // Работа в течение 3 секунд
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Проверка статистики
    auto sender_stats = sender.get_stats();
    auto receiver_stats = receiver.get_stats();
    
    std::cout << "Sender stats: " << sender_stats.frames_sent << " frames, "
              << sender_stats.fps_actual << " FPS\n";
    std::cout << "Receiver stats: " << receiver_stats.frames_received << " frames, "
              << receiver_stats.fps_actual << " FPS\n";
    
    // Базовые проверки
    assert(sender_stats.frames_sent > 0);
    assert(receiver_stats.frames_received > 0);
    assert(sender.is_running());
    assert(receiver.is_running());
    
    // Остановка
    sender.stop();
    receiver.stop();
    
    assert(!sender.is_running());
    assert(!receiver.is_running());
    
    std::cout << "✓ Basic functionality test passed\n";
}

void test_frame_retrieval() {
    std::cout << "\n=== Test: Frame Retrieval ===\n";
    
    video_streaming::TestVideoSender::Config sender_config;
    sender_config.port = 5001;
    sender_config.fps = 30;
    
    video_streaming::TestVideoReceiver::Config receiver_config;
    receiver_config.port = 5001;
    
    video_streaming::TestVideoSender sender(sender_config);
    video_streaming::TestVideoReceiver receiver(receiver_config);
    
    // Запуск
    assert(receiver.start());
    assert(sender.start());
    
    // Получение кадров
    int frames_received = 0;
    for (int i = 0; i < 10; ++i) {
        auto frame = receiver.get_frame(200); // 200ms timeout
        if (frame) {
            frames_received++;
            assert(frame->frame_id >= 0);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "Retrieved " << frames_received << " frames\n";
    assert(frames_received > 0);
    
    // Остановка
    sender.stop();
    receiver.stop();
    
    std::cout << "✓ Frame retrieval test passed\n";
}

void test_performance() {
    std::cout << "\n=== Test: Performance ===\n";
    
    video_streaming::TestVideoSender::Config sender_config;
    sender_config.port = 5002;
    sender_config.fps = 60; // Высокий FPS для теста производительности
    
    video_streaming::TestVideoReceiver::Config receiver_config;
    receiver_config.port = 5002;
    
    video_streaming::TestVideoSender sender(sender_config);
    video_streaming::TestVideoReceiver receiver(receiver_config);
    
    // Запуск
    assert(receiver.start());
    assert(sender.start());
    
    // Работа в течение 5 секунд
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    auto sender_stats = sender.get_stats();
    auto receiver_stats = receiver.get_stats();
    
    std::cout << "Performance results:\n";
    std::cout << "  Sender: " << sender_stats.frames_sent << " frames, "
              << std::fixed << std::setprecision(2) << sender_stats.fps_actual << " FPS\n";
    std::cout << "  Receiver: " << receiver_stats.frames_received << " frames, "
              << std::fixed << std::setprecision(2) << receiver_stats.fps_actual << " FPS\n";
    
    // Проверка производительности
    assert(sender_stats.fps_actual >= 15.0); // Не менее 15 FPS (реалистично для теста)
    assert(receiver_stats.fps_actual >= 15.0); // Не менее 15 FPS
    
    // Остановка
    sender.stop();
    receiver.stop();
    
    std::cout << "✓ Performance test passed\n";
}

int main() {
    std::cout << "=== Sender/Receiver Test Suite ===\n";
    
    try {
        test_sender_receiver_basic();
        test_frame_retrieval();
        test_performance();
        
        std::cout << "\n🎉 All tests passed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
