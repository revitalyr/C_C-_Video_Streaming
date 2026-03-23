#include "rtsp_server.hpp"
#include "frame.hpp"
#include "synthetic_encoder.hpp"
#include "ffmpeg_h264_encoder.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

namespace video_streaming {

RTSPServer::RTSPServer(const Config& config) 
    : m_config(config)
    , m_logger(Logger::get_logger("RTSPServer"))
{
    m_stats.start_time = std::chrono::steady_clock::now();
    log_info("RTSP Server created with config: " + std::to_string(config.rtsp_port));
}

RTSPServer::~RTSPServer() {
    stop();
}

bool RTSPServer::start() {
    if (m_running.load()) {
        log_error("Server is already running");
        return false;
    }

    // Инициализация видео источника
    if (!init_video_source()) {
        log_error("Failed to initialize video source");
        return false;
    }

    // Создание серверного сокета
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_socket < 0) {
        log_error("Failed to create server socket");
        return false;
    }

    // Настройка сокета
    int opt = 1;
    if (setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_error("Failed to set socket options");
        close(m_server_socket);
        return false;
    }

    // Привязка к адресу
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(m_config.bind_address.c_str());
    server_addr.sin_port = htons(m_config.rtsp_port);

    if (bind(m_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_error("Failed to bind server socket");
        close(m_server_socket);
        return false;
    }

    // Начало прослушивания
    if (listen(m_server_socket, 5) < 0) {
        log_error("Failed to listen on server socket");
        close(m_server_socket);
        return false;
    }

    // Установка неблокирующего режима
    int flags = fcntl(m_server_socket, F_GETFL, 0);
    fcntl(m_server_socket, F_SETFL, flags | O_NONBLOCK);

    m_running.store(true);
    
    // Запуск потоков
    m_server_thread = std::thread(&RTSPServer::server_loop, this);
    m_streaming_thread = std::thread(&RTSPServer::streaming_loop, this);

    log_info("RTSP Server started on " + get_rtsp_url());
    return true;
}

void RTSPServer::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    // Закрытие клиентских сокетов
    {
        std::lock_guard<std::mutex> lock(m_clients_mutex);
        for (int client_socket : m_client_sockets) {
            close(client_socket);
        }
        m_client_sockets.clear();
    }

    // Закрытие серверного сокета
    if (m_server_socket >= 0) {
        close(m_server_socket);
        m_server_socket = -1;
    }

    // Ожидание потоков
    if (m_server_thread.joinable()) {
        m_server_thread.join();
    }
    if (m_streaming_thread.joinable()) {
        m_streaming_thread.join();
    }

    log_info("RTSP Server stopped");
}

RTSPServer::Stats RTSPServer::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    
    Stats stats = m_stats;
    
    // Расчет текущего FPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time);
    if (elapsed.count() > 0) {
        stats.current_fps = static_cast<double>(stats.frames_sent) / elapsed.count();
    }
    
    return stats;
}

void RTSPServer::set_request_handler(RequestHandler handler) {
    m_request_handler = std::move(handler);
}

std::string RTSPServer::get_rtsp_url() const {
    return "rtsp://" + m_config.bind_address + ":" + std::to_string(m_config.rtsp_port) + "/live";
}

void RTSPServer::send_frame(const uint8_t* frame_data, size_t frame_size, uint32_t timestamp) {
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    
    // Создание RTP пакета
    auto rtp_packet = create_rtp_packet(frame_data, frame_size, 
                                       m_rtp_sequence.fetch_add(1), timestamp, m_rtp_ssrc);
    
    // Отправка всем клиентам
    for (int client_socket : m_client_sockets) {
        send_rtp_packet(rtp_packet, client_socket);
    }
    
    // Обновление статистики
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_stats.frames_sent++;
        m_stats.bytes_sent += frame_size;
        m_stats.rtp_packets_sent++;
    }
}

void RTSPServer::server_loop() {
    log_info("Server loop started");
    
    while (m_running.load()) {
        // Принятие новых соединений
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket >= 0) {
            log_info("New client connected from " + std::string(inet_ntoa(client_addr.sin_addr)));
            
            // Добавление клиента
            {
                std::lock_guard<std::mutex> lock(m_clients_mutex);
                m_client_sockets.push_back(client_socket);
                
                std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
                m_stats.total_connections++;
                m_stats.active_connections = m_client_sockets.size();
            }
            
            // Обработка клиента в отдельном потоке
            std::thread([this, client_socket]() {
                handle_client(client_socket);
            }).detach();
        }
        
        // Удаление отключенных клиентов
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_client_sockets.erase(
                std::remove_if(m_client_sockets.begin(), m_client_sockets.end(),
                    [this](int socket) {
                        // Проверка соединения
                        char buffer;
                        ssize_t result = recv(socket, &buffer, 1, MSG_PEEK | MSG_DONTWAIT);
                        if (result == 0) {
                            log_info("Client disconnected");
                            close(socket);
                            
                            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
                            m_stats.active_connections--;
                            return true;
                        }
                        return false;
                    }
                ),
                m_client_sockets.end()
            );
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    log_info("Server loop stopped");
}

void RTSPServer::streaming_loop() {
    log_info("Streaming loop started");
    
    auto frame_interval = std::chrono::milliseconds(1000 / m_config.fps);
    auto last_frame_time = std::chrono::steady_clock::now();
    
    while (m_running.load()) {
        auto now = std::chrono::steady_clock::now();
        
        if (now - last_frame_time >= frame_interval) {
            // Получение следующего кадра
            auto frame = get_next_frame();
            if (frame) {
                // Отправка кадра
                send_frame(frame->data.data(), frame->data.size(), m_rtp_timestamp.fetch_add(90000 / m_config.fps));
                last_frame_time = now;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    log_info("Streaming loop stopped");
}

void RTSPServer::handle_client(int client_socket) {
    char buffer[4096];
    std::string request;
    
    // Чтение RTSP запроса
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        request = std::string(buffer);
        
        // Парсинг запроса
        std::istringstream iss(request);
        std::string method, url, version;
        iss >> method >> url >> version;
        
        // Чтение заголовков
        std::map<std::string, std::string> headers;
        std::string line;
        while (std::getline(iss, line) && line != "\r" && line != "") {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Удаление пробелов
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                
                headers[key] = value;
            }
        }
        
        log_info("RTSP request: " + method + " " + url);
        
        // Обработка запроса
        std::string response;
        if (method == "OPTIONS") {
            response = handle_options();
        } else if (method == "DESCRIBE") {
            response = handle_describe();
        } else if (method == "SETUP") {
            response = handle_setup(headers);
        } else if (method == "PLAY") {
            response = handle_play();
        } else if (method == "TEARDOWN") {
            response = handle_teardown();
        } else {
            response = "RTSP/1.0 501 Not Implemented\r\n\r\n";
        }
        
        // Отправка ответа
        send(client_socket, response.c_str(), response.length(), 0);
        
        log_info("RTSP response sent");
    }
    
    // Закрытие соединения (для простоты)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    close(client_socket);
}

std::string RTSPServer::handle_options() {
    return "RTSP/1.0 200 OK\r\n"
           "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n"
           "\r\n";
}

std::string RTSPServer::handle_describe() {
    std::string sdp = generate_sdp();
    
    return "RTSP/1.0 200 OK\r\n"
           "Content-Type: application/sdp\r\n"
           "Content-Length: " + std::to_string(sdp.length()) + "\r\n"
           "\r\n" + sdp;
}

std::string RTSPServer::handle_setup(const std::map<std::string, std::string>& headers) {
    // Для простоты - всегда успешный ответ
    return "RTSP/1.0 200 OK\r\n"
           "Session: 12345678\r\n"
           "Transport: RTP/AVP;unicast;client_port=" + 
           std::to_string(m_config.rtp_port_start) + "-" + 
           std::to_string(m_config.rtp_port_start + 1) + "\r\n"
           "\r\n";
}

std::string RTSPServer::handle_play() {
    return "RTSP/1.0 200 OK\r\n"
           "Session: 12345678\r\n"
           "Range: npt=0.000-\r\n"
           "\r\n";
}

std::string RTSPServer::handle_teardown() {
    return "RTSP/1.0 200 OK\r\n"
           "Session: 12345678\r\n"
           "\r\n";
}

std::string RTSPServer::generate_sdp() {
    std::ostringstream sdp;
    
    sdp << "v=0\r\n";
    sdp << "o=- 1234567890 1234567890 IN IP4 " << m_config.bind_address << "\r\n";
    sdp << "s=Local Video Stream\r\n";
    sdp << "c=IN IP4 " << m_config.bind_address << "\r\n";
    sdp << "t=0 0\r\n";
    sdp << "m=video 0 RTP/AVP 96\r\n";
    sdp << "a=rtpmap:96 H264/90000\r\n";
    sdp << "a=fmtp:96 profile-level-id=42C01E;packetization-mode=1\r\n";
    sdp << "a=control:trackID=1\r\n";
    
    return sdp.str();
}

std::vector<uint8_t> RTSPServer::create_rtp_packet(const uint8_t* payload, size_t payload_size,
                                                   uint16_t sequence, uint32_t timestamp, uint32_t ssrc) {
    std::vector<uint8_t> packet;
    
    // RTP Header (12 bytes)
    packet.push_back(0x80);  // Version=2, Padding=0, Extension=0, CSRC=0
    packet.push_back(96);    // Payload type=96 (H264)
    
    // Sequence number
    packet.push_back((sequence >> 8) & 0xFF);
    packet.push_back(sequence & 0xFF);
    
    // Timestamp
    packet.push_back((timestamp >> 24) & 0xFF);
    packet.push_back((timestamp >> 16) & 0xFF);
    packet.push_back((timestamp >> 8) & 0xFF);
    packet.push_back(timestamp & 0xFF);
    
    // SSRC
    packet.push_back((ssrc >> 24) & 0xFF);
    packet.push_back((ssrc >> 16) & 0xFF);
    packet.push_back((ssrc >> 8) & 0xFF);
    packet.push_back(ssrc & 0xFF);
    
    // Payload
    packet.insert(packet.end(), payload, payload + payload_size);
    
    return packet;
}

void RTSPServer::send_rtp_packet(const std::vector<uint8_t>& packet, int client_socket) {
    // Для простоты отправляем через TCP (RTSP соединение)
    // В реальной реализации нужно UDP
    send(client_socket, packet.data(), packet.size(), MSG_NOSIGNAL);
}

bool RTSPServer::init_video_source() {
    if (m_config.video_file.empty()) {
        // Синтетический видео источник
        m_encoder = std::make_unique<SyntheticEncoder>();
        log_info("Using synthetic video source");
    } else {
        // FFmpeg видео источник
        m_encoder = std::make_unique<FFmpegH264Encoder>();
        log_info("Using video file: " + m_config.video_file);
    }
    
    return m_encoder != nullptr;
}

std::unique_ptr<VideoFrame> RTSPServer::get_next_frame() {
    if (!m_encoder) {
        return nullptr;
    }
    
    // Генерация или чтение следующего кадра
    uint32_t frame_id = m_frame_count.fetch_add(1);
    return m_encoder->encode_frame(frame_id);
}

void RTSPServer::log_info(const std::string& message) {
    if (m_config.enable_logging && m_logger) {
        m_logger->info(message);
    }
}

void RTSPServer::log_error(const std::string& message) {
    if (m_config.enable_logging && m_logger) {
        m_logger->error(message);
    }
}

} // namespace video_streaming
