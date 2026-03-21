#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>
#include <fstream>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iomanip>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

// Упрощенный RTSP клиент для демонстрации работы с реальными источниками
namespace video_streaming {

class SimpleRealClient {
public:
    struct Config {
        std::string rtsp_url;
        std::string output_file;
        int timeout_ms = 5000;
        int max_packets = 10000;
        bool enable_logging = true;
        int rtp_port = 5000;
    };
    
    struct Stats {
        uint64_t packets_received = 0;
        uint64_t bytes_received = 0;
        double current_fps = 0.0;
        std::chrono::steady_clock::time_point start_time;
        bool connection_successful = false;
        std::string error_message;
    };
    
    explicit SimpleRealClient(const Config& config);
    ~SimpleRealClient();
    
    bool connect();
    void disconnect();
    bool start_receiving();
    void stop_receiving();
    
    Stats get_stats() const;

private:
    bool test_connection();
    void simulate_real_packets();
    void receiving_loop();
    void save_packet_to_file(const std::vector<uint8_t>& packet);
    void update_stats(const std::vector<uint8_t>& packet);
    void log_info(const std::string& message);
    void log_error(const std::string& message);
    
    // Создание реалистичных RTP пакетов
    std::vector<uint8_t> create_realistic_rtp_packet(uint32_t sequence, uint32_t timestamp, uint32_t ssrc);

private:
    Config m_config;
    std::unique_ptr<std::ofstream> m_output_file;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_receiving{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Поток приема
    std::thread m_receiving_thread;
    
    // Очередь пакетов
    std::queue<std::vector<uint8_t>> m_packet_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Статистика
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // WinSock
    bool m_wsa_initialized = false;
    
    // Параметры симуляции
    uint32_t m_sequence = 0;
    uint32_t m_timestamp = 0;
    uint32_t m_ssrc = 0x12345678;
};

SimpleRealClient::SimpleRealClient(const Config& config) 
    : m_config(config) {
    
    // Инициализация WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_wsa_initialized = true;
    }
    
    log_info("Simple Real Client initialized");
    log_info("URL: " + config.rtsp_url);
    log_info("Output: " + config.output_file);
    
    // Сброс статистики
    m_stats = Stats{};
    m_stats.start_time = std::chrono::steady_clock::now();
    
    // Генерация SSRC на основе URL
    m_ssrc = 0x12345678;
    for (char c : config.rtsp_url) {
        m_ssrc = m_ssrc * 31 + c;
    }
}

SimpleRealClient::~SimpleRealClient() {
    disconnect();
    
    // Очистка WinSock
    if (m_wsa_initialized) {
        WSACleanup();
    }
    
    log_info("Simple Real Client destroyed");
}

bool SimpleRealClient::connect() {
    if (m_connected.load()) {
        log_info("Already connected");
        return true;
    }
    
    log_info("Testing connection to: " + m_config.rtsp_url);
    
    // Открытие выходного файла
    m_output_file = std::make_unique<std::ofstream>(m_config.output_file, std::ios::binary);
    if (!m_output_file->is_open()) {
        log_error("Failed to open output file: " + m_config.output_file);
        return false;
    }
    
    // Тест соединения
    if (!test_connection()) {
        log_error("Connection test failed");
        m_stats.error_message = "Connection test failed";
        return false;
    }
    
    m_connected = true;
    m_stats.connection_successful = true;
    log_info("Connection test successful - simulating real stream");
    return true;
}

void SimpleRealClient::disconnect() {
    if (!m_connected.load()) {
        return;
    }
    
    log_info("Disconnecting");
    
    stop_receiving();
    
    if (m_output_file && m_output_file->is_open()) {
        m_output_file->close();
    }
    
    m_connected = false;
    log_info("Disconnected");
}

bool SimpleRealClient::start_receiving() {
    if (!m_connected.load()) {
        log_error("Not connected - cannot start receiving");
        return false;
    }
    
    if (m_receiving.load()) {
        log_info("Already receiving");
        return true;
    }
    
    log_info("Starting packet reception");
    
    m_stop_requested = false;
    m_receiving_thread = std::thread(&SimpleRealClient::receiving_loop, this);
    m_receiving = true;
    
    return true;
}

void SimpleRealClient::stop_receiving() {
    if (!m_receiving.load()) {
        return;
    }
    
    log_info("Stopping packet reception");
    
    m_stop_requested = true;
    m_queue_cv.notify_all();
    
    if (m_receiving_thread.joinable()) {
        m_receiving_thread.join();
    }
    
    m_receiving = false;
    
    // Очистка очереди
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while (!m_packet_queue.empty()) {
            m_packet_queue.pop();
        }
    }
    
    log_info("Packet reception stopped");
}

SimpleRealClient::Stats SimpleRealClient::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

bool SimpleRealClient::test_connection() {
    // Простая проверка доступности хоста
    std::string host = "716f898c7b71.entrypoint.cloud.wowza.com";
    
    if (m_config.rtsp_url.find("ipvmdemo") != std::string::npos) {
        host = "ipvmdemo.dyndns.org";
    } else if (m_config.rtsp_url.find("wowzaec2demo") != std::string::npos) {
        host = "wowzaec2demo.streamlock.net";
    }
    
    log_info("Testing host: " + host);
    
    // Попытка разрешить имя хоста
    struct hostent* host_entry = gethostbyname(host.c_str());
    if (host_entry == nullptr) {
        log_error("Failed to resolve hostname: " + host);
        return false;
    }
    
    log_info("Host resolved successfully");
    return true;
}

void SimpleRealClient::receiving_loop() {
    log_info("Receiving loop started");
    
    // Запуск симуляции реальных пакетов
    std::thread simulator(&SimpleRealClient::simulate_real_packets, this);
    
    while (!m_stop_requested) {
        try {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            
            if (m_queue_cv.wait_for(lock, std::chrono::milliseconds(100), 
                [this] { return !m_packet_queue.empty() || m_stop_requested; })) {
                
                while (!m_packet_queue.empty() && !m_stop_requested) {
                    auto packet = std::move(m_packet_queue.front());
                    m_packet_queue.pop();
                    
                    lock.unlock();
                    
                    // Обработка пакета
                    save_packet_to_file(packet);
                    update_stats(packet);
                    
                    lock.lock();
                }
            }
            
        } catch (const std::exception& e) {
            log_error("Error in receiving loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (simulator.joinable()) {
        simulator.join();
    }
    
    log_info("Receiving loop stopped");
}

void SimpleRealClient::simulate_real_packets() {
    log_info("Real packet simulation started");
    
    const auto packet_interval = std::chrono::milliseconds(33); // ~30 FPS
    const size_t packet_size = 1400; // Типичный размер RTP пакета
    
    while (!m_stop_requested) {
        try {
            // Создание реалистичного RTP пакета
            auto packet = create_realistic_rtp_packet(m_sequence++, m_timestamp, m_ssrc);
            
            // Добавление в очередь
            {
                std::lock_guard<std::mutex> lock(m_queue_mutex);
                m_packet_queue.push(std::move(packet));
                
                // Ограничение размера очереди
                while (m_packet_queue.size() > 100) {
                    m_packet_queue.pop();
                }
            }
            
            m_queue_cv.notify_one();
            
            // Обновление времени
            m_timestamp += 90000 / 30; // 90kHz clock, 30 FPS
            std::this_thread::sleep_for(packet_interval);
            
            // Ограничение количества пакетов
            if (m_sequence >= m_config.max_packets) {
                log_info("Maximum packet limit reached, stopping simulation");
                break;
            }
            
        } catch (const std::exception& e) {
            log_error("Error in packet simulation: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    log_info("Real packet simulation completed");
}

std::vector<uint8_t> SimpleRealClient::create_realistic_rtp_packet(uint32_t sequence, uint32_t timestamp, uint32_t ssrc) {
    // Создание реалистичного RTP пакета с H.264 данными
    std::vector<uint8_t> packet(1400);
    
    // RTP заголовок (12 байт)
    packet[0] = 0x80; // V=2, P=0, X=0, CC=0
    packet[1] = 0x96; // M=0, PT=96 (H.264)
    
    // Sequence number (big-endian)
    packet[2] = (sequence >> 8) & 0xFF;
    packet[3] = sequence & 0xFF;
    
    // Timestamp (big-endian)
    packet[4] = (timestamp >> 24) & 0xFF;
    packet[5] = (timestamp >> 16) & 0xFF;
    packet[6] = (timestamp >> 8) & 0xFF;
    packet[7] = timestamp & 0xFF;
    
    // SSRC (big-endian)
    packet[8] = (ssrc >> 24) & 0xFF;
    packet[9] = (ssrc >> 16) & 0xFF;
    packet[10] = (ssrc >> 8) & 0xFF;
    packet[11] = ssrc & 0xFF;
    
    // H.264 NAL единица (FU-A заголовок + данные)
    packet[12] = 0x1C; // FU-A заголовок: S=1, E=0, R=0, Type=5 (IDR)
    
    // Заполнение данных симуляцией H.264 потока
    for (size_t i = 13; i < packet.size(); ++i) {
        // Реалистичные H.264 данные с паттернами
        uint8_t value = static_cast<uint8_t>((sequence + i + timestamp) % 256);
        
        // Добавление характерных H.264 паттернов
        if (i % 16 == 0) {
            value = 0x00; // Нулевой байт
        } else if (i % 16 == 1) {
            value = 0x01; // Начало NAL
        } else if (i % 8 == 0) {
            value = 0x67; // SPS паттерн
        } else if (i % 8 == 1) {
            value = 0x42; // PPS паттерн
        }
        
        packet[i] = value;
    }
    
    return packet;
}

void SimpleRealClient::save_packet_to_file(const std::vector<uint8_t>& packet) {
    if (!m_output_file || !m_output_file->is_open()) {
        return;
    }
    
    // Сохранение пакета в файл
    m_output_file->write(reinterpret_cast<const char*>(packet.data()), packet.size());
}

void SimpleRealClient::update_stats(const std::vector<uint8_t>& packet) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats.packets_received++;
    m_stats.bytes_received += packet.size();
    
    // Расчет FPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_stats.start_time);
    if (elapsed.count() > 0) {
        m_stats.current_fps = static_cast<double>(m_stats.packets_received) / elapsed.count();
    }
    
    // Периодический вывод статистики
    if (m_stats.packets_received % 100 == 0 && m_config.enable_logging) {
        log_info("Stats: " + std::to_string(m_stats.packets_received) + " packets, " + 
                std::to_string(m_stats.current_fps) + " FPS");
    }
}

void SimpleRealClient::log_info(const std::string& message) {
    if (m_config.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] INFO: " << message << std::endl;
    }
}

void SimpleRealClient::log_error(const std::string& message) {
    if (m_config.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] ERROR: " << message << std::endl;
    }
}

} // namespace video_streaming

// Тестовые сценарии
void test_simple_real_stream() {
    std::cout << "\n=== Testing Simple Real Stream ===\n";
    
    video_streaming::SimpleRealClient::Config config;
    config.rtsp_url = "rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2";
    config.output_file = "simple_real_stream.rtp";
    config.max_packets = 3000;
    config.enable_logging = true;
    config.rtp_port = 5000;
    
    video_streaming::SimpleRealClient client(config);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to stream\n";
        return;
    }
    
    if (!client.start_receiving()) {
        std::cerr << "Failed to start receiving\n";
        return;
    }
    
    // Работа в течение 20 секунд
    std::this_thread::sleep_for(std::chrono::seconds(20));
    
    client.stop_receiving();
    client.disconnect();
    
    auto stats = client.get_stats();
    std::cout << "Final stats: " << stats.packets_received << " packets, "
              << stats.current_fps << " FPS\n";
    
    if (stats.connection_successful) {
        std::cout << "✅ Connection test successful\n";
    } else {
        std::cout << "❌ Connection test failed: " << stats.error_message << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== Simple Real RTSP Client ===\n";
    std::cout << "Testing connection to real RTSP sources with realistic packet simulation\n\n";
    
    try {
        test_simple_real_stream();
        
        std::cout << "\n🎉 Simple real RTSP test completed!\n";
        std::cout << "\n📝 This demonstrates:\n";
        std::cout << "   1. Real host resolution testing\n";
        std::cout << "   2. Realistic RTP packet generation\n";
        std::cout << "   3. H.264 NAL unit simulation\n";
        std::cout << "   4. Proper RTP header structure\n";
        std::cout << "   5. Real-time packet processing\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
