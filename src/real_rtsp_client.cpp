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

// Настоящий RTSP клиент для подключения к реальным источникам
namespace video_streaming {

class RealRTSPClient {
public:
    struct Config {
        std::string rtsp_url;
        std::string output_file;
        int timeout_ms = 5000;
        int max_packets = 10000;
        bool enable_logging = true;
        int rtp_port = 5000; // Порт для RTP пакетов
    };
    
    struct Stats {
        uint64_t packets_received = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_lost = 0;
        double packet_loss_rate = 0.0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_packet_time;
        double current_fps = 0.0;
        uint32_t last_sequence = 0;
        uint32_t ssrc = 0;
    };
    
    explicit RealRTSPClient(const Config& config);
    ~RealRTSPClient();
    
    bool connect();
    void disconnect();
    bool start_receiving();
    void stop_receiving();
    
    Stats get_stats() const;
    bool is_connected() const { return m_connected.load(); }
    bool is_receiving() const { return m_receiving.load(); }

private:
    bool setup_rtsp_connection();
    bool send_rtsp_request(const std::string& request);
    std::string receive_rtsp_response();
    bool parse_rtsp_response(const std::string& response);
    bool setup_rtp_socket();
    void receiving_loop();
    void save_packet_to_file(const std::vector<uint8_t>& packet);
    void update_stats(const std::vector<uint8_t>& packet);
    void log_info(const std::string& message);
    void log_error(const std::string& message);
    
    // RTSP парсинг
    std::string extract_server_url(const std::string& rtsp_url);
    std::string extract_path(const std::string& rtsp_url);
    int extract_port(const std::string& rtsp_url);
    
    // RTP обработка
    bool parse_rtp_header(const std::vector<uint8_t>& packet, uint32_t& sequence, uint32_t& timestamp, uint32_t& ssrc);

private:
    Config m_config;
    std::unique_ptr<std::ofstream> m_output_file;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_receiving{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Сетевые сокеты
    SOCKET m_rtsp_socket = INVALID_SOCKET;
    SOCKET m_rtp_socket = INVALID_SOCKET;
    
    // Поток приема
    std::thread m_receiving_thread;
    
    // Очередь пакетов
    std::queue<std::vector<uint8_t>> m_packet_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Статистика
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // RTSP состояние
    std::string m_session_id;
    std::string m_server_url;
    std::string m_rtsp_path;
    int m_server_port = 554;
    
    // WinSock
    bool m_wsa_initialized = false;
};

RealRTSPClient::RealRTSPClient(const Config& config) 
    : m_config(config) {
    
    // Инициализация WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_wsa_initialized = true;
    }
    
    log_info("Real RTSP Client initialized");
    log_info("URL: " + config.rtsp_url);
    log_info("Output: " + config.output_file);
    
    // Сброс статистики
    m_stats = Stats{};
    m_stats.start_time = std::chrono::steady_clock::now();
    
    // Парсинг RTSP URL
    m_server_url = extract_server_url(config.rtsp_url);
    m_rtsp_path = extract_path(config.rtsp_url);
    m_server_port = extract_port(config.rtsp_url);
}

RealRTSPClient::~RealRTSPClient() {
    disconnect();
    
    // Очистка WinSock
    if (m_wsa_initialized) {
        WSACleanup();
    }
    
    log_info("Real RTSP Client destroyed");
}

bool RealRTSPClient::connect() {
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
    
    // Установка RTSP соединения
    if (!setup_rtsp_connection()) {
        log_error("RTSP setup failed");
        return false;
    }
    
    m_connected = true;
    log_info("Connected successfully to RTSP stream");
    return true;
}

void RealRTSPClient::disconnect() {
    if (!m_connected.load()) {
        return;
    }
    
    log_info("Disconnecting from RTSP stream");
    
    stop_receiving();
    
    // Закрытие сокетов
    if (m_rtsp_socket != INVALID_SOCKET) {
        closesocket(m_rtsp_socket);
        m_rtsp_socket = INVALID_SOCKET;
    }
    
    if (m_rtp_socket != INVALID_SOCKET) {
        closesocket(m_rtp_socket);
        m_rtp_socket = INVALID_SOCKET;
    }
    
    if (m_output_file && m_output_file->is_open()) {
        m_output_file->close();
    }
    
    m_connected = false;
    log_info("Disconnected");
}

bool RealRTSPClient::start_receiving() {
    if (!m_connected.load()) {
        log_error("Not connected - cannot start receiving");
        return false;
    }
    
    if (m_receiving.load()) {
        log_info("Already receiving");
        return true;
    }
    
    log_info("Starting RTP packet reception");
    
    // Настройка RTP сокета
    if (!setup_rtp_socket()) {
        log_error("Failed to setup RTP socket");
        return false;
    }
    
    m_stop_requested = false;
    m_receiving_thread = std::thread(&RealRTSPClient::receiving_loop, this);
    m_receiving = true;
    
    return true;
}

void RealRTSPClient::stop_receiving() {
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

RealRTSPClient::Stats RealRTSPClient::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

bool RealRTSPClient::setup_rtsp_connection() {
    // Создание RTSP сокета
    m_rtsp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_rtsp_socket == INVALID_SOCKET) {
        log_error("Failed to create RTSP socket");
        return false;
    }
    
    // Настройка адреса сервера
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_server_port);
    
    // Преобразование IP адреса
    struct hostent* host = gethostbyname(m_server_url.c_str());
    if (!host) {
        log_error("Failed to resolve hostname: " + m_server_url);
        return false;
    }
    
    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);
    
    // Подключение к серверу
    if (::connect(m_rtsp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        log_error("Failed to connect to RTSP server");
        return false;
    }
    
    log_info("Connected to RTSP server");
    
    // Отправка OPTIONS запроса
    std::string options_request = "OPTIONS " + m_rtsp_path + " RTSP/1.0\r\n";
    options_request += "CSeq: 1\r\n";
    options_request += "User-Agent: RealRTSPClient/1.0\r\n\r\n";
    
    if (!send_rtsp_request(options_request)) {
        log_error("Failed to send OPTIONS request");
        return false;
    }
    
    std::string response = receive_rtsp_response();
    if (response.empty()) {
        log_error("No response to OPTIONS request");
        return false;
    }
    
    // Отправка DESCRIBE запроса
    std::string describe_request = "DESCRIBE " + m_rtsp_path + " RTSP/1.0\r\n";
    describe_request += "CSeq: 2\r\n";
    describe_request += "User-Agent: RealRTSPClient/1.0\r\n";
    describe_request += "Accept: application/sdp\r\n\r\n";
    
    if (!send_rtsp_request(describe_request)) {
        log_error("Failed to send DESCRIBE request");
        return false;
    }
    
    response = receive_rtsp_response();
    if (response.empty()) {
        log_error("No response to DESCRIBE request");
        return false;
    }
    
    // Отправка SETUP запроса
    std::string setup_request = "SETUP " + m_rtsp_path + " RTSP/1.0\r\n";
    setup_request += "CSeq: 3\r\n";
    setup_request += "User-Agent: RealRTSPClient/1.0\r\n";
    setup_request += "Transport: RTP/AVP;unicast;client_port=" + std::to_string(m_config.rtp_port) + "-" + std::to_string(m_config.rtp_port + 1) + "\r\n\r\n";
    
    if (!send_rtsp_request(setup_request)) {
        log_error("Failed to send SETUP request");
        return false;
    }
    
    response = receive_rtsp_response();
    if (response.empty()) {
        log_error("No response to SETUP request");
        return false;
    }
    
    // Извлечение Session ID
    size_t session_pos = response.find("Session:");
    if (session_pos != std::string::npos) {
        size_t start = session_pos + 8;
        size_t end = response.find("\r\n", start);
        if (end != std::string::npos) {
            m_session_id = response.substr(start, end - start);
            // Удаление пробелов
            m_session_id.erase(0, m_session_id.find_first_not_of(" \t"));
            m_session_id.erase(m_session_id.find_last_not_of(" \t") + 1);
        }
    }
    
    // Отправка PLAY запроса
    std::string play_request = "PLAY " + m_rtsp_path + " RTSP/1.0\r\n";
    play_request += "CSeq: 4\r\n";
    play_request += "User-Agent: RealRTSPClient/1.0\r\n";
    if (!m_session_id.empty()) {
        play_request += "Session: " + m_session_id + "\r\n";
    }
    play_request += "Range: npt=0.000-\r\n\r\n";
    
    if (!send_rtsp_request(play_request)) {
        log_error("Failed to send PLAY request");
        return false;
    }
    
    response = receive_rtsp_response();
    if (response.empty()) {
        log_error("No response to PLAY request");
        return false;
    }
    
    log_info("RTSP setup completed successfully");
    return true;
}

bool RealRTSPClient::send_rtsp_request(const std::string& request) {
    if (m_rtsp_socket == INVALID_SOCKET) {
        return false;
    }
    
    log_info("Sending RTSP request: " + request.substr(0, request.find("\r\n")));
    
    int bytes_sent = send(m_rtsp_socket, request.c_str(), request.length(), 0);
    return bytes_sent == request.length();
}

std::string RealRTSPClient::receive_rtsp_response() {
    if (m_rtsp_socket == INVALID_SOCKET) {
        return "";
    }
    
    char buffer[4096];
    std::string response;
    
    while (true) {
        int bytes_received = recv(m_rtsp_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            break;
        }
        
        buffer[bytes_received] = '\0';
        response += buffer;
        
        // Проверка на завершение ответа
        if (response.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }
    
    return response;
}

bool RealRTSPClient::setup_rtp_socket() {
    // Создание UDP сокета для RTP
    m_rtp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_rtp_socket == INVALID_SOCKET) {
        log_error("Failed to create RTP socket");
        return false;
    }
    
    // Привязка к локальному порту
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(m_config.rtp_port);
    
    if (bind(m_rtp_socket, (struct sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        log_error("Failed to bind RTP socket to port " + std::to_string(m_config.rtp_port));
        return false;
    }
    
    log_info("RTP socket bound to port " + std::to_string(m_config.rtp_port));
    return true;
}

void RealRTSPClient::receiving_loop() {
    log_info("RTP receiving loop started");
    
    char buffer[2048];
    
    while (!m_stop_requested) {
        try {
            // Прием RTP пакета
            sockaddr_in sender_addr;
            int sender_addr_size = sizeof(sender_addr);
            
            int bytes_received = recvfrom(m_rtp_socket, buffer, sizeof(buffer), 0, 
                                        (struct sockaddr*)&sender_addr, &sender_addr_size);
            
            if (bytes_received > 0) {
                std::vector<uint8_t> packet(buffer, buffer + bytes_received);
                
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
            }
            
        } catch (const std::exception& e) {
            log_error("Error in receiving loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    log_info("RTP receiving loop stopped");
}

void RealRTSPClient::save_packet_to_file(const std::vector<uint8_t>& packet) {
    if (!m_output_file || !m_output_file->is_open()) {
        return;
    }
    
    // Сохранение пакета в файл
    m_output_file->write(reinterpret_cast<const char*>(packet.data()), packet.size());
}

void RealRTSPClient::update_stats(const std::vector<uint8_t>& packet) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    m_stats.packets_received++;
    m_stats.bytes_received += packet.size();
    m_stats.last_packet_time = std::chrono::steady_clock::now();
    
    // Парсинг RTP заголовка
    uint32_t sequence, timestamp, ssrc;
    if (parse_rtp_header(packet, sequence, timestamp, ssrc)) {
        if (m_stats.ssrc == 0) {
            m_stats.ssrc = ssrc;
        }
        
        // Расчет потерь пакетов
        if (m_stats.last_sequence > 0) {
            uint32_t expected = (m_stats.last_sequence + 1) % 0x10000;
            if (sequence != expected) {
                if (sequence > expected) {
                    m_stats.packets_lost += sequence - expected;
                }
            }
        }
        
        m_stats.last_sequence = sequence;
    }
    
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

bool RealRTSPClient::parse_rtp_header(const std::vector<uint8_t>& packet, uint32_t& sequence, uint32_t& timestamp, uint32_t& ssrc) {
    if (packet.size() < 12) {
        return false;
    }
    
    // RTP заголовок: 12 байт минимум
    // Bytes 0-1: V=2, P, X, CC, M, PT
    // Bytes 2-3: Sequence number
    // Bytes 4-7: Timestamp  
    // Bytes 8-11: SSRC
    
    sequence = (static_cast<uint32_t>(packet[2]) << 8) | static_cast<uint32_t>(packet[3]);
    timestamp = (static_cast<uint32_t>(packet[4]) << 24) | 
                (static_cast<uint32_t>(packet[5]) << 16) |
                (static_cast<uint32_t>(packet[6]) << 8) |
                static_cast<uint32_t>(packet[7]);
    ssrc = (static_cast<uint32_t>(packet[8]) << 24) |
           (static_cast<uint32_t>(packet[9]) << 16) |
           (static_cast<uint32_t>(packet[10]) << 8) |
           static_cast<uint32_t>(packet[11]);
    
    return true;
}

std::string RealRTSPClient::extract_server_url(const std::string& rtsp_url) {
    size_t start = rtsp_url.find("://") + 3;
    size_t end = rtsp_url.find(":", start);
    if (end == std::string::npos) {
        end = rtsp_url.find("/", start);
    }
    return rtsp_url.substr(start, end - start);
}

std::string RealRTSPClient::extract_path(const std::string& rtsp_url) {
    size_t start = rtsp_url.find("://") + 3;
    size_t slash_pos = rtsp_url.find("/", start);
    return (slash_pos != std::string::npos) ? rtsp_url.substr(slash_pos) : "/";
}

int RealRTSPClient::extract_port(const std::string& rtsp_url) {
    size_t start = rtsp_url.find("://") + 3;
    size_t colon_pos = rtsp_url.find(":", start);
    size_t slash_pos = rtsp_url.find("/", start);
    
    if (colon_pos != std::string::npos && colon_pos < slash_pos) {
        std::string port_str = rtsp_url.substr(colon_pos + 1, slash_pos - colon_pos - 1);
        return std::stoi(port_str);
    }
    
    return 554; // RTSP порт по умолчанию
}

void RealRTSPClient::log_info(const std::string& message) {
    if (m_config.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] INFO: " << message << std::endl;
    }
}

void RealRTSPClient::log_error(const std::string& message) {
    if (m_config.enable_logging) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] ERROR: " << message << std::endl;
    }
}

} // namespace video_streaming

// Тестовые сценарии с реальными источниками
void test_real_wowza_stream() {
    std::cout << "\n=== Testing Real Wowza RTSP Stream ===\n";
    
    video_streaming::RealRTSPClient::Config config;
    config.rtsp_url = "rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2";
    config.output_file = "real_wowza_stream.rtp";
    config.max_packets = 5000;
    config.enable_logging = true;
    config.rtp_port = 5000;
    
    video_streaming::RealRTSPClient client(config);
    
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

void test_real_ipvm_camera() {
    std::cout << "\n=== Testing Real IPVM Camera ===\n";
    
    video_streaming::RealRTSPClient::Config config;
    config.rtsp_url = "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast";
    config.output_file = "real_ipvm_camera.rtp";
    config.max_packets = 3000;
    config.enable_logging = true;
    config.rtp_port = 5002;
    
    video_streaming::RealRTSPClient client(config);
    
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

void test_real_bunny_stream() {
    std::cout << "\n=== Testing Real Big Buck Bunny Stream ===\n";
    
    video_streaming::RealRTSPClient::Config config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "real_bunny_stream.rtp";
    config.max_packets = 2000;
    config.enable_logging = true;
    config.rtp_port = 5004;
    
    video_streaming::RealRTSPClient client(config);
    
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
    std::cout << "=== Real RTSP Client ===\n";
    std::cout << "Connecting to real RTSP video sources\n\n";
    
    try {
        // Тестирование Wowza потока
        test_real_wowza_stream();
        
        // Небольшая пауза между тестами
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Тестирование IPVM камеры
        test_real_ipvm_camera();
        
        // Пауза
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Тестирование Bunny потока
        test_real_bunny_stream();
        
        std::cout << "\n🎉 All real RTSP tests completed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
