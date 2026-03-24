#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

import video_streaming.sender;
import video_streaming.logger;
import video_streaming.media.frame;

using namespace std::chrono_literals;
using namespace video_streaming;

std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping sender..." << std::endl;
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    VideoSender::Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--loss" && i + 1 < argc) {
            config.packet_loss = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            config.delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            config.jitter_ms = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Simple Video Sender\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>      UDP port (default: 5000)\n";
            std::cout << "  --loss <percent>   Packet loss rate 0-100 (default: 0)\n";
            std::cout << "  --delay <ms>       Network delay 0-1000ms (default: 0)\n";
            std::cout << "  --jitter <ms>      Network jitter 0-200ms (default: 0)\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        std::cout << "🎬 Starting Simple Video Sender with Simulation..." << std::endl;
        std::cout << "🌐 Network Simulation Config:" << std::endl;
        std::cout << "  📉 Packet Loss: " << config.packet_loss << "%" << std::endl;
        std::cout << "  ⏱️  Delay: " << config.delay_ms << "ms" << std::endl;
        std::cout << "  🔄 Jitter: " << config.jitter_ms << "ms" << std::endl;
        std::cout << "  📡 Destination: " << config.destination_ip << ":" << config.port << std::endl;
        std::cout << std::endl;
        
        VideoSender sender(config);
        
        if (!sender.start()) {
            std::cerr << "❌ Failed to start sender" << std::endl;
            return 1;
        }
        
        // Run until shutdown
        while (!g_shutdown.load()) {
            std::this_thread::sleep_for(100ms);
            
            // Print stats every 2 seconds
            static auto last_stats = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats >= 2s) {
                auto stats = sender.get_stats();
                std::cout << "📤 Sent: " << stats.frames_sent << " frames, " 
                          << stats.packets_sent << " packets, " 
                          << std::fixed << std::setprecision(1) << stats.fps_actual << " fps" << std::endl;
                last_stats = now;
            }
        }
        
        sender.stop();
        std::cout << "🎬 Sender stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
