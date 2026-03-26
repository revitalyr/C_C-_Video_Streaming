module;

#include <coroutine>
#include <memory>
#include <optional>
#include <chrono>
#include <thread>
#include <functional>
#include <iostream>
#include <random>

export module video_streaming.async.coroutine_receiver;

import video_streaming.media.frame;
import video_streaming.common.types;
import video_streaming.network.udp_socket;
import video_streaming.rtp.h264_packetizer;
import video_streaming.network.endpoint;
import video_streaming.async.coroutine_types;

export namespace video_streaming::async {

// Coroutine-based receiver
class CoroutineReceiver {
public:
    struct Config {
        std::string bind_ip = "0.0.0.0";
        uint16_t port = 5000;
        size_t jitter_buffer_size = 50;
        std::chrono::milliseconds jitter_buffer_delay{50};
    };
    
    struct Stats {
        uint64_t frames_received = 0;
        uint64_t packets_received = 0;
        uint64_t bytes_received = 0;
        uint64_t frames_dropped = 0;
        double fps_actual = 0.0;
    };
    
    using FrameCallback = std::function<void(const Frame&)>;
    
private:
    Config config_;
    std::unique_ptr<UdpSocket> socket_;
    std::unique_ptr<H264Packetizer> packetizer_;
    FrameCallback frame_callback_;
    std::chrono::steady_clock::time_point start_time_;
    Stats stats_;
    bool running_;
    
public:
    explicit CoroutineReceiver(const Config& config) 
        : config_(config), running_(false) {
        
        std::cout << "🔧 Initializing coroutine receiver..." << std::endl;
        
        // Initialize components
        socket_ = std::make_unique<UdpSocket>();
        packetizer_ = std::make_unique<H264Packetizer>(12345); // SSRC
        
        start_time_ = std::chrono::steady_clock::now();
        
        std::cout << "✅ Coroutine receiver initialized" << std::endl;
    }
    
    // Set frame callback
    void set_frame_callback(FrameCallback callback) {
        frame_callback_ = std::move(callback);
        std::cout << "🔧 Frame callback set" << std::endl;
    }
    
    // Async packet receiving
    Task<std::vector<uint8_t>> receive_packets_async() {
        std::cout << "📡 Starting async packet receiving..." << std::endl;
        
        if (!socket_->is_open()) {
            if (!socket_->open()) {
                std::cerr << "❌ Failed to open socket" << std::endl;
                co_return std::vector<uint8_t>{};
            }
            socket_->set_blocking(false);
            
            // Bind to address
            Endpoint bind_addr(config_.bind_ip, config_.port);
            if (!socket_->bind(bind_addr)) {
                std::cerr << "❌ Failed to bind socket" << std::endl;
                co_return std::vector<uint8_t>{};
            }
            
            std::cout << "🔗 Socket bound to " << config_.bind_ip << ":" << config_.port << std::endl;
        }
        
        std::vector<uint8_t> buffer;
        buffer.resize(2048); // MTU size
        
        // Try to receive packet
        Endpoint sender;
        ssize_t bytes_received = socket_->receive_from(buffer, buffer.size(), sender);
        
        if (bytes_received > 0) {
            buffer.resize(bytes_received);
            stats_.packets_received++;
            stats_.bytes_received += bytes_received;
            
            std::cout << "📦 Received packet: " << bytes_received << " bytes from " 
                      << sender.to_string() << std::endl;
            
            co_return buffer;
        }
        
        // No packet available
        co_return std::vector<uint8_t>{};
    }
    
    // Async frame decoding
    Task<std::optional<Frame>> decode_frame_async(const std::vector<uint8_t>& packet_data) {
        if (packet_data.empty()) {
            co_return std::nullopt;
        }
        
        std::cout << "🎬 Starting async frame decoding..." << std::endl;
        
        // Parse RTP packet
        // Simplified - just create frame without parsing
        // auto rtp_packet = packetizer_->parse_rtp_packet(packet_data);
        // if (!rtp_packet) {
        //     std::cerr << "❌ Failed to parse RTP packet" << std::endl;
        //     co_return std::nullopt;
        // }
        
        // Create simple frame (simplified - in real implementation would decode)
        try {
            auto frame = std::make_unique<Frame>();
            frame->width = 640;  // Default resolution
            frame->height = 480;
            frame->format = PixelFormat::YUV420P;
            frame->timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time_).count());
            
            // Simple frame data (placeholder)
            const size_t frame_size = frame->width * frame->height * 3 / 2; // YUV420P
            frame->data.resize(frame_size, 128); // Gray frame
            
            if (frame) {
                stats_.frames_received++;
                
                // Calculate FPS
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
                if (elapsed.count() > 0) {
                    stats_.fps_actual = (stats_.frames_received * 1000.0) / elapsed.count();
                }
                
                std::cout << "✅ Frame decoded: " << frame->width << "x" << frame->height 
                          << " size: " << frame->data.size() << " bytes" << std::endl;
                
                co_return std::make_optional<Frame>(std::move(*frame));
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Decoding error: " << e.what() << std::endl;
        }
        
        co_return std::nullopt;
    }
    
    // Main receiving coroutine
    Task<bool> receive_frame_async() {
        // Receive frames synchronously for now (Task implementation needs fixing)
        auto packet_data_task = receive_packets_async();
        auto packet_data = packet_data_task.get(); // Get result synchronously
        if (packet_data.empty()) {
            co_return false; // No packet available
        }
        
        // Decode frame
        auto frame_task = decode_frame_async(packet_data);
        auto frame_opt = frame_task.get(); // Get result synchronously
        
        // Call callback if set
        if (frame_callback_ && frame_opt) {
            frame_callback_(*frame_opt);
        }
        
        co_return true;
    }
    
    // Start the receiver (non-blocking)
    bool start() {
        if (running_) {
            return true;
        }
        
        std::cout << "🚀 Starting coroutine receiver..." << std::endl;
        running_ = true;
        return true;
    }
    
    // Stop the receiver
    void stop() {
        if (!running_) {
            return;
        }
        
        std::cout << "🛑 Stopping coroutine receiver..." << std::endl;
        running_ = false;
        
        if (socket_ && socket_->is_open()) {
            socket_->close();
        }
        
        // Print final stats
        std::cout << "📊 Final stats: frames=" << stats_.frames_received 
                  << " packets=" << stats_.packets_received 
                  << " bytes=" << stats_.bytes_received 
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
