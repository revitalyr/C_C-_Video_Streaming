#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <signal.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <signal.h>
#endif

using namespace std::chrono_literals;

// Simple visual demo with real-time metrics
class VisualDemo {
private:
    std::atomic<bool> m_running{false};
    std::thread m_demo_thread;
    std::atomic<uint64_t> m_frames_sent{0};
    std::atomic<uint64_t> m_bytes_sent{0};
    std::atomic<uint64_t> m_packets_lost{0};
    std::atomic<double> m_latency_ms{0.0};
    
    // Network simulation parameters
    double m_packet_loss_rate = 0.0;  // 0-100%
    int m_network_delay_ms = 0;       // 0-1000ms
    int m_jitter_ms = 0;              // 0-200ms
    
public:
    VisualDemo(double packet_loss = 0.0, int delay = 0, int jitter = 0)
        : m_packet_loss_rate(packet_loss), m_network_delay_ms(delay), m_jitter_ms(jitter) {}
    
    void start() {
        m_running.store(true);
        m_demo_thread = std::thread(&VisualDemo::demo_loop, this);
    }
    
    void stop() {
        m_running.store(false);
        if (m_demo_thread.joinable()) {
            m_demo_thread.join();
        }
    }
    
private:
    void demo_loop() {
        auto start_time = std::chrono::steady_clock::now();
        auto last_metrics_time = start_time;
        
        std::cout << "\n";
        std::cout << "🎬 === VISUAL VIDEO STREAMING DEMO ===\n";
        std::cout << "📡 Network: loss=" << m_packet_loss_rate 
                  << "%, delay=" << m_network_delay_ms 
                  << "ms, jitter=" << m_jitter_ms << "ms\n";
        std::cout << "🎥 Status: Starting...\n\n";
        
        while (m_running.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Simulate frame generation and sending
            simulate_frame_sending();
            
            // Update metrics every second
            if (now - last_metrics_time >= 1s) {
                update_metrics();
                print_visual_status();
                last_metrics_time = now;
            }
            
            std::this_thread::sleep_for(40ms); // ~25 FPS
        }
    }
    
    void simulate_frame_sending() {
        // Simulate H.264 frame (SPS/PPS/IDR)
        std::vector<uint8_t> frame = generate_demo_frame();
        
        // Apply network conditions
        if (should_drop_packet()) {
            m_packets_lost.fetch_add(1);
            return;
        }
        
        // Apply network delay and jitter
        int actual_delay = m_network_delay_ms + (rand() % (2 * m_jitter_ms + 1)) - m_jitter_ms;
        if (actual_delay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(actual_delay));
        }
        
        // Update statistics
        m_frames_sent.fetch_add(1);
        m_bytes_sent.fetch_add(frame.size());
        
        // Simulate latency measurement
        double latency = 10.0 + (rand() % 50) + (m_jitter_ms * 0.5);
        m_latency_ms.store(latency);
    }
    
    std::vector<uint8_t> generate_demo_frame() {
        static uint32_t frame_id = 0;
        frame_id++;
        
        std::vector<uint8_t> frame;
        
        // SPS (Sequence Parameter Set)
        frame.push_back(0x00); frame.push_back(0x00); frame.push_back(0x00); frame.push_back(0x01);
        frame.push_back(0x67); frame.push_back(0x42); frame.push_back(0x00); frame.push_back(0x1E);
        frame.push_back(0x8D); frame.push_back(0x40); frame.push_back(0x50); frame.push_back(0x17);
        frame.push_back(0xFC); frame.push_back(0xB0); frame.push_back(0x0F); frame.push_back(0x08);
        frame.push_back(0x84);
        
        // PPS (Picture Parameter Set)
        frame.push_back(0x00); frame.push_back(0x00); frame.push_back(0x00); frame.push_back(0x01);
        frame.push_back(0x68); frame.push_back(0xCE); frame.push_back(0x3C); frame.push_back(0x80);
        
        // IDR Frame (simulated video content)
        frame.push_back(0x00); frame.push_back(0x00); frame.push_back(0x00); frame.push_back(0x01);
        frame.push_back(0x65); frame.push_back(0x88); frame.push_back(0x80); frame.push_back(0x00);
        frame.push_back(0x05); frame.push_back(0xFF); frame.push_back(0xEE); frame.push_back(0x3D);
        
        // Add some "video data" that changes each frame
        for (int i = 0; i < 100; ++i) {
            frame.push_back(static_cast<uint8_t>((frame_id * 7 + i * 13) % 256));
        }
        
        return frame;
    }
    
    bool should_drop_packet() {
        if (m_packet_loss_rate <= 0.0) return false;
        return (rand() % 1000) < (m_packet_loss_rate * 10.0);
    }
    
    void update_metrics() {
        // Calculate packet loss percentage
        uint64_t total = m_frames_sent.load() + m_packets_lost.load();
        double loss_percent = total > 0 ? (m_packets_lost.load() * 100.0 / total) : 0.0;
        
        // Update latency with some variation
        double base_latency = 10.0 + m_network_delay_ms * 0.1;
        double jitter_effect = (rand() % (m_jitter_ms + 1)) * 0.5;
        m_latency_ms.store(base_latency + jitter_effect);
    }
    
    void print_visual_status() {
        // Clear screen (Windows compatible)
        system("cls");
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_start_time).count();
        
        uint64_t frames = m_frames_sent.load();
        uint64_t bytes = m_bytes_sent.load();
        uint64_t lost = m_packets_lost.load();
        double latency = m_latency_ms.load();
        
        double fps = elapsed > 0 ? static_cast<double>(frames) / elapsed : 0.0;
        double mbps = elapsed > 0 ? (bytes * 8.0 / 1024 / 1024) / elapsed : 0.0;
        double loss_percent = (frames + lost) > 0 ? (lost * 100.0 / (frames + lost)) : 0.0;
        
        std::cout << "\n";
        std::cout << "🎬 === VISUAL VIDEO STREAMING DEMO ===\n";
        std::cout << "⏱️  Runtime: " << elapsed << "s\n";
        std::cout << "📡 Network: loss=" << m_packet_loss_rate 
                  << "%, delay=" << m_network_delay_ms 
                  << "ms, jitter=" << m_jitter_ms << "ms\n\n";
        
        // Video Stream Status
        std::cout << "🎥 VIDEO STREAM STATUS\n";
        std::cout << "┌─────────────────────────────────────────┐\n";
        std::cout << "│ 📹 Frames Sent: " << std::setw(25) << std::left << frames << "│\n";
        std::cout << "│ 🎬 FPS:        " << std::setw(25) << std::fixed << std::setprecision(1) << fps << "│\n";
        std::cout << "│ 📊 Bitrate:    " << std::setw(25) << std::fixed << std::setprecision(2) << mbps << " Mbps│\n";
        std::cout << "│ 💾 Data Sent:   " << std::setw(25) << format_bytes(bytes) << "│\n";
        std::cout << "└─────────────────────────────────────────┘\n\n";
        
        // Network Performance
        std::cout << "🌐 NETWORK PERFORMANCE\n";
        std::cout << "┌─────────────────────────────────────────┐\n";
        std::cout << "│ 📉 Packet Loss: " << std::setw(22) << std::fixed << std::setprecision(1) << loss_percent << "% │\n";
        std::cout << "│ 📦 Lost:        " << std::setw(25) << lost << "│\n";
        std::cout << "│ ⏱️  Latency:     " << std::setw(25) << std::fixed << std::setprecision(1) << latency << " ms │\n";
        std::cout << "└─────────────────────────────────────────┘\n\n";
        
        // Visual Video Representation
        std::cout << "🎬 VIDEO PREVIEW (640x480)\n";
        std::cout << "┌─────────────────────────────────────────┐\n";
        for (int row = 0; row < 8; ++row) {
            std::cout << "│";
            for (int col = 0; col < 40; ++col) {
                // Create animated pattern based on frame number
                uint8_t pixel = static_cast<uint8_t>((frames * 3 + row * 17 + col * 7) % 256);
                char visual = get_visual_char(pixel);
                std::cout << visual;
            }
            std::cout << "│\n";
        }
        std::cout << "└─────────────────────────────────────────┘\n\n";
        
        // Pipeline Status
        std::cout << "🔄 PIPELINE STATUS\n";
        std::cout << "┌─────────────────────────────────────────┐\n";
        std::cout << "│ 📹 Capture:     " << std::setw(23) << "✅ Active" << "│\n";
        std::cout << "│ 🎬 Encode:      " << std::setw(23) << "✅ H.264" << "│\n";
        std::cout << "│ 📦 RTP:         " << std::setw(23) << "✅ FU-A" << "│\n";
        std::cout << "│ 🌐 Network:     " << std::setw(23) << get_network_status() << "│\n";
        std::cout << "│ 📊 Jitter Buf:  " << std::setw(23) << "✅ Active" << "│\n";
        std::cout << "│ 🎮 Decode:      " << std::setw(23) << "✅ H.264" << "│\n";
        std::cout << "│ 🖥️  Render:     " << std::setw(23) << "✅ SDL" << "│\n";
        std::cout << "└─────────────────────────────────────────┘\n\n";
        
        std::cout << "Press Ctrl+C to stop demo...\n";
    }
    
    char get_visual_char(uint8_t value) {
        // Map pixel values to visual characters
        const char* chars = " .:-=+*#%@";
        int index = (value * 9) / 256;
        return chars[index];
    }
    
    std::string format_bytes(uint64_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    
    std::string get_network_status() {
        double loss = (m_frames_sent.load() + m_packets_lost.load()) > 0 ? 
                     (m_packets_lost.load() * 100.0 / (m_frames_sent.load() + m_packets_lost.load())) : 0.0;
        
        if (loss < 1.0) return "✅ Good";
        if (loss < 5.0) return "⚠️  Fair";
        return "❌ Poor";
    }
    
    std::chrono::steady_clock::time_point m_start_time = std::chrono::steady_clock::now();
};

// Signal handler for graceful shutdown
std::atomic<bool> g_shutdown{false};
void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", stopping demo...\n";
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    double packet_loss = 0.0;
    int delay = 0;
    int jitter = 0;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--loss" && i + 1 < argc) {
            packet_loss = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            delay = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            jitter = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Visual Video Streaming Demo\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --loss <percent>    Packet loss rate (0-100)\n";
            std::cout << "  --delay <ms>        Network delay (0-1000)\n";
            std::cout << "  --jitter <ms>       Network jitter (0-200)\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << "                           # Perfect network\n";
            std::cout << "  " << argv[0] << " --loss 5                  # 5% packet loss\n";
            std::cout << "  " << argv[0] << " --loss 10 --delay 100    # Poor network\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "🎬 Starting Visual Video Streaming Demo...\n";
    std::cout << "📡 Network conditions: loss=" << packet_loss 
              << "%, delay=" << delay << "ms, jitter=" << jitter << "ms\n";
    
    // Create and start demo
    VisualDemo demo(packet_loss, delay, jitter);
    demo.start();
    
    // Wait for shutdown
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(100ms);
    }
    
    demo.stop();
    std::cout << "🎬 Demo stopped. Thanks for watching!\n";
    
    return 0;
}
