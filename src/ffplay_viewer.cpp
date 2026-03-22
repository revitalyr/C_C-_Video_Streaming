#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <csignal>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

using namespace std::chrono_literals;

class FFplayViewer {
private:
    std::atomic<bool> m_running{false};
    std::thread m_receiver_thread;
    
    // Network
    int m_socket;
    struct sockaddr_in m_server_addr;
    
    // FFmpeg format context for stdout
    AVFormatContext* m_format_ctx = nullptr;
    
    // Metrics
    std::atomic<uint64_t> m_packets_received{0};
    std::atomic<uint64_t> m_bytes_received{0};
    
public:
    FFplayViewer(int port = 5000) {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        
        // Create UDP socket
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        
        // Bind to port
        m_server_addr.sin_family = AF_INET;
        m_server_addr.sin_addr.s_addr = INADDR_ANY;
        m_server_addr.sin_port = htons(port);
        
        if (bind(m_socket, (struct sockaddr*)&m_server_addr, sizeof(m_server_addr)) < 0) {
            std::cerr << "Failed to bind to port " << port << "\n";
            return;
        }
        
        initialize_format();
    }
    
    ~FFplayViewer() {
        stop();
        cleanup();
    }
    
    void start() {
        m_running.store(true);
        m_receiver_thread = std::thread(&FFplayViewer::receive_loop, this);
    }
    
    void stop() {
        m_running.store(false);
        if (m_receiver_thread.joinable()) {
            m_receiver_thread.join();
        }
    }
    
private:
    void initialize_format() {
        // Allocate output format context
        if (avformat_alloc_output_context2(&m_format_ctx, nullptr, "h264", nullptr) < 0) {
            std::cerr << "Could not create output context\n";
            return;
        }
        
        // Set output to stdout
        m_format_ctx->pb = avio_alloc_context(nullptr, 4096, 1, nullptr, nullptr, write_packet, nullptr);
        if (!m_format_ctx->pb) {
            std::cerr << "Could not create IO context\n";
            return;
        }
        
        // Write header to stdout
        if (avformat_write_header(m_format_ctx, nullptr) < 0) {
            std::cerr << "Could not write header\n";
        }
    }
    
    static int write_packet(void* opaque, uint8_t* buf, int buf_size) {
        // Write to stdout
        fwrite(buf, 1, buf_size, stdout);
        return buf_size;
    }
    
    void receive_loop() {
        std::cout << "📡 FFplay receiver started on port " << ntohs(m_server_addr.sin_port) << "\n";
        std::cout << "🎬 Pipe to ffplay: ./build/ffplay_viewer --port 5000 | ffplay -f h264 -\n\n";
        
        uint8_t buffer[65536];
        auto start_time = std::chrono::steady_clock::now();
        auto last_metrics_time = start_time;
        
        while (m_running.load()) {
            // Receive UDP packet
            ssize_t received = recvfrom(m_socket, (char*)buffer, sizeof(buffer), 0, nullptr, nullptr);
            
            if (received > 0) {
                // Write H.264 packet directly to stdout
                fwrite(buffer, 1, received, stdout);
                fflush(stdout);
                
                m_packets_received.fetch_add(1);
                m_bytes_received.fetch_add(received);
            }
            
            // Print metrics every 5 seconds (to stderr to not interfere with video stream)
            auto now = std::chrono::steady_clock::now();
            if (now - last_metrics_time >= 5s) {
                print_metrics(now - start_time);
                last_metrics_time = now;
            }
        }
    }
    
    void print_metrics(std::chrono::steady_clock::duration elapsed) {
        uint64_t packets = m_packets_received.load();
        uint64_t bytes = m_bytes_received.load();
        
        double pps = elapsed.count() > 0 ? (packets * 1000.0) / elapsed.count() : 0.0;
        double mbps = elapsed.count() > 0 ? (bytes * 8.0 / 1024 / 1024) / (elapsed.count() / 1000.0) : 0.0;
        
        // Write to stderr to not interfere with video stream
        fprintf(stderr, "📦 Packets: %lu | 📊 Rate: %.1f pps | 🌐 Bitrate: %.2f Mbps | 💾 Received: %s\n",
                packets, pps, mbps, format_bytes(bytes).c_str());
    }
    
    std::string format_bytes(uint64_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    
    void cleanup() {
        if (m_format_ctx) {
            av_write_trailer(m_format_ctx);
            if (m_format_ctx->pb) {
                avio_context_free(&m_format_ctx->pb);
            }
            avformat_free_context(m_format_ctx);
        }
        
        if (m_socket > 0) close(m_socket);
        
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

std::atomic<bool> g_shutdown{false};
void signal_handler(int signal) {
    fprintf(stderr, "\n🛑 Signal %d received, stopping viewer...\n", signal);
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    int port = 5000;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "FFplay Viewer - Pipe H.264 stream to ffplay\n";
            std::cout << "Usage: " << argv[0] << " [options] | ffplay -f h264 -\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>       UDP port to listen on (default: 5000)\n";
            std::cout << "Example:\n";
            std::cout << "  " << argv[0] << " --port 5000 | ffplay -f h264 -\n";
            std::cout << "  " << argv[0] << " --port 5000 > video.h264\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    fprintf(stderr, "🎬 Starting FFplay Viewer...\n");
    fprintf(stderr, "📡 Listening on port %d\n", port);
    
    FFplayViewer viewer(port);
    viewer.start();
    
    // Wait for shutdown
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(100ms);
    }
    
    viewer.stop();
    fprintf(stderr, "🎬 Viewer stopped.\n");
    
    return 0;
}
