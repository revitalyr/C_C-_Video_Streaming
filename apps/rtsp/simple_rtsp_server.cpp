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
#include <csignal>
#include <cstring>

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
#include <sys/time.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavdevice/avdevice.h>
}

using namespace std::chrono_literals;

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping server...\n";
    g_shutdown.store(true);
}

class SocketManager {
public:
    static bool initialize() {
#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        return result == 0;
#else
        return true;
#endif
    }
    
    static void cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    static void close_socket(int socket) {
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
    }
};

class SimpleRTSPServer {
public:
    struct Config {
        std::string bind_address = "0.0.0.0";
        uint16_t rtsp_port = 8554;
        int fps = 30;
        int bitrate = 1000000;
        int width = 640;
        int height = 480;
        bool enable_logging = true;
    };

    struct ClientSession {
        std::string client_ip;
        bool playing = false;
        bool is_tcp = true;
        
        uint16_t client_rtp_port = 0;
        uint16_t client_rtcp_port = 0;
        int server_rtp_socket = -1;
        int server_rtcp_socket = -1;
        uint16_t server_rtp_port = 0;
        uint16_t server_rtcp_port = 0;
    };

private:
    Config m_config;
    std::atomic<bool> m_running{false};
    std::thread m_server_thread;
    std::thread m_streaming_thread;
    int m_server_socket = -1;
    
    std::vector<int> m_client_sockets;
    std::map<int, ClientSession> m_sessions;
    mutable std::mutex m_clients_mutex;
    
    std::atomic<uint16_t> m_rtp_sequence{0};
    std::atomic<uint32_t> m_rtp_timestamp{0};
    const uint32_t m_rtp_ssrc = 12345;
    std::atomic<int> m_active_threads{0};

    // FFmpeg components
    AVCodecContext* m_enc_ctx = nullptr;
    AVFilterGraph* m_filter_graph = nullptr;
    AVFilterContext* m_buf_sink_ctx = nullptr;
    AVFilterContext* m_buf_src_ctx = nullptr;
    
    std::string m_sps_base64;
    std::string m_pps_base64;

public:
    SimpleRTSPServer(const Config& config) : m_config(config) {
        avdevice_register_all();
    }

    ~SimpleRTSPServer() {
        stop();
        if (m_filter_graph) avfilter_graph_free(&m_filter_graph);
        if (m_enc_ctx) avcodec_free_context(&m_enc_ctx);
    }

    bool start() {
        if (m_running.load()) return false;

        // Initialize Network
        if (!SocketManager::initialize()) {
            std::cerr << "Failed to initialize socket library" << std::endl;
            return false;
        }

        // Initialize FFmpeg Encoder
        const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
        if (!codec) {
            std::cerr << "❌ libx264 not found" << std::endl;
            return false;
        }

        m_enc_ctx = avcodec_alloc_context3(codec);
        m_enc_ctx->height = m_config.height;
        m_enc_ctx->width = m_config.width;
        m_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        m_enc_ctx->time_base = {1, m_config.fps}; // Time base for timestamp
        m_enc_ctx->framerate = {m_config.fps, 1};  // Target framerate
        m_enc_ctx->bit_rate = m_config.bitrate;
        m_enc_ctx->gop_size = m_config.fps; // 1 keyframe per second
        m_enc_ctx->max_b_frames = 0; // Low latency
        
        av_opt_set(m_enc_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(m_enc_ctx->priv_data, "tune", "zerolatency", 0);
        // Force global header to extract SPS/PPS in extradata
        m_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(m_enc_ctx, codec, nullptr) < 0) {
            std::cerr << "❌ Could not open codec" << std::endl;
            return false;
        }

        // Extract SPS/PPS from extradata
        extract_sps_pps();

        // Setup Filter Graph
        if (!setup_filters()) return false;

        // Setup Server Socket
        m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_server_socket < 0) {
            std::cerr << "Failed to create server socket" << std::endl;
            return false;
        }

        int opt = 1;
#ifdef _WIN32
        setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(m_config.bind_address.c_str());
        server_addr.sin_port = htons(m_config.rtsp_port);

        if (bind(m_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind server socket" << std::endl;
            SocketManager::close_socket(m_server_socket);
            return false;
        }

        if (listen(m_server_socket, 5) < 0) {
            std::cerr << "Failed to listen on server socket" << std::endl;
            SocketManager::close_socket(m_server_socket);
            return false;
        }

        // Set receive timeout for accept to allow graceful shutdown
#ifdef _WIN32
        DWORD timeout = 500;
        setsockopt(m_server_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        setsockopt(m_server_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

        m_running.store(true);
        m_server_thread = std::thread(&SimpleRTSPServer::server_loop, this);
        m_streaming_thread = std::thread(&SimpleRTSPServer::streaming_loop, this);

        std::cout << "🚀 Simple RTSP Server started on rtsp://" << m_config.bind_address << ":" << m_config.rtsp_port << "/live" << std::endl;
        return true;
    }

    void stop() {
        if (!m_running.load()) return;
        m_running.store(false);

        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            for (int s : m_client_sockets) {
#ifndef _WIN32
                shutdown(s, SHUT_RDWR);
#endif
                SocketManager::close_socket(s);
            }
            m_client_sockets.clear();
            m_sessions.clear();
        }

        if (m_server_socket != -1) {
#ifndef _WIN32
            shutdown(m_server_socket, SHUT_RDWR);
#endif
            SocketManager::close_socket(m_server_socket);
            m_server_socket = -1;
        }

        if (m_server_thread.joinable()) m_server_thread.join();
        if (m_streaming_thread.joinable()) m_streaming_thread.join();

        // Wait for client threads
        while (m_active_threads.load() > 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

        SocketManager::cleanup();
        std::cout << "Simple RTSP Server stopped" << std::endl;
    }

private:
    void extract_sps_pps() {
        if (!m_enc_ctx->extradata || m_enc_ctx->extradata_size <= 0) return;
        
        uint8_t* extradata = m_enc_ctx->extradata;
        int size = m_enc_ctx->extradata_size;
        
        // Simple AVCC parsing (assuming start codes or size prefixes depending on global header)
        // Typically with GLOBAL_HEADER in libx264 we get start codes in extradata
        // Or we might get AVCC format. Let's look for start codes 00 00 00 01
        
        std::vector<uint8_t> sps, pps;
        int i = 0;
        while (i < size - 4) {
            if (extradata[i] == 0 && extradata[i+1] == 0 && extradata[i+2] == 0 && extradata[i+3] == 1) {
                int start = i + 4;
                int next = start;
                while (next < size - 4) {
                    if (extradata[next] == 0 && extradata[next+1] == 0 && extradata[next+2] == 0 && extradata[next+3] == 1)
                        break;
                    next++;
                }
                if (next == size - 4) next = size; // Last NAL

                uint8_t type = extradata[start] & 0x1F;
                if (type == 7) sps.assign(extradata + start, extradata + next);
                else if (type == 8) pps.assign(extradata + start, extradata + next);
                
                i = next;
            } else {
                i++;
            }
        }
        
        if (!sps.empty()) m_sps_base64 = base64_encode(sps.data(), sps.size());
        if (!pps.empty()) m_pps_base64 = base64_encode(pps.data(), pps.size());
    }

    void server_loop() {
        while (m_running.load()) {
            struct sockaddr_in client_addr;
#ifdef _WIN32
            int len = sizeof(client_addr);
#else
            socklen_t len = sizeof(client_addr);
#endif
            int client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &len);
            
            if (client_socket >= 0) {
                // Set timeouts for client socket to prevent blocking forever
#ifdef _WIN32
                DWORD timeout = 2000;
                setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
                setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
                struct timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
                setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
                setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif
                {
                    std::lock_guard<std::mutex> lock(m_clients_mutex);
                    m_client_sockets.push_back(client_socket);
                    m_sessions[client_socket] = ClientSession();
                }
                
                m_active_threads++;
                std::thread([this, client_socket]() {
                    handle_client(client_socket);
                    m_active_threads--;
                    m_active_threads.notify_all();
                }).detach();
            }
        }
    }

    void streaming_loop() {
        AVFrame* frame = av_frame_alloc();
        AVPacket* pkt = av_packet_alloc();
        int64_t frame_count = 0;
        auto frame_duration = std::chrono::microseconds(1000000 / m_config.fps);
        auto next_frame_time = std::chrono::steady_clock::now();

        while (m_running.load()) {
            std::this_thread::sleep_until(next_frame_time);
            next_frame_time += frame_duration;

            int ret = av_buffersink_get_frame(m_buf_sink_ctx, frame);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                continue;
            }

            frame->pts = frame_count;
            frame_count++;

            encode_and_send(frame, pkt);
            av_frame_unref(frame);
        }
        av_frame_free(&frame);
        av_packet_free(&pkt);
    }

    void handle_client(int client_socket) {
        char buffer[4096];
        while (m_running.load()) {
            int bytes = recv(client_socket, buffer, sizeof(buffer)-1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = 0;
            
            std::string req(buffer);
            if (m_config.enable_logging) {
                std::cout << "\n[DEBUG] <<< Received:\n" << req << std::endl;
            }

            std::istringstream iss(req);
            std::string method, url, ver;
            iss >> method >> url >> ver;
            
            std::string cseq;
            std::string transport;
            std::string line;
            while(std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.find("CSeq:") == 0) {
                    cseq = line.substr(5); // "CSeq:" is 5 chars
                    size_t p = cseq.find_first_not_of(" \t");
                    if (p != std::string::npos) cseq = cseq.substr(p);
                }
                if (line.find("Transport:") == 0) {
                    transport = line.substr(10); // "Transport:" is 10 chars
                    size_t p = transport.find_first_not_of(" \t");
                    if (p != std::string::npos) transport = transport.substr(p);
                }
            }

            std::string resp;
            if (method == "OPTIONS") {
                resp = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n";
            } else if (method == "DESCRIBE") {
                std::string sdp = generate_sdp();
                resp = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\nContent-Type: application/sdp\r\nContent-Length: " + std::to_string(sdp.length()) + "\r\n\r\n" + sdp;
            } else if (method == "SETUP") {
                // Simple TCP interleaved setup
                // Note: If client requests UDP (RTP/AVP) but we reply with TCP (RTP/AVP/TCP), 
                // ffmpeg will complain "Nonmatching transport".
                // Ideally we should check 'transport' string here.
                resp = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\nTransport: RTP/AVP/TCP;unicast;interleaved=0-1\r\nSession: 12345678\r\n\r\n";
            } else if (method == "PLAY") {
                {
                    std::lock_guard<std::mutex> lock(m_clients_mutex);
                    m_sessions[client_socket].playing = true;
                }
                uint16_t seq = m_rtp_sequence.load();
                uint32_t ts = m_rtp_timestamp.load();
                resp = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\nSession: 12345678\r\nRange: npt=0.000-\r\nRTP-Info: url=" + url + ";seq=" + std::to_string(seq) + ";rtptime=" + std::to_string(ts) + "\r\n\r\n";
            } else if (method == "TEARDOWN") {
                resp = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\nSession: 12345678\r\n\r\n";
                send(client_socket, resp.c_str(), resp.length(), 0);
                break; // Close connection
            }

            if (m_config.enable_logging) {
                std::cout << "[DEBUG] >>> Sending:\n" << resp << std::endl;
            }
            send(client_socket, resp.c_str(), resp.length(), 0);
        }
        
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_sessions.erase(client_socket);
            auto it = std::find(m_client_sockets.begin(), m_client_sockets.end(), client_socket);
            if (it != m_client_sockets.end()) m_client_sockets.erase(it);
        }
        SocketManager::close_socket(client_socket);
    }

    std::string generate_sdp() {
        std::ostringstream sdp;
        sdp << "v=0\r\n"
            << "o=- 1234567890 1234567890 IN IP4 " << m_config.bind_address << "\r\n"
            << "s=Simple Stream\r\n"
            << "c=IN IP4 " << m_config.bind_address << "\r\n"
            << "t=0 0\r\n"
            << "m=video 0 RTP/AVP 96\r\n"
            << "a=rtpmap:96 H264/90000\r\n"
            << "a=fmtp:96 packetization-mode=1";
        
        if (!m_sps_base64.empty() && !m_pps_base64.empty()) {
            sdp << ";profile-level-id=42C01E;sprop-parameter-sets=" << m_sps_base64 << "," << m_pps_base64;
        }
        sdp << "\r\n" << "a=control:trackID=1\r\n";
        return sdp.str();
    }

    static std::string base64_encode(const uint8_t* data, size_t len) {
        static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string res;
        int val=0, valb=-6;
        for (size_t i=0; i<len; i++) {
            val = (val<<8) + data[i];
            valb += 8;
            while (valb>=0) {
                res.push_back(tbl[(val>>valb)&0x3F]);
                valb-=6;
            }
        }
        if (valb>-6) res.push_back(tbl[((val<<8)>>(valb+8))&0x3F]);
        while (res.size()%4) res.push_back('=');
        return res;
    }

    bool setup_filters() {
        m_filter_graph = avfilter_graph_alloc();
        const AVFilter* buffersink = avfilter_get_by_name("buffersink");
        
        int ret;
        char args[512];
        snprintf(args, sizeof(args), "size=%dx%d:rate=%d", m_config.width, m_config.height, m_config.fps);
        
        const AVFilter* testsrc_filter = avfilter_get_by_name("testsrc");
        AVFilterContext* testsrc_ctx;
        ret = avfilter_graph_create_filter(&testsrc_ctx, testsrc_filter, "source", args, nullptr, m_filter_graph);
        if (ret < 0) return false;

        const AVFilter* format_filter = avfilter_get_by_name("format");
        AVFilterContext* format_ctx;
        ret = avfilter_graph_create_filter(&format_ctx, format_filter, "format", "pix_fmts=yuv420p", nullptr, m_filter_graph);
        if (ret < 0) return false;

        ret = avfilter_graph_create_filter(&m_buf_sink_ctx, buffersink, "sink", nullptr, nullptr, m_filter_graph);
        if (ret < 0) return false;

        if (avfilter_link(testsrc_ctx, 0, format_ctx, 0) < 0) return false;
        if (avfilter_link(format_ctx, 0, m_buf_sink_ctx, 0) < 0) return false;

        if (avfilter_graph_config(m_filter_graph, nullptr) < 0) return false;
        
        return true;
    }

    void encode_and_send(AVFrame* frame, AVPacket* pkt) {
        int ret = avcodec_send_frame(m_enc_ctx, frame);
        if (ret < 0) return;

        while (ret >= 0) {
            ret = avcodec_receive_packet(m_enc_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

            // Send packet to clients
            // Simple RTP encapsulation: Header + Payload (assuming 1 NAL per packet for simplicity or using internal logic)
            // Real RTP would require proper splitting of large NALs (FU-A).
            // Here we assume small packets or just basic sending for demo.
            // Using packet data directly.
            
            broadcast_packet(pkt->data, pkt->size, pkt->pts);
            av_packet_unref(pkt);
        }
    }
    
    void broadcast_packet(const uint8_t* data, size_t size, int64_t pts) {
        uint32_t timestamp = (uint32_t)(pts * 90000 / m_config.fps); // approximate mapping
        m_rtp_timestamp.store(timestamp);
        uint16_t seq = m_rtp_sequence.fetch_add(1);

        std::vector<uint8_t> rtp_packet;
        // RTP Header
        rtp_packet.resize(12);
        rtp_packet[0] = 0x80; // V=2
        rtp_packet[1] = 96;   // PT=96 (H264)
        rtp_packet[2] = (seq >> 8) & 0xFF;
        rtp_packet[3] = seq & 0xFF;
        rtp_packet[4] = (timestamp >> 24) & 0xFF;
        rtp_packet[5] = (timestamp >> 16) & 0xFF;
        rtp_packet[6] = (timestamp >> 8) & 0xFF;
        rtp_packet[7] = timestamp & 0xFF;
        rtp_packet[8] = (m_rtp_ssrc >> 24) & 0xFF;
        rtp_packet[9] = (m_rtp_ssrc >> 16) & 0xFF;
        rtp_packet[10] = (m_rtp_ssrc >> 8) & 0xFF;
        rtp_packet[11] = m_rtp_ssrc & 0xFF;

        rtp_packet.insert(rtp_packet.end(), data, data + size);

        // TCP interleaved frame
        std::vector<uint8_t> tcp_frame;
        tcp_frame.push_back('$');
        tcp_frame.push_back(0); // Channel 0
        tcp_frame.push_back((rtp_packet.size() >> 8) & 0xFF);
        tcp_frame.push_back(rtp_packet.size() & 0xFF);
        tcp_frame.insert(tcp_frame.end(), rtp_packet.begin(), rtp_packet.end());

        std::vector<int> target_sockets;
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            for (const auto& [sock, session] : m_sessions) {
                if (session.playing) {
                    target_sockets.push_back(sock);
                }
            }
        }

        // Send without holding the lock to avoid deadlock with stop()
        for (int sock : target_sockets) {
#ifdef _WIN32
            send(sock, (const char*)tcp_frame.data(), static_cast<int>(tcp_frame.size()), 0);
#else
            send(sock, (const char*)tcp_frame.data(), tcp_frame.size(), MSG_NOSIGNAL);
#endif
        }
    }
};

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    SimpleRTSPServer::Config config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.rtsp_port = std::stoi(argv[++i]);
        } else if (arg == "--fps" && i + 1 < argc) {
            config.fps = std::stoi(argv[++i]);
        } else if (arg == "--bitrate" && i + 1 < argc) {
            config.bitrate = std::stoi(argv[++i]);
        }
    }

    SimpleRTSPServer server(config);
    if (!server.start()) {
        std::cerr << "Failed to start RTSP Server" << std::endl;
        return 1;
    }

    // Main thread just waits
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}