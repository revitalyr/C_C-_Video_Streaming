#pragma once

#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <fstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socklen_t = int;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    using SOCKET = int;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace video_streaming {

class RTSPClient {
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
        uint64_t packets_lost = 0;
        double packet_loss_rate = 0.0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_packet_time;
        double current_fps = 0.0;
        uint32_t last_sequence = 0;
        uint32_t ssrc = 0;
    };
    
    explicit RTSPClient(const Config& config);
    ~RTSPClient();
    
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
    bool setup_rtp_socket();
    void receiving_loop();
    void save_packet_to_file(const std::vector<uint8_t>& packet);
    void update_stats(const std::vector<uint8_t>& packet);
    void log_info(const std::string& message);
    void log_error(const std::string& message);
    bool parse_rtp_header(const std::vector<uint8_t>& packet, uint32_t& sequence, uint32_t& timestamp, uint32_t& ssrc);

    Config m_config;
    std::unique_ptr<std::ofstream> m_output_file;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_receiving{false};
    std::atomic<bool> m_stop_requested{false};
    SOCKET m_rtsp_socket = INVALID_SOCKET;
    SOCKET m_rtp_socket = INVALID_SOCKET;
    std::thread m_receiving_thread;
    std::queue<std::vector<uint8_t>> m_packet_queue;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    std::string m_session_id;
    std::string m_server_url;
    std::string m_rtsp_path;
    int m_server_port = 554;
    std::string m_username;
    std::string m_password;
    bool m_wsa_initialized = false;
};

} // namespace video_streaming