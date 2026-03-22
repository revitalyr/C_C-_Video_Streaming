#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

import video_streaming.logger;
#include "../media/frame.hpp"
#include "../media/synthetic_encoder.hpp"
#include "../network/udp_socket.hpp"
#include "../rtp/h264_packetizer.hpp"

namespace video_streaming {

class VideoSender {
public:
    struct Config {
        uint16_t port = 5000;
        std::string destination_ip = "127.0.0.1";
        int fps = 30;
        int width = 1920;
        int height = 1080;
        int bitrate = 4000000; // 4 Mbps
    };

    explicit VideoSender(const Config& config);
    ~VideoSender();

    // Main management methods
    bool start();
    void stop();
    bool is_running() const noexcept { return m_running.load(); }

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
    // Main worker loop
    void sender_loop();
    
    // Generate video frames
    std::unique_ptr<VideoFrame> generate_frame();
    
    // Send frame
    bool send_frame(const VideoFrame& frame);

private:
    Config m_config;
    std::unique_ptr<Logger> m_logger;
    
    // Components
    std::unique_ptr<SyntheticH264Encoder> m_encoder;
    std::unique_ptr<UdpSocket> m_socket;
    std::unique_ptr<H264Packetizer> m_packetizer;
    
    // Thread management
    std::thread m_sender_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Frame queue
    std::queue<std::unique_ptr<VideoFrame>> m_frame_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // Timing
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_frame_time;
};

} // namespace video_streaming
