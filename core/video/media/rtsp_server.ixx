module;

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

export module video_streaming.media.rtsp_server;

import video_streaming.media.frame;
import video_streaming.media.synthetic_encoder;
import video_streaming.media.ffmpeg_h264_encoder;
import video_streaming.common.types;
import video_streaming.network.endpoint;
import video_streaming.network.udp_socket;
import video_streaming.logger;

namespace video_streaming {

export struct RtspServerConfig {
    u16 rtsp_port = 8554;
    std::string rtsp_url;
    std::string output_file;
};

export class RtspServer {
private:
    RtspServerConfig m_config;
    std::shared_ptr<Logger> m_logger;
    std::atomic<bool> m_running{false};
    std::thread m_server_thread;
    
public:
    explicit RtspServer(const RtspServerConfig& config);
    ~RtspServer();
    
    bool start();
    void stop();
    bool is_running() const;
    void send_frame(const Frame& frame);
    
    std::string generate_sdp();
    std::string generate_response(const std::string& status, const std::string& content = "");
    
private:
    void server_loop();
    void handle_client_request(const std::string& request, const Endpoint& client);
};

} // namespace video_streaming
