#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

import video_streaming.receiver;
import video_streaming.logger;
import video_streaming.media.frame;

using namespace std::chrono_literals;
using namespace video_streaming;

std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping viewer..." << std::endl;
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    VideoReceiver::Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Simple Video Viewer\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>      UDP port (default: 5000)\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        std::cout << "🎬 Starting Simple Video Viewer..." << std::endl;
        std::cout << "📡 Listening on port " << config.port << std::endl;
        std::cout << std::endl;
        
        VideoReceiver receiver(config);
        
        // Set frame callback
        receiver.set_frame_callback([](const Frame& frame) {
            std::cout << "📹 Received frame: " << frame.width << "x" << frame.height 
                      << " size: " << frame.data.size() << " bytes" << std::endl;
        });
        
        if (!receiver.start()) {
            std::cerr << "❌ Failed to start receiver" << std::endl;
            return 1;
        }
        
        // Run until shutdown
        auto receiver_start = std::chrono::steady_clock::now();
        while (!g_shutdown.load()) {
            std::this_thread::sleep_for(100ms);
            
            // Print stats every 2 seconds
            static auto last_stats = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats >= 2s) {
                auto stats = receiver.get_stats();
                double loss_rate = stats.packets_received > 0 ? 
                    ((double)(stats.packets_received - stats.frames_decoded) / stats.packets_received) * 100.0 : 0.0;
                
                auto elapsed_seconds = std::max(1L, std::chrono::duration_cast<std::chrono::seconds>(now - receiver_start).count());
                double mbps = (stats.bytes_received * 8.0 / 1024 / 1024) / elapsed_seconds;
                
                std::cout << "📹 Pkts Rcvd: " << stats.packets_received 
                          << " | 🎬 Frames Decoded: " << stats.frames_decoded 
                          << " | 🎬 FPS: " << std::fixed << std::setprecision(1) << stats.fps_actual
                          << " | 📊 Bitrate: " << std::setprecision(2) << mbps << " Mbps"
                          << " | 📉 Loss: " << std::setprecision(2) << loss_rate << "%" << std::endl;
                last_stats = now;
            }
        }
        
        receiver.stop();
        std::cout << "🎬 Viewer stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
