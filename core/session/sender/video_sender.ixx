module;

#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <string>

export module video_streaming.sender;

import video_streaming.logger;
import video_streaming.media.frame;
import video_streaming.interfaces;
import video_streaming.rtp.h264_packetizer;
import video_streaming.network.udp_socket;
import video_streaming.media.synthetic_encoder;

namespace video_streaming {

export class VideoSender : public video_streaming::IRunnable {
public:
    struct Config {
        uint16_t port = 5000;
        std::string destination_ip = "127.0.0.1";
        int fps = 30;
        int width = 1920;
        int height = 1080;
        int bitrate = 4000000; // 4 Mbps
        
        // Network simulation
        double packet_loss = 0.0; // 0-100%
        int delay_ms = 0;
        int jitter_ms = 0;
    };

    explicit VideoSender(const Config& config);
    ~VideoSender();

    // Main management methods
    bool start() override;
    void stop() override;
    bool is_running() const override { return m_running.load(); }

    // Statistics
    struct Stats {
        uint64_t frames_sent = 0;
        uint64_t packets_sent = 0;
        uint64_t bytes_sent = 0;
        double fps_actual = 0.0;
        std::chrono::milliseconds encoding_time{0};
        std::chrono::milliseconds network_time{0};
    };
    
    Stats get_stats() const;

private:
    void sender_loop();
    std::unique_ptr<Frame> generate_frame();
    bool send_frame(const Frame& frame);

    Config m_config;
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<SyntheticH264Encoder> m_encoder;
    std::unique_ptr<UdpSocket> m_socket;
    std::unique_ptr<H264Packetizer> m_packetizer;
    std::thread m_sender_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    std::queue<std::unique_ptr<Frame>> m_frame_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_frame_time;
    std::mt19937 m_rng;
};

} // namespace video_streaming