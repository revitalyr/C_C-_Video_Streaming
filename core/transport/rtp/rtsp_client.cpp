#include "rtsp_client.hpp"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <ctime>

namespace video_streaming {

static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static std::string base64_encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

RTSPClient::RTSPClient(const Config& config) 
    : m_config(config) {
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_wsa_initialized = true;
    }
#endif
    
    log_info("RTSP Client initialized");
    log_info("URL: " + config.rtsp_url);
    
    m_stats = Stats{};
    m_stats.start_time = std::chrono::steady_clock::now();
    
    std::string url_str = config.rtsp_url;
    m_server_port = 554;
    
    size_t scheme_pos = url_str.find("://");
    if (scheme_pos != std::string::npos) {
        url_str = url_str.substr(scheme_pos + 3);
    }

    size_t slash_pos = url_str.find('/');
    if (slash_pos != std::string::npos) {
        m_rtsp_path = url_str.substr(slash_pos);
        url_str = url_str.substr(0, slash_pos);
    } else {
        m_rtsp_path = "/";
    }

    size_t at_pos = url_str.find('@');
    if (at_pos != std::string::npos) {
        std::string user_pass = url_str.substr(0, at_pos);
        url_str = url_str.substr(at_pos + 1);
        
        size_t colon_pos = user_pass.find(':');
        if (colon_pos != std::string::npos) {
            m_username = user_pass.substr(0, colon_pos);
            m_password = user_pass.substr(colon_pos + 1);
        } else {
            m_username = user_pass;
        }
    }

    size_t colon_pos = url_str.find(':');
    if (colon_pos != std::string::npos) {
        m_server_url = url_str.substr(0, colon_pos);
        try { m_server_port = std::stoi(url_str.substr(colon_pos + 1)); } catch (...) {}
    } else {
        m_server_url = url_str;
    }
}

RTSPClient::~RTSPClient() {
    disconnect();
#ifdef _WIN32
    if (m_wsa_initialized) {
        WSACleanup();
    }
#endif
    log_info("RTSP Client destroyed");
}

bool RTSPClient::connect() {
    if (m_connected.load()) return true;
    
    log_info("Connecting to RTSP stream: " + m_config.rtsp_url);
    
    m_output_file = std::make_unique<std::ofstream>(m_config.output_file, std::ios::binary);
    if (!m_output_file->is_open()) {
        log_error("Failed to open output file: " + m_config.output_file);
        return false;
    }
    
    if (!setup_rtsp_connection()) {
        log_error("RTSP setup failed");
        return false;
    }
    
    m_connected = true;
    log_info("Connected successfully to RTSP stream");
    return true;
}

void RTSPClient::disconnect() {
    if (!m_connected.load()) return;
    
    stop_receiving();
    
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
}

bool RTSPClient::start_receiving() {
    if (!m_connected.load()) return false;
    if (m_receiving.load()) return true;
    
    if (!setup_rtp_socket()) return false;
    
    m_stop_requested = false;
    m_receiving_thread = std::thread(&RTSPClient::receiving_loop, this);
    m_receiving = true;
    return true;
}

void RTSPClient::stop_receiving() {
    if (!m_receiving.load()) return;
    
    m_stop_requested = true;
    m_queue_cv.notify_all();
    
    if (m_receiving_thread.joinable()) {
        m_receiving_thread.join();
    }
    
    m_receiving = false;
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        while (!m_packet_queue.empty()) m_packet_queue.pop();
    }
}

RTSPClient::Stats RTSPClient::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

bool RTSPClient::setup_rtsp_connection() {
    m_rtsp_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_rtsp_socket == INVALID_SOCKET) return false;
    
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    std::string port_str = std::to_string(m_server_port);
    int err = getaddrinfo(m_server_url.c_str(), port_str.c_str(), &hints, &res);
    if (err != 0) {
        log_error("Failed to resolve hostname '" + m_server_url + "': " + std::string(gai_strerror(err)));
        return false;
    }
    
    // Re-create socket with correct family from getaddrinfo
    if (m_rtsp_socket != INVALID_SOCKET) closesocket(m_rtsp_socket);
    m_rtsp_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (m_rtsp_socket == INVALID_SOCKET) {
        log_error("Failed to create socket");
        freeaddrinfo(res);
        return false;
    }
    
    if (::connect(m_rtsp_socket, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR) {
        log_error("Failed to connect to " + m_server_url + ":" + port_str);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);
    
    std::string options_req = "OPTIONS " + m_rtsp_path + " RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: RTSPClient/1.0\r\n\r\n";
    if (!send_rtsp_request(options_req)) return false;
    if (receive_rtsp_response().find(" 200 OK") == std::string::npos) return false;
    
    std::string describe_req = "DESCRIBE " + m_rtsp_path + " RTSP/1.0\r\nCSeq: 2\r\nUser-Agent: RTSPClient/1.0\r\nAccept: application/sdp\r\n\r\n";
    if (!send_rtsp_request(describe_req)) return false;
    if (receive_rtsp_response().find(" 200 OK") == std::string::npos) return false;
    
    std::string response = receive_rtsp_response(); // Need to read headers+body properly to get SDP
    
    // Parse SDP to find the control URL for SETUP (simplified: looking for video track control)
    std::string setup_url = m_rtsp_path;
    size_t media_pos = response.find("m=video");
    if (media_pos != std::string::npos) {
        size_t control_pos = response.find("a=control:", media_pos);
        if (control_pos != std::string::npos) {
            size_t val_start = control_pos + 10;
            size_t val_end = response.find("\r\n", val_start);
            std::string control_val = response.substr(val_start, val_end - val_start);
            
            if (control_val.find("://") != std::string::npos) {
                setup_url = control_val;
            } else if (control_val != "*") {
                setup_url = (m_rtsp_path.back() == '/') ? m_rtsp_path + control_val : m_rtsp_path + "/" + control_val;
            }
        }
    }

    std::string setup_req = "SETUP " + setup_url + " RTSP/1.0\r\nCSeq: 3\r\nUser-Agent: RTSPClient/1.0\r\nTransport: RTP/AVP;unicast;client_port=" + std::to_string(m_config.rtp_port) + "-" + std::to_string(m_config.rtp_port + 1) + "\r\n\r\n";
    if (!send_rtsp_request(setup_req)) return false;
    
    response = receive_rtsp_response();
    if (response.find(" 200 OK") == std::string::npos) return false;
    
    size_t session_pos = response.find("Session:");
    if (session_pos != std::string::npos) {
        size_t start = session_pos + 8;
        size_t end = response.find("\r\n", start);
        if (end != std::string::npos) {
            std::string line = response.substr(start, end - start);
            size_t semi = line.find(";");
            m_session_id = (semi != std::string::npos) ? line.substr(0, semi) : line;
            m_session_id.erase(0, m_session_id.find_first_not_of(" \t"));
            m_session_id.erase(m_session_id.find_last_not_of(" \t") + 1);
        }
    }
    
    std::string play_req = "PLAY " + m_rtsp_path + " RTSP/1.0\r\nCSeq: 4\r\nUser-Agent: RTSPClient/1.0\r\n";
    if (!m_session_id.empty()) play_req += "Session: " + m_session_id + "\r\n";
    play_req += "Range: npt=0.000-\r\n\r\n";
    
    if (!send_rtsp_request(play_req)) return false;
    if (receive_rtsp_response().find(" 200 OK") == std::string::npos) return false;
    
    return true;
}

bool RTSPClient::send_rtsp_request(const std::string& request) {
    if (m_rtsp_socket == INVALID_SOCKET) return false;
    log_info("Request: " + request.substr(0, request.find("\r\n")));
    
    std::string final = request;
    if (!m_username.empty()) {
        std::string auth = m_username + ":" + m_password;
        size_t end_headers = final.rfind("\r\n\r\n");
        if (end_headers != std::string::npos) {
            final.insert(end_headers + 2, "Authorization: Basic " + base64_encode(auth) + "\r\n");
        }
    }
    return send(m_rtsp_socket, final.c_str(), final.length(), 0) == (int)final.length();
}

std::string RTSPClient::receive_rtsp_response() {
    if (m_rtsp_socket == INVALID_SOCKET) return "";
    char buffer[4096];
    std::string response;
    int content_length = 0;
    bool headers_done = false;

    while (true) {
        // Check if headers are complete
        if (!headers_done) {
            size_t header_end = response.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                headers_done = true;
                // Extract Content-Length
                size_t cl_pos = response.find("Content-Length:");
                if (cl_pos != std::string::npos && cl_pos < header_end) {
                    try { content_length = std::stoi(response.substr(cl_pos + 15)); } catch(...) {}
                }
            }
        }

        // If headers done, check if we have full body
        if (headers_done && response.length() >= (response.find("\r\n\r\n") + 4 + content_length)) {
            break;
        }

        int bytes = recv(m_rtsp_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        response.append(buffer, bytes);
    }
    return response;
}

bool RTSPClient::setup_rtp_socket() {
    m_rtp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_rtp_socket == INVALID_SOCKET) return false;
    
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(m_config.rtp_port);
    
    return bind(m_rtp_socket, (struct sockaddr*)&local, sizeof(local)) != SOCKET_ERROR;
}

void RTSPClient::receiving_loop() {
    char buffer[2048];
    while (!m_stop_requested) {
        sockaddr_in sender{};
        socklen_t size = sizeof(sender);
        int bytes = recvfrom(m_rtp_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &size);
        if (bytes > 0) {
            std::vector<uint8_t> packet(buffer, buffer + bytes);
            save_packet_to_file(packet);
            update_stats(packet);
            
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_packet_queue.push(std::move(packet));
            if (m_packet_queue.size() > 100) m_packet_queue.pop();
            m_queue_cv.notify_one();
        }
    }
}

void RTSPClient::save_packet_to_file(const std::vector<uint8_t>& packet) {
    if (m_output_file && m_output_file->is_open()) m_output_file->write((const char*)packet.data(), packet.size());
}

void RTSPClient::update_stats(const std::vector<uint8_t>& packet) {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats.packets_received++;
    m_stats.bytes_received += packet.size();
}

void RTSPClient::log_info(const std::string& message) {
    if (m_config.enable_logging) std::cout << "[INFO] " << message << std::endl;
}

void RTSPClient::log_error(const std::string& message) {
    if (m_config.enable_logging) std::cerr << "[ERROR] " << message << std::endl;
}

} // namespace video_streaming