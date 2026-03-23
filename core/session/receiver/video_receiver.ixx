module;

#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>

#include "common/types.hpp"
#include "media/frame.hpp"
#include "network/receiver.hpp"
#include "network/udp_socket.hpp"

extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVFormatContext;
}

export module video_streaming.receiver;

import video_streaming.logger;
import video_streaming.rtp.h264_depacketizer;
import video_streaming.jitter;

namespace video_streaming {

export class VideoReceiver {
public:
    struct Config {
        uint16_t port = 5000;
        std::string bind_ip = "0.0.0.0";
        int jitter_buffer_size = 50; // packets
        int max_frame_size = 1024 * 1024; // 1MB
        bool use_rtsp = false;
        std::string rtsp_url;
    };

    using FrameCallback = std::function<void(const Frame&)>;

    explicit VideoReceiver(const Config& config);
    ~VideoReceiver();

    bool start();
    void stop();

    void set_frame_callback(FrameCallback callback);

    struct Stats {
        uint64_t packets_received = 0;
        uint64_t frames_received = 0; // Decoded
        uint64_t frames_decoded = 0; // Alias for frames_received in new logic
        uint64_t bytes_received = 0;
        uint64_t packets_lost = 0;
        uint64_t packets_reordered = 0;
        double decoder_latency_ms = 0.0;
        double fps_actual = 0.0;
        std::chrono::milliseconds jitter_buffer_delay{0};
        double packet_loss_rate = 0.0;
    };

    Stats get_stats() const;
    void reset_stats();

private:
    void receive_loop();
    void rtsp_receive_loop();
    void process_rtp_packet(const std::vector<uint8_t>& packet);
    void process_jitter_buffer();
    std::unique_ptr<Frame> assemble_frame(const std::vector<std::vector<uint8_t>>& nal_units);
    void update_stats();

    Config m_config;
    std::unique_ptr<Logger> m_logger;
    FrameCallback m_frame_callback;

    std::unique_ptr<UdpSocket> m_socket; // or Receiver if unified
    std::unique_ptr<Receiver> m_receiver;
    std::unique_ptr<JitterBuffer> m_jitter_buffer;
    std::unique_ptr<H264Depacketizer> m_depacketizer;
    
    AVCodecContext* m_codec_ctx = nullptr;
    AVFrame* m_decoded_frame = nullptr;
    AVPacket* m_packet_for_decoder = nullptr;
    AVFormatContext* m_format_ctx = nullptr;

    std::thread m_receiver_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};

    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // Legacy fields for compilation compatibility if methods used them, 
    // cleaned up for module version:
    std::queue<std::unique_ptr<Frame>> m_frame_queue;
    mutable std::mutex m_frame_queue_mutex;
    std::condition_variable m_frame_queue_cv;
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_stats_update;
    uint64_t m_total_packets_expected = 0;
};

} // namespace video_streaming