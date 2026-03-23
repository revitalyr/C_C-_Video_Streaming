#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>

import video_streaming.logger;
#include "media/frame.hpp"
#include "network/udp_socket.hpp"
#include "rtp/h264_depacketizer.hpp"
#include "jitter/jitter_buffer.hpp"

namespace video_streaming {

class VideoReceiver {
public:
    struct Config {
        uint16_t port = 5000;
        std::string bind_ip = "0.0.0.0";
        int jitter_buffer_size = 50; // packets
        int max_frame_size = 1024 * 1024; // 1MB
        bool enable_reordering = true;
    };

    explicit VideoReceiver(const Config& config);
    ~VideoReceiver();

    // Main management methods
    bool start();
    void stop();
    bool is_running() const noexcept { return m_running.load(); }

    // Get decoded frames
    std::unique_ptr<VideoFrame> get_frame(int timeout_ms = 100);
    
    // Statistics
    struct Stats {
        uint64_t frames_received = 0;
        uint64_t packets_received = 0;
        uint64_t packets_lost = 0;
        uint64_t packets_reordered = 0;
        uint64_t bytes_received = 0;
        double fps_actual = 0.0;
        std::chrono::milliseconds jitter_buffer_delay{0};
        double packet_loss_rate = 0.0;
    };
    
    Stats get_stats() const;
    
    // Reset statistics
    void reset_stats();

private:
    // Main worker loop
    void receiver_loop();
    
    // RTSP worker loop
    void rtsp_receive_loop();
    
    // Process RTP packet
    void process_rtp_packet(const std::vector<uint8_t>& packet);
    
    // Assemble frame from NAL units
    std::unique_ptr<VideoFrame> assemble_frame(const std::vector<std::vector<uint8_t>>& nal_units);
    
    // Update statistics
    void update_stats();

private:
    Config m_config;
    std::unique_ptr<Logger> m_logger;
    
    // Components
    std::unique_ptr<UdpSocket> m_socket;
    std::unique_ptr<H264Depacketizer> m_depacketizer;
    std::unique_ptr<JitterBuffer> m_jitter_buffer;
    
    // Thread management
    std::thread m_receiver_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Frame queue for output
    std::queue<std::unique_ptr<VideoFrame>> m_frame_queue;
    mutable std::mutex m_frame_queue_mutex;
    std::condition_variable m_frame_queue_cv;
    
    // Statistics
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // Timings
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_stats_update;
    
    // RTP state
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_sequence_timestamps;
    uint32_t m_last_sequence_number = 0;
    uint64_t m_total_packets_expected = 0;
};

} // namespace video_streaming
