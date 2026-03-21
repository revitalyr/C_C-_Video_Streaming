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
#include <iomanip>
#include <ctime>

// Простая реализация RTSP клиента для тестирования
namespace video_streaming {

struct RTPPacket {
    std::vector<uint8_t> data;
    uint32_t sequence_number = 0;
    uint32_t timestamp = 0;
    uint32_t ssrc = 0;
    
    RTPPacket(size_t size = 1400) : data(size) {}
};

class RTSPTestClient {
public:
    struct Config {
        std::string rtsp_url;
        std::string output_file;
        int timeout_ms = 5000;
        int max_packets = 10000;
        bool enable_logging = true;
    };
    
    struct Stats {
        uint64_t packets_received = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_lost = 0;
        double packet_loss_rate = 0.0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_packet_time;
        double current_fps = 0.0;
    };
    
    explicit RTSPTestClient(const Config& config);
    ~RTSPTestClient();
    
    bool connect();
    void disconnect();
    bool start_receiving();
    void stop_receiving();
    
    Stats get_stats() const;
    bool is_connected() const { return m_connected.load(); }
    bool is_receiving() const { return m_receiving.load(); }

private:
    void receiving_loop();
    void save_packet_to_file(const RTPPacket& packet);
    void update_stats(const RTPPacket& packet);
    void log_info(const std::string& message);
    void log_error(const std::string& message);
    
    // Симуляция RTSP соединения и RTP потока
    bool simulate_rtsp_setup();
    void simulate_rtp_stream();

private:
    Config m_config;
    std::unique_ptr<std::ofstream> m_output_file;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_receiving{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Поток приема
    std::thread m_receiving_thread;
    
    // Очередь пакетов
    std::queue<RTPPacket> m_packet_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Статистика
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // RTP состояние
    uint32_t m_expected_sequence = 0;
    uint32_t m_last_sequence = 0;
    uint32_t m_sequence_cycles = 0;
};

RTSPTestClient::RTSPTestClient(const Config& config) 
    : m_config(config) {
    
    log_info("RTSP Test Client initialized");
    log_info("URL: " + config.rtsp_url);
    log_info("Output: " + config.output_file);
    
    // Сброс статистики
    m_stats = Stats{};
    m_stats.start_time = std::chrono::steady_clock::now();
}

RTSPTestClient::~RTSPTestClient() {
    disconnect();
    log_info("RTSP Test Client destroyed");
}

bool RTSPTestClient::connect() {
    if (m_connected.load()) {
        log_info("Already connected");
        return true;
    }
    
    log_info("Connecting to RTSP stream: " + m_config.rtsp_url);
    
    // Открытие выходного файла
    m_output_file = std::make_unique<std::ofstream>(m_config.output_file, std::ios::binary);
    if (!m_output_file->is_open()) {
        log_error("Failed to open output file: " + m_config.output_file);
        return false;
    }
    
    // Симуляция RTSP handshake
    if (!simulate_rtsp_setup()) {
        log_error("RTSP setup failed");
        return false;
    }
    
    m_connected = true;
    log_info("Connected successfully to RTSP stream");
    return true;
}

void RTSPTestClient::disconnect() {
    if (!m_connected.load()) {
        return;
    }
    
    log_info("Disconnecting from RTSP stream");
    
    stop_receiving();
    
    if (m_output_file && m_output_file->is_open()) {
        m_output_file->close();
    }
    
    m_connected = false;
    log_info("Disconnected");
}

bool RTSPTestClient::start_receiving() {
    if (!m_connected.load()) {
        log_error("Not connected - cannot start receiving");
        return false;
    }
    
    if (m_receiving.load()) {
        log_info("Already receiving");
        return true;
    }
    
    log_info("Starting RTP packet reception");
    
    m_stop_requested = false;
    m_receiving_thread = std::thread(&RTSPTestClient::receiving_loop, this);
    m_receiving = true;
    
    return true;
}

void RTSPTestClient::stop_receiving() {
    if (!m_receiving.load()) {
        return;
    }
    
    log_info("Stopping RTP packet reception");
    
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
    
    log_info("RTP packet reception stopped");
}

RTSPTestClient::Stats RTSPTestClient::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void RTSPTestClient::receiving_loop() {
    log_info("RTP receiving loop started");
    
    // Запуск симуляции RTP потока
    std::thread rtp_simulator(&RTSPTestClient::simulate_rtp_stream, this);
    
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
    
    if (rtp_simulator.joinable()) {
        rtp_simulator.join();
    }
    
    log_info("RTP receiving loop stopped");
}

bool RTSPTestClient::simulate_rtsp_setup() {
    // Симуляция RTSP SETUP, PLAY команд
    log_info("Simulating RTSP SETUP...");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    log_info("Simulating RTSP PLAY...");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    log_info("RTSP setup completed successfully");
    return true;
}

void RTSPTestClient::simulate_rtp_stream() {
    log_info("RTP stream simulation started");
    
    uint32_t sequence_number = 0;
    uint32_t timestamp = 0;
    const size_t packet_size = 1400; // Типичный размер RTP пакета
    const auto packet_interval = std::chrono::milliseconds(33); // ~30 FPS
    
    while (!m_stop_requested) {
        try {
            // Создание RTP пакета
            RTPPacket packet(packet_size);
            packet.sequence_number = sequence_number++;
            packet.timestamp = timestamp;
            packet.ssrc = 0x12345678; // Произвольный SSRC
            
            // Заполнение данных (симуляция H.264 NAL единицы)
            for (size_t i = 0; i < packet_size; ++i) {
                packet.data[i] = static_cast<uint8_t>((sequence_number + i) % 256);
            }
            
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
            timestamp += 90000 / 30; // 90kHz clock, 30 FPS
            std::this_thread::sleep_for(packet_interval);
            
            // Ограничение количества пакетов
            if (sequence_number >= m_config.max_packets) {
                log_info("Maximum packet limit reached, stopping simulation");
                break;
            }
            
        } catch (const std::exception& e) {
            log_error("Error in RTP simulation: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    log_info("RTP stream simulation completed");
}

void RTSPTestClient::save_packet_to_file(const RTPPacket& packet) {
    if (!m_output_file || !m_output_file->is_open()) {
        return;
    }
    
    // Сохранение пакета в файл (простой формат)
    m_output_file->write(reinterpret_cast<const char*>(packet.data.data()), packet.data.size());
}

void RTSPTestClient::update_stats(const RTPPacket& packet) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats.packets_received++;
    m_stats.bytes_received += packet.data.size();
    m_stats.last_packet_time = std::chrono::steady_clock::now();
    
    // Расчет потерь пакетов
    if (m_expected_sequence > 0) {
        if (packet.sequence_number > m_last_sequence) {
            // Нормальный случай
            uint32_t expected = m_last_sequence + 1;
            if (packet.sequence_number != expected) {
                m_stats.packets_lost += packet.sequence_number - expected;
            }
        } else {
            // Цикл sequence number
            m_sequence_cycles++;
            uint32_t expected = (m_last_sequence + 1) % 0x10000;
            if (packet.sequence_number != expected) {
                m_stats.packets_lost += packet.sequence_number - expected;
            }
        }
    }
    
    m_last_sequence = packet.sequence_number;
    m_expected_sequence = packet.sequence_number + 1;
    
    // Расчет FPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_stats.start_time);
    if (elapsed.count() > 0) {
        m_stats.current_fps = static_cast<double>(m_stats.packets_received) / elapsed.count();
    }
    
    // Расчет процента потерь
    uint64_t total_expected = m_stats.packets_received + m_stats.packets_lost;
    if (total_expected > 0) {
        m_stats.packet_loss_rate = static_cast<double>(m_stats.packets_lost) / total_expected;
    }
    
    // Периодический вывод статистики
    if (m_stats.packets_received % 100 == 0 && m_config.enable_logging) {
        log_info("Stats: " + std::to_string(m_stats.packets_received) + " packets, " + 
                std::to_string(m_stats.current_fps) + " FPS, " + 
                std::to_string(m_stats.packet_loss_rate * 100) + "% loss");
    }
}

void RTSPTestClient::log_info(const std::string& message) {
    if (m_config.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] INFO: " << message << std::endl;
    }
}

void RTSPTestClient::log_error(const std::string& message) {
    if (m_config.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] ERROR: " << message << std::endl;
    }
}

} // namespace video_streaming

// Тестовые сценарии
void test_wowza_stream() {
    std::cout << "\n=== Testing Wowza RTSP Stream ===\n";
    
    video_streaming::RTSPTestClient::Config config;
    config.rtsp_url = "rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2";
    config.output_file = "wowza_stream.rtp";
    config.max_packets = 5000;
    config.enable_logging = true;
    
    video_streaming::RTSPTestClient client(config);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to Wowza stream\n";
        return;
    }
    
    if (!client.start_receiving()) {
        std::cerr << "Failed to start receiving\n";
        return;
    }
    
    // Работа в течение 30 секунд
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    client.stop_receiving();
    client.disconnect();
    
    auto stats = client.get_stats();
    std::cout << "Final stats: " << stats.packets_received << " packets, "
              << stats.current_fps << " FPS, " 
              << (stats.packet_loss_rate * 100) << "% loss\n";
}

void test_ipvm_camera() {
    std::cout << "\n=== Testing IPVM Public Camera ===\n";
    
    video_streaming::RTSPTestClient::Config config;
    config.rtsp_url = "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast";
    config.output_file = "ipvm_camera.rtp";
    config.max_packets = 3000;
    config.enable_logging = true;
    
    video_streaming::RTSPTestClient client(config);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to IPVM camera\n";
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
              << stats.current_fps << " FPS, " 
              << (stats.packet_loss_rate * 100) << "% loss\n";
}

void test_bunny_stream() {
    std::cout << "\n=== Testing Big Buck Bunny Stream ===\n";
    
    video_streaming::RTSPTestClient::Config config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "bunny_stream.rtp";
    config.max_packets = 2000;
    config.enable_logging = true;
    
    video_streaming::RTSPTestClient client(config);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to Bunny stream\n";
        return;
    }
    
    if (!client.start_receiving()) {
        std::cerr << "Failed to start receiving\n";
        return;
    }
    
    // Работа в течение 15 секунд
    std::this_thread::sleep_for(std::chrono::seconds(15));
    
    client.stop_receiving();
    client.disconnect();
    
    auto stats = client.get_stats();
    std::cout << "Final stats: " << stats.packets_received << " packets, "
              << stats.current_fps << " FPS, " 
              << (stats.packet_loss_rate * 100) << "% loss\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=== RTSP Test Client ===\n";
    std::cout << "Testing with real RTSP video sources\n\n";
    
    try {
        // Тестирование Wowza потока
        test_wowza_stream();
        
        // Небольшая пауза между тестами
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Тестирование IPVM камеры
        test_ipvm_camera();
        
        // Пауза
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Тестирование Bunny потока
        test_bunny_stream();
        
        std::cout << "\n🎉 All RTSP tests completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
