module;

#include <coroutine>
#include <memory>
#include <optional>
#include <chrono>
#include <thread>
#include <functional>
#include <iostream>
#include <random>

export module video_streaming.async.coroutine_network_sender;

import video_streaming.media.frame;
import video_streaming.common.types;
import video_streaming.network.endpoint;
import video_streaming.network.udp_socket;
import video_streaming.rtp.h264_packetizer;
import video_streaming.rtp.packet;
import video_streaming.media.synthetic_encoder;
import video_streaming.async.coroutine_types;

export namespace video_streaming::async {

// Coroutine-based network sender
class CoroutineNetworkSender {
public:
    struct Config {
        std::string destination_ip = "127.0.0.1";
        uint16_t port = 5000;
        int fps = 30;
        int width = 1920;
        int height = 1080;
        int bitrate = 4000000;
        
        // Network simulation
        double packet_loss = 0.0; // 0-100%
        int delay_ms = 0;
        int jitter_ms = 0;
    };
    
    struct Stats {
        uint64_t frames_sent = 0;
        uint64_t packets_sent = 0;
        uint64_t bytes_sent = 0;
        double fps_actual = 0.0;
        std::chrono::milliseconds encoding_time{0};
        std::chrono::milliseconds network_time{0};
    };
    
private:
    Config config_;
    std::unique_ptr<UdpSocket> socket_;
    std::unique_ptr<SyntheticH264Encoder> encoder_;
    std::unique_ptr<H264Packetizer> packetizer_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
    Stats stats_;
    bool running_;
    
public:
    explicit CoroutineNetworkSender(const Config& config) 
        : config_(config), running_(false) {
        
        std::cout << "🔧 Initializing coroutine network sender..." << std::endl;
        
        // Initialize components
        encoder_ = std::make_unique<SyntheticH264Encoder>(
            config.width, config.height, config.fps, config.bitrate);
        
        socket_ = std::make_unique<UdpSocket>();
        packetizer_ = std::make_unique<H264Packetizer>(12345); // SSRC
        
        start_time_ = std::chrono::steady_clock::now();
        last_frame_time_ = start_time_;
        
        std::cout << "✅ Coroutine network sender initialized" << std::endl;
    }
    
    // Async frame encoding
    Task<std::vector<uint8_t>> encode_frame_async(const Frame& frame) {
        std::cout << "🎬 Starting async frame encoding..." << std::endl;
        auto encode_start = std::chrono::steady_clock::now();
        
        // Simulate async encoding (in real implementation, this would be truly async)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        
        std::vector<uint8_t> encoded_data;
        try {
            auto encoded_frames = encoder_->encode(frame);
            // Convert vector<EncodedFrame> to vector<uint8_t>
            for (const auto& encoded_frame : encoded_frames) {
                encoded_data.insert(encoded_data.end(), 
                                  encoded_frame.data.begin(), 
                                  encoded_frame.data.end());
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Encoding error: " << e.what() << std::endl;
            co_return std::vector<uint8_t>{};
        }
        
        auto encode_end = std::chrono::steady_clock::now();
        stats_.encoding_time = std::chrono::duration_cast<std::chrono::milliseconds>(encode_end - encode_start);
        
        std::cout << "✅ Frame encoded: " << encoded_data.size() << " bytes" << std::endl;
        co_return encoded_data;
    }
    
    // Async packet sending
    Task<bool> send_packets_async(const std::vector<uint8_t>& encoded_data) {
        std::cout << "📡 Starting async packet sending..." << std::endl;
        auto send_start = std::chrono::steady_clock::now();
        
        if (!socket_->is_open()) {
            if (!socket_->open()) {
                std::cerr << "❌ Failed to open socket" << std::endl;
                co_return false;
            }
            socket_->set_blocking(false);
        }
        
        // Packetize data
        uint32_t timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_).count());
        auto packets = packetizer_->packetize_frame(encoded_data, timestamp);
        
        // Send packets
        Endpoint destination(config_.destination_ip, config_.port);
        bool success = true;
        
        for (const auto& packet : packets) {
            // Simulate network delay and jitter
            if (config_.delay_ms > 0 || config_.jitter_ms > 0) {
                int delay = config_.delay_ms;
                if (config_.jitter_ms > 0) {
                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    std::uniform_int_distribution<> jitter_dist(-config_.jitter_ms, config_.jitter_ms);
                    delay += jitter_dist(gen);
                }
                if (delay > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                }
            }
            
            // Simulate packet loss
            if (config_.packet_loss > 0) {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_real_distribution<> loss_dist(0.0, 100.0);
                
                if (loss_dist(gen) < config_.packet_loss) {
                    std::cout << "📉 Packet lost (simulated)" << std::endl;
                    continue;
                }
            }
            
            if (!socket_->send_to(packet.serialize(), destination.ip_address, destination.port)) {
                std::cerr << "❌ Failed to send packet" << std::endl;
                success = false;
            }
            
            stats_.packets_sent++;
            stats_.bytes_sent += packet.size();
        }
        
        auto send_end = std::chrono::steady_clock::now();
        stats_.network_time = std::chrono::duration_cast<std::chrono::milliseconds>(send_end - send_start);
        
        std::cout << "✅ Packets sent: " << packets.size() << " packets, " 
                  << stats_.bytes_sent << " bytes" << std::endl;
        co_return success;
    }
    
    // Main sending coroutine
    Task<bool> send_frame_async(const Frame& frame) {
        std::cout << "🎬 Starting async frame sending..." << std::endl;
        
        // Encode frame synchronously for now (Task implementation needs fixing)
        auto encoded_data_task = encode_frame_async(frame);
        auto encoded_data = encoded_data_task.get(); // Get result synchronously
        
        if (encoded_data.empty()) {
            std::cerr << "❌ Frame encoding failed" << std::endl;
            co_return false;
        }
        
        // Send packets
        auto send_task = send_packets_async(encoded_data);
        bool success = send_task.get(); // Get result synchronously
        
        if (!success) {
            std::cerr << "❌ Packet sending failed" << std::endl;
            co_return false;
        }
        
        // Update stats
        stats_.frames_sent++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        if (elapsed.count() > 0) {
            stats_.fps_actual = (stats_.frames_sent * 1000.0) / elapsed.count();
        }
        
        std::cout << "✅ Frame sent successfully" << std::endl;
        co_return success;
    }
    
    // Start the sender (non-blocking)
    bool start() {
        if (running_) {
            return true;
        }
        
        std::cout << "🚀 Starting coroutine network sender..." << std::endl;
        running_ = true;
        return true;
    }
    
    // Stop the sender
    void stop() {
        if (!running_) {
            return;
        }
        
        std::cout << "🛑 Stopping coroutine network sender..." << std::endl;
        running_ = false;
        
        if (socket_ && socket_->is_open()) {
            socket_->close();
        }
        
        // Print final stats
        std::cout << "📊 Final stats: frames=" << stats_.frames_sent 
                  << " packets=" << stats_.packets_sent 
                  << " bytes=" << stats_.bytes_sent 
                  << " fps=" << std::fixed << std::setprecision(2) << stats_.fps_actual << std::endl;
    }
    
    // Get statistics
    Stats get_stats() const {
        return stats_;
    }
    
    bool is_running() const {
        return running_;
    }
};

} // namespace video_streaming::async
