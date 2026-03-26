module;

#include <string>
#include <vector>
#include <memory>
#include <chrono>

export module video_streaming.rtp.rtsp_client;

import video_streaming.common.types;
import video_streaming.network.endpoint;
import video_streaming.network.udp_socket;
import video_streaming.logger;

namespace video_streaming {

export struct Stats {
    std::chrono::steady_clock::time_point start_time;
    u64 packets_sent = 0;
    u64 packets_received = 0;
    u64 bytes_sent = 0;
    u64 bytes_received = 0;
};

export class RtspClient {
private:
    Endpoint m_server_endpoint;
    std::unique_ptr<UdpSocket> m_rtp_socket;
    std::unique_ptr<UdpSocket> m_rtcp_socket;
    std::string m_session_id;
    std::string m_username;
    std::string m_password;
    std::string m_rtsp_path;
    std::string m_server_url;
    u16 m_client_port;
    u16 m_server_port;
    bool m_connected = false;
    Stats m_stats;
    std::shared_ptr<Logger> m_logger;
    
public:
    explicit RtspClient(const Endpoint& server_endpoint);
    ~RtspClient();
    
    bool connect();
    bool setup_stream();
    bool play();
    bool pause();
    bool teardown();
    void disconnect();
    
    bool is_connected() const { return m_connected; }
    Endpoint get_server_endpoint() const { return m_server_endpoint; }
    u16 get_client_port() const { return m_client_port; }
    
    std::vector<u8> receive_rtp_packet();
    bool send_rtcp_packet(const std::vector<u8>& packet);
    
private:
    std::string send_command(const std::string& command, const std::string& url);
    std::string generate_session_id();
    std::string base64_encode(const std::string& data);
    void parse_url(const std::string& url);
};

} // namespace video_streaming
