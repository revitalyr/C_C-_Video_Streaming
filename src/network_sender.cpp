#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <random>
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
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

using namespace std::chrono_literals;

class NetworkSimulator {
private:
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_dist;
    
    // Network parameters
    double m_packet_loss_rate = 0.0;  // 0-100%
    int m_network_delay_ms = 0;       // 0-1000ms
    int m_jitter_ms = 0;              // 0-200ms
    
public:
    NetworkSimulator(double loss = 0.0, int delay = 0, int jitter = 0)
        : m_rng(std::random_device{}()), m_dist(0.0, 100.0),
          m_packet_loss_rate(loss), m_network_delay_ms(delay), m_jitter_ms(jitter) {}
    
    bool should_drop_packet() {
        if (m_packet_loss_rate <= 0.0) return false;
        return m_dist(m_rng) < m_packet_loss_rate;
    }
    
    int get_delay() {
        if (m_network_delay_ms == 0 && m_jitter_ms == 0) return 0;
        
        std::uniform_int_distribution<int> jitter_dist(-m_jitter_ms, m_jitter_ms);
        int jitter = jitter_dist(m_rng);
        return std::max(0, m_network_delay_ms + jitter);
    }
    
    void print_status() {
        std::cout << "🌐 Network Simulation:\n";
        std::cout << "  📉 Packet Loss: " << m_packet_loss_rate << "%\n";
        std::cout << "  ⏱️  Delay: " << m_network_delay_ms << "ms\n";
        std::cout << "  🔄 Jitter: " << m_jitter_ms << "ms\n";
    }
};

class VideoSender {
private:
    std::atomic<bool> m_running{false};
    std::thread m_sender_thread;
    
    // Network
    int m_socket;
    struct sockaddr_in m_client_addr;
    NetworkSimulator m_network;
    
    // FFmpeg encoding
    AVCodecContext* m_codec_ctx = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
    
    // Metrics
    std::atomic<uint64_t> m_frames_sent{0};
    std::atomic<uint64_t> m_bytes_sent{0};
    std::atomic<uint64_t> m_packets_dropped{0};
    
public:
    VideoSender(const std::string& client_ip = "127.0.0.1", int port = 5000,
                double loss = 0.0, int delay = 0, int jitter = 0)
        : m_network(loss, delay, jitter) {
        
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        
        // Create UDP socket
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        
        m_client_addr.sin_family = AF_INET;
        m_client_addr.sin_port = htons(port);
        inet_pton(AF_INET, client_ip.c_str(), &m_client_addr.sin_addr);
        
        initialize_encoder();
    }
    
    ~VideoSender() {
        stop();
        cleanup();
    }
    
    void start() {
        m_running.store(true);
        m_sender_thread = std::thread(&VideoSender::send_loop, this);
    }
    
    void stop() {
        m_running.store(false);
        if (m_sender_thread.joinable()) {
            m_sender_thread.join();
        }
    }
    
private:
    void initialize_encoder() {
        // Find H.264 encoder
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            std::cerr << "H.264 encoder not found\n";
            return;
        }
        
        // Create codec context
        m_codec_ctx = avcodec_alloc_context3(codec);
        if (!m_codec_ctx) {
            std::cerr << "Could not create codec context\n";
            return;
        }
        
        // Set encoding parameters
        m_codec_ctx->codec_id = AV_CODEC_ID_H264;
        m_codec_ctx->bit_rate = 400000;
        m_codec_ctx->width = 640;
        m_codec_ctx->height = 480;
        m_codec_ctx->time_base = {1, 25};
        m_codec_ctx->framerate = {25, 1};
        m_codec_ctx->gop_size = 10;
        m_codec_ctx->max_b_frames = 1;
        m_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        
        // Ultrafast preset for low latency
        av_opt_set(m_codec_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(m_codec_ctx->priv_data, "tune", "zerolatency", 0);
        
        // Open encoder
        if (avcodec_open2(m_codec_ctx, codec, nullptr) < 0) {
            std::cerr << "Could not open codec\n";
            return;
        }
        
        // Allocate frame and packet
        m_frame = av_frame_alloc();
        m_packet = av_packet_alloc();
        
        if (!m_frame || !m_packet) {
            std::cerr << "Could not allocate frame or packet\n";
            return;
        }
        
        // Set frame parameters
        m_frame->format = m_codec_ctx->pix_fmt;
        m_frame->width = m_codec_ctx->width;
        m_frame->height = m_codec_ctx->height;
        
        if (av_frame_get_buffer(m_frame, 0) < 0) {
            std::cerr << "Could not allocate frame data\n";
            return;
        }
    }
    
    void generate_test_frame() {
        static uint64_t frame_counter = 0;
        frame_counter++;
        
        // Fill Y plane with animated pattern
        for (int y = 0; y < m_codec_ctx->height; y++) {
            for (int x = 0; x < m_codec_ctx->width; x++) {
                uint8_t value = (x + y + frame_counter * 2) % 256;
                m_frame->data[0][y * m_frame->linesize[0] + x] = value;
            }
        }
        
        // Fill UV planes with color changes
        for (int y = 0; y < m_codec_ctx->height / 2; y++) {
            for (int x = 0; x < m_codec_ctx->width / 2; x++) {
                uint8_t u_val = 128 + sin(frame_counter * 0.1) * 50;
                uint8_t v_val = 128 + cos(frame_counter * 0.1) * 50;
                m_frame->data[1][y * m_frame->linesize[1] + x] = u_val;
                m_frame->data[2][y * m_frame->linesize[2] + x] = v_val;
            }
        }
        
        m_frame->pts = frame_counter;
    }
    
    void send_loop() {
        std::cout << "🎬 Network Video Sender started\n";
        m_network.print_status();
        std::cout << "📡 Sending to " << inet_ntoa(m_client_addr.sin_addr) 
                  << ":" << ntohs(m_client_addr.sin_port) << "\n\n";
        
        auto start_time = std::chrono::steady_clock::now();
        auto last_metrics_time = start_time;
        
        while (m_running.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Generate test frame
            generate_test_frame();
            
            // Encode frame
            int ret = avcodec_send_frame(m_codec_ctx, m_frame);
            if (ret < 0) {
                std::cerr << "Error sending frame to encoder\n";
                continue;
            }
            
            while (ret >= 0) {
                ret = avcodec_receive_packet(m_codec_ctx, m_packet);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error during encoding\n";
                    break;
                }
                
                // Apply network simulation
                if (m_network.should_drop_packet()) {
                    m_packets_dropped.fetch_add(1);
                    av_packet_unref(m_packet);
                    continue;
                }
                
                // Apply network delay
                int delay = m_network.get_delay();
                if (delay > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                }
                
                // Send packet via UDP
                ssize_t sent = sendto(m_socket, m_packet->data, m_packet->size, 0,
                                     (struct sockaddr*)&m_client_addr, sizeof(m_client_addr));
                
                if (sent > 0) {
                    m_frames_sent.fetch_add(1);
                    m_bytes_sent.fetch_add(sent);
                }
                
                av_packet_unref(m_packet);
            }
            
            // Print metrics every 2 seconds
            if (now - last_metrics_time >= 2s) {
                print_metrics(now - start_time);
                last_metrics_time = now;
            }
            
            std::this_thread::sleep_for(40ms); // 25 FPS
        }
    }
    
    void print_metrics(std::chrono::steady_clock::duration elapsed) {
        uint64_t frames = m_frames_sent.load();
        uint64_t dropped = m_packets_dropped.load();
        uint64_t bytes = m_bytes_sent.load();
        
        double fps = elapsed.count() > 0 ? (frames * 1000.0) / elapsed.count() : 0.0;
        double mbps = elapsed.count() > 0 ? (bytes * 8.0 / 1024 / 1024) / (elapsed.count() / 1000.0) : 0.0;
        double loss_rate = (frames + dropped) > 0 ? (dropped * 100.0) / (frames + dropped) : 0.0;
        
        std::cout << "📹 Frames: " << frames 
                  << " | 🎬 FPS: " << std::fixed << std::setprecision(1) << fps
                  << " | 📊 Bitrate: " << std::setprecision(2) << mbps << " Mbps"
                  << " | 💾 Sent: " << format_bytes(bytes)
                  << " | 📉 Loss: " << std::setprecision(1) << loss_rate << "%\n";
    }
    
    std::string format_bytes(uint64_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    
    void cleanup() {
        if (m_frame) av_frame_free(&m_frame);
        if (m_packet) av_packet_free(&m_packet);
        if (m_codec_ctx) avcodec_free_context(&m_codec_ctx);
        if (m_socket > 0) close(m_socket);
        
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

std::atomic<bool> g_shutdown{false};
void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping sender...\n";
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    std::string client_ip = "127.0.0.1";
    int port = 5000;
    double loss = 0.0;
    int delay = 0;
    int jitter = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--client" && i + 1 < argc) {
            client_ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--loss" && i + 1 < argc) {
            loss = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            delay = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            jitter = std::stoi(argv[++i]);
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
    
    std::cout << "🎬 Starting Network Video Sender...\n";
    std::cout << "📡 Client: " << client_ip << ":" << port << "\n";
    
    VideoSender sender(client_ip, port, loss, delay, jitter);
    sender.start();
    
    // Wait for shutdown
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(100ms);
    }
    
    sender.stop();
    std::cout << "🎬 Sender stopped.\n";
    
    return 0;
}
