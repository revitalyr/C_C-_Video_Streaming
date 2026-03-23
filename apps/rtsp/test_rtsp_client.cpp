#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

using namespace std::chrono_literals;

// Helper for Base64 decoding
static std::vector<uint8_t> base64_decode(const std::string& in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return std::vector<uint8_t>(out.begin(), out.end());
}

static const uint8_t START_CODE[4] = {0, 0, 0, 1};

static void write_nal(std::ofstream& outfile, const std::vector<uint8_t>& data) {
    outfile.write((const char*)START_CODE, 4);
    outfile.write((const char*)data.data(), data.size());
}
static void write_nal(std::ofstream& outfile, const uint8_t* data, size_t size) {
    outfile.write((const char*)START_CODE, 4);
    outfile.write((const char*)data, size);
}

class RTSPClientRecorder {
public:
    struct Config {
        std::string url;
        std::string output_file;
        int duration_sec = 10;
    };

    explicit RTSPClientRecorder(const Config& config) : m_config(config) {}

    ~RTSPClientRecorder() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool run() {
        if (!initialize_network()) return false;
        if (!parse_url()) return false;
        if (!connect_server()) return false;
        
        std::cout << "📡 Connected to " << m_host << ":" << m_port << "\n";

        if (!perform_handshake()) return false;

        std::cout << "🎥 Recording stream to " << m_config.output_file << " for " << m_config.duration_sec << "s...\n";
        return record_loop();
    }

private:
    Config m_config;
    SOCKET m_socket = INVALID_SOCKET;
    std::string m_host;
    int m_port = 554;
    std::string m_path;
    std::string m_session;
    int m_cseq = 1;
    std::atomic<bool> m_running{true};
    std::vector<uint8_t> m_sps;
    std::vector<uint8_t> m_pps;

    bool initialize_network() {
#ifdef _WIN32
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
        return true;
#endif
    }

    bool parse_url() {
        std::string url = m_config.url;
        if (url.find("rtsp://") != 0) {
            std::cerr << "❌ Invalid URL format (must start with rtsp://)\n";
            return false;
        }
        
        std::string no_proto = url.substr(7);
        size_t slash_pos = no_proto.find('/');
        std::string host_port = (slash_pos == std::string::npos) ? no_proto : no_proto.substr(0, slash_pos);
        m_path = (slash_pos == std::string::npos) ? "/" : no_proto.substr(slash_pos);

        size_t colon_pos = host_port.find(':');
        if (colon_pos != std::string::npos) {
            m_host = host_port.substr(0, colon_pos);
            m_port = std::stoi(host_port.substr(colon_pos + 1));
        } else {
            m_host = host_port;
        }
        return true;
    }

    bool connect_server() {
        struct hostent* he = gethostbyname(m_host.c_str());
        if (!he) return false;

        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket == INVALID_SOCKET) return false;

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(m_port);
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);

        if (connect(m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "❌ Connection failed\n";
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }
        return true;
    }

    bool send_request(const std::string& method, const std::string& extra_headers = "") {
        std::string req = method + " " + m_config.url + " RTSP/1.0\r\n" +
                          "CSeq: " + std::to_string(m_cseq++) + "\r\n" +
                          "User-Agent: TestRTSPClient/1.0\r\n";
        if (!m_session.empty()) req += "Session: " + m_session + "\r\n";
        req += extra_headers + "\r\n";

        return send(m_socket, req.c_str(), req.length(), 0) > 0;
    }

    bool read_response(std::string& response) {
        char buffer[4096];
        int bytes = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) return false;
        buffer[bytes] = 0;
        response = buffer;
        return response.find("200 OK") != std::string::npos;
    }

    bool perform_handshake() {
        std::string resp;

        // OPTIONS
        if (!send_request("OPTIONS") || !read_response(resp)) return false;

        // DESCRIBE
        if (!send_request("DESCRIBE", "Accept: application/sdp") || !read_response(resp)) return false;

        // Parse sprop-parameter-sets for SPS/PPS
        size_t sprop_pos = resp.find("sprop-parameter-sets=");
        if (sprop_pos != std::string::npos) {
            size_t val_start = sprop_pos + 21;
            size_t val_end = resp.find_first_of("\r\n;", val_start);
            std::string sprop = resp.substr(val_start, val_end - val_start);
            
            size_t comma_pos = sprop.find(',');
            if (comma_pos != std::string::npos) {
                std::string sps_str = sprop.substr(0, comma_pos);
                std::string pps_str = sprop.substr(comma_pos + 1);
                m_sps = base64_decode(sps_str);
                m_pps = base64_decode(pps_str);
                std::cout << "ℹ️  Found SPS (" << m_sps.size() << " bytes) and PPS (" << m_pps.size() << " bytes)\n";
            }
        }

        // SETUP (TCP Interleaved)
        if (!send_request("SETUP", "Transport: RTP/AVP/TCP;unicast;interleaved=0-1") || !read_response(resp)) return false;
        
        // Parse Session ID
        size_t sess_pos = resp.find("Session: ");
        if (sess_pos != std::string::npos) {
            size_t end_pos = resp.find_first_of("\r\n;", sess_pos);
            m_session = resp.substr(sess_pos + 9, end_pos - (sess_pos + 9));
        }

        // PLAY
        if (!send_request("PLAY", "Range: npt=0.000-") || !read_response(resp)) return false;

        return true;
    }

    bool record_loop() {
        std::ofstream outfile(m_config.output_file, std::ios::binary);
        if (!outfile.is_open()) {
            std::cerr << "❌ Could not open output file\n";
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();
        std::vector<uint8_t> buffer(65536); // 64KB buffer
        std::vector<uint8_t> packet_buffer;
        uint64_t total_bytes = 0;

        // Write SPS/PPS first (crucial for playback)
        if (!m_sps.empty()) write_nal(outfile, m_sps);
        if (!m_pps.empty()) write_nal(outfile, m_pps);

        while (m_running) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= m_config.duration_sec) {
                break;
            }

            int received = recv(m_socket, (char*)buffer.data(), buffer.size(), 0);
            if (received <= 0) break;

            packet_buffer.insert(packet_buffer.end(), buffer.begin(), buffer.begin() + received);

            // Process TCP interleaved packets: $ + Channel(1) + Length(2) + Data
            while (packet_buffer.size() >= 4) {
                if (packet_buffer[0] != '$') {
                    // Lost sync or garbage, find next '$'
                    auto it = std::find(packet_buffer.begin(), packet_buffer.end(), '$');
                    if (it == packet_buffer.end()) {
                        packet_buffer.clear();
                        break;
                    }
                    packet_buffer.erase(packet_buffer.begin(), it);
                    continue;
                }

                int channel = packet_buffer[1];
                uint16_t length = (packet_buffer[2] << 8) | packet_buffer[3];

                if (packet_buffer.size() < 4 + length) break; // Wait for more data

                // Extract RTP payload if channel 0 (Video RTP)
                if (channel == 0) {
                    // RTP Header is typically 12 bytes
                    // Convert to Annex B (Start Code + NALU)
                    if (length > 12) {
                        write_nal(outfile, packet_buffer.data() + 4 + 12, length - 12);
                    }
                    total_bytes += length;
                }

                packet_buffer.erase(packet_buffer.begin(), packet_buffer.begin() + 4 + length);
            }
        }

        std::cout << "✅ Recording finished. Saved " << total_bytes << " bytes to " << m_config.output_file << "\n";
        send_request("TEARDOWN");
        return total_bytes > 0;
    }

    void stop() {
        m_running = false;
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <rtsp_url> <output_file> <duration_sec>\n";
        return 1;
    }

    RTSPClientRecorder::Config config;
    config.url = argv[1];
    config.output_file = argv[2];
    config.duration_sec = std::stoi(argv[3]);

    RTSPClientRecorder recorder(config);
    if (recorder.run()) {
        return 0;
    } else {
        return 1;
    }
}