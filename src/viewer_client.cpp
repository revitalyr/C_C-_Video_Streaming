#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <vector>
#include <optional>
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
#include <netdb.h>
using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

// H.264 NAL unit assembler based on your detailed explanation
struct RtpPacket {
    uint16_t seq;
    bool marker;
    const uint8_t* payload;
    size_t payload_size;
};

class H264Assembler {
public:
    // Возвращает готовый NAL-unit (Annex-B), если он завершён
    std::optional<std::vector<uint8_t>> assemble(const RtpPacket& pkt) {
        if (pkt.payload_size < 1)
            return std::nullopt;

        uint8_t nal_header = pkt.payload[0];
        uint8_t nal_type = nal_header & 0x1F;

        // -------------------------------
        // 1) Обычный NAL (тип 1–23)
        // -------------------------------
        if (nal_type >= 1 && nal_type <= 23) {
            std::vector<uint8_t> nal;
            add_start_code(nal);
            nal.insert(nal.end(), pkt.payload, pkt.payload + pkt.payload_size);
            return nal;
        }

        // -------------------------------
        // 2) STAP-A (тип 24)
        // -------------------------------
        if (nal_type == 24) {
            size_t offset = 1;
            std::vector<uint8_t> nal;

            while (offset + 2 < pkt.payload_size) {
                uint16_t size = (pkt.payload[offset] << 8) | pkt.payload[offset + 1];
                offset += 2;

                if (offset + size > pkt.payload_size)
                    break;

                add_start_code(nal);
                nal.insert(nal.end(), pkt.payload + offset, pkt.payload + offset + size);
                offset += size;
            }
            return nal;
        }

        // -------------------------------
        // 3) FU-A (тип 28)
        // -------------------------------
        if (nal_type == 28) {
            if (pkt.payload_size < 2)
                return std::nullopt;

            uint8_t fu_indicator = pkt.payload[0];
            uint8_t fu_header = pkt.payload[1];

            bool start = fu_header & 0x80;
            bool end   = fu_header & 0x40;
            uint8_t original_nal_type = fu_header & 0x1F;

            if (start) {
                // Начало нового FU-A
                fu_buffer.clear();
                add_start_code(fu_buffer);

                uint8_t restored_header =
                    (fu_indicator & 0xE0) | original_nal_type;

                fu_buffer.push_back(restored_header);
                fu_buffer.insert(fu_buffer.end(),
                                 pkt.payload + 2,
                                 pkt.payload + pkt.payload_size);

                return std::nullopt; // ещё не готово
            }
            else {
                // Продолжение
                fu_buffer.insert(fu_buffer.end(),
                                 pkt.payload + 2,
                                 pkt.payload + pkt.payload_size);

                if (end) {
                    // Конец FU-A → вернуть собранный NAL
                    auto nal = fu_buffer;
                    fu_buffer.clear();
                    return nal;
                }

                return std::nullopt;
            }
        }

        // Неизвестный тип
        return std::nullopt;
    }

private:
    std::vector<uint8_t> fu_buffer;

    void add_start_code(std::vector<uint8_t>& out) {
        static const uint8_t sc[4] = {0x00, 0x00, 0x00, 0x01};
        out.insert(out.end(), sc, sc + 4);
    }
};

class RTSPViewer {
private:
    std::string m_rtsp_url;
    std::atomic<bool> m_running{false};
    std::thread m_receive_thread;
    SOCKET m_socket = INVALID_SOCKET;
    H264Assembler m_assembler;
    std::vector<uint8_t> m_tcp_buffer;
    
    // TCP interleave parser
    std::optional<std::vector<uint8_t>> parse_tcp_interleave(const uint8_t* data, size_t size) {
        // Looking for $<channel><length><data> format
        if (size < 4 || data[0] != '$')
            return std::nullopt;
            
        uint8_t channel = data[1];
        uint16_t length = (data[2] << 8) | data[3];
        
        if (size < 4 + length)
            return std::nullopt;
            
        // Extract RTP payload (skip $<channel><length>)
        const uint8_t* rtp_data = data + 4;
        size_t rtp_size = length;
        
        // Parse RTP header (12 bytes minimum)
        if (rtp_size < 12)
            return std::nullopt;
            
        // RTP header parsing
        uint8_t version = (rtp_data[0] >> 6) & 0x03;
        uint8_t padding = (rtp_data[0] >> 5) & 0x01;
        uint8_t extension = (rtp_data[0] >> 4) & 0x01;
        uint8_t csrc_count = rtp_data[0] & 0x0F;
        bool marker = (rtp_data[1] >> 7) & 0x01;
        uint8_t payload_type = rtp_data[1] & 0x7F;
        uint16_t seq = (rtp_data[2] << 8) | rtp_data[3];
        
        // Skip RTP header and CSRC
        size_t payload_offset = 12 + (csrc_count * 4);
        if (rtp_size < payload_offset)
            return std::nullopt;
            
        const uint8_t* payload = rtp_data + payload_offset;
        size_t payload_size = rtp_size - payload_offset;
        
        // Create RTP packet for assembler
        RtpPacket pkt;
        pkt.seq = seq;
        pkt.marker = marker;
        pkt.payload = payload;
        pkt.payload_size = payload_size;
        
        // Assemble NAL unit
        return m_assembler.assemble(pkt);
    }
    
    void process_tcp_data(const uint8_t* data, size_t size) {
        // Append to buffer
        m_tcp_buffer.insert(m_tcp_buffer.end(), data, data + size);
        
        // Process complete packets
        size_t offset = 0;
        while (offset < m_tcp_buffer.size()) {
            // Look for $ marker
            if (m_tcp_buffer[offset] == '$') {
                // Check if we have enough data for length
                if (offset + 3 < m_tcp_buffer.size()) {
                    uint16_t packet_length = (m_tcp_buffer[offset + 2] << 8) | m_tcp_buffer[offset + 3];
                    size_t total_size = 4 + packet_length;
                    
                    if (offset + total_size <= m_tcp_buffer.size()) {
                        // Complete packet available
                        auto nal = parse_tcp_interleave(&m_tcp_buffer[offset], total_size);
                        if (nal.has_value()) {
                            std::cout << "🎬 Received complete NAL unit (" << nal->size() << " bytes)\n";
                            
                            // Print NAL type for debugging
                            if (nal->size() >= 4) {
                                uint8_t nal_type = (*nal)[4] & 0x1F;
                                std::string type_name = "Unknown";
                                switch (nal_type) {
                                    case 1: type_name = "Non-IDR slice"; break;
                                    case 5: type_name = "IDR slice"; break;
                                    case 6: type_name = "SEI"; break;
                                    case 7: type_name = "SPS"; break;
                                    case 8: type_name = "PPS"; break;
                                    case 9: type_name = "AUD"; break;
                                }
                                std::cout << "   📦 NAL Type: " << (int)nal_type << " (" << type_name << ")\n";
                            }
                        }
                        
                        offset += total_size;
                        continue;
                    }
                }
            }
            
            // Skip to next potential packet start
            offset++;
        }
        
        // Remove processed data from buffer
        if (offset > 0) {
            m_tcp_buffer.erase(m_tcp_buffer.begin(), m_tcp_buffer.begin() + offset);
        }
    }
    
    void receive_loop() {
        std::cout << "🖥 Starting RTSP viewer for: " << m_rtsp_url << "\n";
        
        // Parse URL
        size_t host_start = m_rtsp_url.find("://") + 3;
        size_t host_end = m_rtsp_url.find(":", host_start);
        size_t port_start = m_rtsp_url.find(":", host_end) + 1;
        size_t port_end = m_rtsp_url.find("/", port_start);
        
        std::string host = m_rtsp_url.substr(host_start, host_end - host_start);
        std::string port = m_rtsp_url.substr(port_start, port_end - port_start);
        std::string path = m_rtsp_url.substr(port_end);
        
        std::cout << "📡 Host: " << host << ", Port: " << port << ", Path: " << path << "\n";
        
        // Connect to RTSP server
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(std::stoi(port));
        server_addr.sin_addr.s_addr = inet_addr(host.c_str());
        
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket == INVALID_SOCKET) {
            std::cerr << "❌ Failed to create socket\n";
            return;
        }
        
        if (connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "❌ Failed to connect to RTSP server\n";
            closesocket(m_socket);
            return;
        }
        
        std::cout << "✅ Connected to RTSP server\n";
        
        // Send OPTIONS request
        std::string options = "OPTIONS " + path + " RTSP/1.0\r\n"
                           "CSeq: 1\r\n"
                           "User-Agent: RTSPViewer/1.0\r\n"
                           "\r\n";
        
        send(m_socket, options.c_str(), static_cast<int>(options.length()), 0);
        std::cout << "📤 Sent OPTIONS request\n";
        
        // Receive response
        char buffer[4096];
        int bytes_received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "📥 Received response (" << bytes_received << " bytes):\n";
            std::cout << std::string(buffer) << "\n";
        }
        
        // Send DESCRIBE request
        std::string describe = "DESCRIBE " + path + " RTSP/1.0\r\n"
                            "CSeq: 2\r\n"
                            "User-Agent: RTSPViewer/1.0\r\n"
                            "\r\n";
        
        send(m_socket, describe.c_str(), static_cast<int>(describe.length()), 0);
        std::cout << "📤 Sent DESCRIBE request\n";
        
        bytes_received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "📥 Received SDP (" << bytes_received << " bytes):\n";
            std::cout << std::string(buffer) << "\n";
        }
        
        // Send SETUP request
        std::string setup = "SETUP " + path + " RTSP/1.0\r\n"
                          "CSeq: 3\r\n"
                          "User-Agent: RTSPViewer/1.0\r\n"
                          "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                          "\r\n";
        
        send(m_socket, setup.c_str(), static_cast<int>(setup.length()), 0);
        std::cout << "📤 Sent SETUP request\n";
        
        bytes_received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "📥 Received SETUP response (" << bytes_received << " bytes):\n";
            std::cout << std::string(buffer) << "\n";
        }
        
        // Send PLAY request
        std::string play = "PLAY " + path + " RTSP/1.0\r\n"
                        "CSeq: 4\r\n"
                        "User-Agent: RTSPViewer/1.0\r\n"
                        "Session: 12345678\r\n"
                        "\r\n";
        
        send(m_socket, play.c_str(), static_cast<int>(play.length()), 0);
        std::cout << "📤 Sent PLAY request\n";
        
        bytes_received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::cout << "📥 Received PLAY response (" << bytes_received << " bytes):\n";
            std::cout << std::string(buffer) << "\n";
        }
        
        std::cout << "🎬 RTSP handshake completed\n";
        std::cout << "📊 Starting to receive RTP packets...\n";
        
        // Enhanced RTP receiving loop with TCP interleave parsing
        auto start_time = std::chrono::steady_clock::now();
        int packet_count = 0;
        int nal_count = 0;
        
        while (m_running.load()) {
            char buffer[8192];  // Larger buffer for TCP interleave
            int bytes_received = recv(m_socket, buffer, sizeof(buffer), 0);
            
            if (bytes_received > 0) {
                packet_count++;
                
                // Process TCP interleave data
                process_tcp_data(reinterpret_cast<const uint8_t*>(buffer), bytes_received);
                
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start_time);
                
                if (elapsed.count() > 0 && packet_count % 50 == 0) {
                    std::cout << "📊 Received " << packet_count << " packets in " 
                              << elapsed.count() << " seconds\n";
                }
            } else if (bytes_received == 0) {
                std::cout << "🔌 Connection closed by server\n";
                break;
            } else {
                // Error occurred
                std::cerr << "❌ Receive error\n";
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        std::cout << "🛑 Viewer stopped\n";
    }
    
public:
    explicit RTSPViewer(const std::string& url) : m_rtsp_url(url) {}
    
    ~RTSPViewer() {
        stop();
    }
    
    bool start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "❌ WSAStartup failed\n";
            return false;
        }
#endif
        
        m_running.store(true);
        m_receive_thread = std::thread(&RTSPViewer::receive_loop, this);
        
        std::cout << "🚀 RTSP Viewer started\n";
        return true;
    }
    
    void stop() {
        m_running.store(false);
        if (m_receive_thread.joinable()) {
            m_receive_thread.join();
        }
        
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        
#ifdef _WIN32
        WSACleanup();
#endif
        
        std::cout << "🛑 RTSP Viewer stopped\n";
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "=== RTSP Viewer Client ===\n";
        std::cout << "Usage: " << argv[0] << " <rtsp_url>\n";
        std::cout << "Example: " << argv[0] << " rtsp://127.0.0.1:8554/live\n";
        return 1;
    }
    
    std::string rtsp_url = argv[1];
    
    std::cout << "🖥 RTSP Viewer Client\n";
    std::cout << "📡 Connecting to: " << rtsp_url << "\n";
    std::cout << "=====================================\n";
    
    RTSPViewer viewer(rtsp_url);
    
    if (!viewer.start()) {
        std::cerr << "❌ Failed to start viewer\n";
        return 1;
    }
    
    std::cout << "🎬 Viewer is running. Press Ctrl+C to stop.\n";
    
    // Simple signal handling
    std::cin.get();
    
    viewer.stop();
    return 0;
}
