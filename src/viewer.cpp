#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
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
#include <libswscale/swscale.h>
}

#ifdef ENABLE_SDL
#include <SDL2/SDL.h>
#endif

using namespace std::chrono_literals;

class VideoViewer {
private:
    std::atomic<bool> m_running{false};
    std::thread m_receiver_thread;
    std::thread m_decoder_thread;
    
    // Network
    int m_socket;
    struct sockaddr_in m_server_addr;
    
    // FFmpeg decoding
    AVCodecContext* m_codec_ctx = nullptr;
    AVFrame* m_frame = nullptr;
    AVFrame* m_rgb_frame = nullptr;
    AVPacket* m_packet = nullptr;
    SwsContext* m_sws_ctx = nullptr;
    
    // Packet buffer
    std::queue<std::vector<uint8_t>> m_packet_queue;
    std::mutex m_queue_mutex;
    
    // Metrics
    std::atomic<uint64_t> m_frames_received{0};
    std::atomic<uint64_t> m_frames_decoded{0};
    std::atomic<uint64_t> m_bytes_received{0};
    std::atomic<double> m_latency_ms{0.0};
    
    // SDL
#ifdef ENABLE_SDL
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;
#endif
    
public:
    VideoViewer(int port = 5000) {
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
        
        initialize_decoder();
        
#ifdef ENABLE_SDL
        initialize_sdl();
#endif
    }
    
    ~VideoViewer() {
        stop();
        cleanup();
    }
    
    void start() {
        m_running.store(true);
        m_receiver_thread = std::thread(&VideoViewer::receive_loop, this);
        m_decoder_thread = std::thread(&VideoViewer::decode_loop, this);
    }
    
    void stop() {
        m_running.store(false);
        if (m_receiver_thread.joinable()) {
            m_receiver_thread.join();
        }
        if (m_decoder_thread.joinable()) {
            m_decoder_thread.join();
        }
    }
    
private:
    void initialize_decoder() {
        // Find H.264 decoder
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) {
            std::cerr << "H.264 decoder not found\n";
            return;
        }
        
        // Create codec context
        m_codec_ctx = avcodec_alloc_context3(codec);
        if (!m_codec_ctx) {
            std::cerr << "Could not create codec context\n";
            return;
        }
        
        // Open decoder
        if (avcodec_open2(m_codec_ctx, codec, nullptr) < 0) {
            std::cerr << "Could not open codec\n";
            return;
        }
        
        // Allocate frame and packet
        m_frame = av_frame_alloc();
        m_rgb_frame = av_frame_alloc();
        m_packet = av_packet_alloc();
        
        if (!m_frame || !m_rgb_frame || !m_packet) {
            std::cerr << "Could not allocate frame or packet\n";
            return;
        }
    }
    
#ifdef ENABLE_SDL
    void initialize_sdl() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
            return;
        }
        
        m_window = SDL_CreateWindow("Video Stream Viewer", 
                                   SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   640, 480, SDL_WINDOW_SHOWN);
        if (!m_window) {
            std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
            return;
        }
        
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
            return;
        }
        
        m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_YV12,
                                     SDL_TEXTUREACCESS_STREAMING, 640, 480);
        if (!m_texture) {
            std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
            return;
        }
    }
#endif
    
    void receive_loop() {
        std::cout << "📡 Video receiver started on port " << ntohs(m_server_addr.sin_port) << "\n";
        
        uint8_t buffer[65536];
        
        while (m_running.load()) {
            // Receive UDP packet
            ssize_t received = recvfrom(m_socket, (char*)buffer, sizeof(buffer), 0, nullptr, nullptr);
            
            if (received > 0) {
                std::vector<uint8_t> packet_data(buffer, buffer + received);
                
                {
                    std::lock_guard<std::mutex> lock(m_queue_mutex);
                    m_packet_queue.push(packet_data);
                    if (m_packet_queue.size() > 100) { // Prevent queue overflow
                        m_packet_queue.pop();
                    }
                }
                
                m_frames_received.fetch_add(1);
                m_bytes_received.fetch_add(received);
            }
        }
    }
    
    void decode_loop() {
        std::cout << "🎬 Video decoder started\n";
        
        auto start_time = std::chrono::steady_clock::now();
        auto last_metrics_time = start_time;
        
        while (m_running.load()) {
            std::vector<uint8_t> packet_data;
            
            {
                std::lock_guard<std::mutex> lock(m_queue_mutex);
                if (m_packet_queue.empty()) {
                    std::this_thread::sleep_for(10ms);
                    continue;
                }
                packet_data = m_packet_queue.front();
                m_packet_queue.pop();
            }
            
            // Measure latency
            auto decode_start = std::chrono::steady_clock::now();
            
            // Decode packet
            m_packet->data = packet_data.data();
            m_packet->size = packet_data.size();
            
            int ret = avcodec_send_packet(m_codec_ctx, m_packet);
            if (ret < 0) {
                continue;
            }
            
            while (ret >= 0) {
                ret = avcodec_receive_frame(m_codec_ctx, m_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    continue;
                }
                
                // Frame decoded successfully
                m_frames_decoded.fetch_add(1);
                
                // Calculate latency
                auto decode_end = std::chrono::steady_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(decode_end - decode_start);
                m_latency_ms.store(latency.count() / 1000.0);
                
#ifdef ENABLE_SDL
                // Display frame
                display_frame();
#else
                // Print frame info
                if (m_frames_decoded.load() % 25 == 0) { // Every 25 frames
                    std::cout << "🎬 Frame " << m_frames_decoded.load() 
                              << " | Size: " << m_frame->width << "x" << m_frame->height
                              << " | PTS: " << m_frame->pts << "\n";
                }
#endif
                
                av_frame_unref(m_frame);
            }
            
            // Print metrics every 2 seconds
            auto now = std::chrono::steady_clock::now();
            if (now - last_metrics_time >= 2s) {
                print_metrics(now - start_time);
                last_metrics_time = now;
            }
        }
    }
    
#ifdef ENABLE_SDL
    void display_frame() {
        if (!m_renderer || !m_texture) return;
        
        // Update texture with frame data
        SDL_UpdateYUVTexture(m_texture, nullptr,
                           m_frame->data[0], m_frame->linesize[0],
                           m_frame->data[1], m_frame->linesize[1],
                           m_frame->data[2], m_frame->linesize[2]);
        
        // Clear renderer and copy texture
        SDL_RenderClear(m_renderer);
        SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
        SDL_RenderPresent(m_renderer);
    }
#endif
    
    void print_metrics(std::chrono::steady_clock::duration elapsed) {
        uint64_t received = m_frames_received.load();
        uint64_t decoded = m_frames_decoded.load();
        uint64_t bytes = m_bytes_received.load();
        double latency = m_latency_ms.load();
        
        double fps = elapsed.count() > 0 ? (decoded * 1000.0) / elapsed.count() : 0.0;
        double mbps = elapsed.count() > 0 ? (bytes * 8.0 / 1024 / 1024) / (elapsed.count() / 1000.0) : 0.0;
        double loss_rate = received > 0 ? ((received - decoded) * 100.0) / received : 0.0;
        
        std::cout << "📹 Received: " << received 
                  << " | 🎬 Decoded: " << decoded
                  << " | 🎬 FPS: " << std::fixed << std::setprecision(1) << fps
                  << " | 📊 Bitrate: " << std::setprecision(2) << mbps << " Mbps"
                  << " | ⏱️ Latency: " << std::setprecision(1) << latency << "ms"
                  << " | 📉 Loss: " << std::setprecision(1) << loss_rate << "%\n";
    }
    
    std::string format_bytes(uint64_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }
    
    void cleanup() {
        if (m_frame) av_frame_free(&m_frame);
        if (m_rgb_frame) av_frame_free(&m_rgb_frame);
        if (m_packet) av_packet_free(&m_packet);
        if (m_codec_ctx) avcodec_free_context(&m_codec_ctx);
        if (m_sws_ctx) sws_freeContext(m_sws_ctx);
        
#ifdef ENABLE_SDL
        if (m_texture) SDL_DestroyTexture(m_texture);
        if (m_renderer) SDL_DestroyRenderer(m_renderer);
        if (m_window) SDL_DestroyWindow(m_window);
        SDL_Quit();
#endif
        
        if (m_socket > 0) close(m_socket);
        
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

std::atomic<bool> g_shutdown{false};
void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping viewer...\n";
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
            std::cout << "Video Viewer - Real-time H.264 streaming receiver\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>       UDP port to listen on (default: 5000)\n";
            std::cout << "Example:\n";
            std::cout << "  " << argv[0] << " --port 5000\n";
            std::cout << "\nTo pipe to ffplay:\n";
            std::cout << "  " << argv[0] << " --port 5000 | ffplay -f h264 -\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "🎬 Starting Video Viewer...\n";
    std::cout << "📡 Listening on port " << port << "\n";
    
    VideoViewer viewer(port);
    viewer.start();
    
    // Wait for shutdown
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(100ms);
    }
    
    viewer.stop();
    std::cout << "🎬 Viewer stopped.\n";
    
    return 0;
}
