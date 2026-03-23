#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <csignal>

import video_streaming.sender;

using namespace std::chrono_literals;
using namespace video_streaming;

std::atomic<bool> g_shutdown{false};
void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping sender...\n";
    g_shutdown.store(true);
}

void print_status_header(const VideoSender::Config& config) {
    std::cout << "🌐 Network Simulation Config:\n";
    std::cout << "  📉 Packet Loss: " << config.packet_loss << "%\n";
    std::cout << "  ⏱️  Delay: " << config.delay_ms << "ms\n";
    std::cout << "  🔄 Jitter: " << config.jitter_ms << "ms\n";
    std::cout << "  📡 Destination: " << config.destination_ip << ":" << config.port << "\n\n";
}

int main(int argc, char* argv[]) {
    VideoSender::Config config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--client" && i + 1 < argc) {
            config.destination_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--loss" && i + 1 < argc) {
            config.packet_loss = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            config.delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            config.jitter_ms = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Network Video Sender - Real-time H.264 streaming with network simulation\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --client <ip>      Client IP address (default: 127.0.0.1)\n";
            std::cout << "  --port <port>      UDP port (default: 5000)\n";
            std::cout << "  --loss <percent>   Packet loss rate 0-100 (default: 0)\n";
            std::cout << "  --delay <ms>       Network delay 0-1000ms (default: 0)\n";
            std::cout << "  --jitter <ms>      Network jitter 0-200ms (default: 0)\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << "                                    # Perfect network\n";
            std::cout << "  " << argv[0] << " --loss 5                           # 5% packet loss\n";
            std::cout << "  " << argv[0] << " --loss 10 --delay 100 --jitter 30 # Poor network\n";
            std::cout << "  " << argv[0] << " --loss 20 --delay 200 --jitter 50 # Terrible network\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "🎬 Starting Core Video Sender with Simulation...\n";
    print_status_header(config);
    
    try {
        VideoSender sender(config);
        if (!sender.start()) {
            std::cerr << "❌ Failed to start VideoSender\n";
            return 1;
        }
    
        // Wait for shutdown
        while (!g_shutdown.load()) {
            auto stats = sender.get_stats();
            // Simple console heartbeat if needed, but VideoSender logs via spdlog
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        sender.stop();
        std::cout << "🎬 Sender stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
