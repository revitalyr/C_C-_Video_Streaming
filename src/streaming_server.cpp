#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <algorithm>
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

class StreamingServer {
public:
    struct Config {
        int relay_port = 8555;
        int metrics_port = 8080;
        std::string bind_address = "0.0.0.0";
        bool enable_nat_traversal = true;
        bool enable_metrics = true;
    };

private:
    struct Client {
        SOCKET socket;
        std::string rtsp_url;
        std::string client_id;
        std::chrono::steady_clock::time_point connect_time;
        uint64_t bytes_received = 0;
        uint64_t packets_forwarded = 0;
    };
    
    Config m_config;
    std::atomic<bool> m_running{false};
    std::thread m_server_thread;
    std::thread m_metrics_thread;
    SOCKET m_server_socket = INVALID_SOCKET;
    std::vector<Client> m_clients;
    std::mutex m_clients_mutex;
    std::atomic<uint64_t> m_total_bytes{0};
    std::atomic<uint64_t> m_total_packets{0};
    
    void log_info(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] 🌐 " << message << "\n";
    }
    
    void handle_client(SOCKET client_socket) {
        char client_ip[INET_ADDRSTRLEN];
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len);
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        Client new_client;
        new_client.socket = client_socket;
        new_client.client_id = "client_" + std::to_string(static_cast<long long>(client_socket));
        new_client.connect_time = std::chrono::steady_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_clients.push_back(new_client);
        }
        
        log_info("New client connected: " + new_client.client_id + " from " + std::string(client_ip));
        
        // Simple RTSP proxy - forward to our local server
        char buffer[4096];
        while (m_running.load()) {
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) break;
            
            buffer[bytes_received] = '\0';
            std::string request(buffer);
            
            log_info(new_client.client_id + " received: " + request.substr(0, std::min(request.length(), size_t(50))) + "...");
            
            // Forward to local RTSP server (127.0.0.1:8554)
            SOCKET local_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (local_socket != INVALID_SOCKET) {
                struct sockaddr_in local_addr;
                memset(&local_addr, 0, sizeof(local_addr));
                local_addr.sin_family = AF_INET;
                local_addr.sin_port = htons(8554);
                local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                
                if (connect(local_socket, (struct sockaddr*)&local_addr, sizeof(local_addr)) >= 0) {
                    send(local_socket, buffer, bytes_received, 0);
                    
                    // Receive response from local server
                    int response_bytes = recv(local_socket, buffer, sizeof(buffer) - 1, 0);
                    if (response_bytes > 0) {
                        buffer[response_bytes] = '\0';
                        send(client_socket, buffer, response_bytes, 0);
                        
                        new_client.bytes_received += bytes_received;
                        new_client.packets_forwarded++;
                        
                        m_total_bytes.fetch_add(bytes_received);
                        m_total_packets.fetch_add(1);
                    }
                }
                
                closesocket(local_socket);
            }
        }
        
        // Cleanup
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            m_clients.erase(
                std::remove_if(m_clients.begin(), m_clients.end(),
                    [client_socket](const Client& c) { return c.socket == client_socket; })
            );
        }
        
        closesocket(client_socket);
        log_info("Client disconnected: " + new_client.client_id);
    }
    
    void server_loop() {
        log_info("Streaming server started on port " + std::to_string(m_config.relay_port));
        
        while (m_running.load()) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            SOCKET client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket != INVALID_SOCKET) {
                // Handle client in separate thread
                std::thread client_thread(&StreamingServer::handle_client, this, client_socket);
                client_thread.detach();
            }
        }
    }
    
    void metrics_loop() {
        if (!m_config.enable_metrics) return;
        
        log_info("Metrics server started on port " + std::to_string(m_config.metrics_port));
        
        // Simple HTTP server for metrics
        SOCKET metrics_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (metrics_socket == INVALID_SOCKET) {
            log_info("Failed to create metrics socket");
            return;
        }
        
        int opt = 1;
        setsockopt(metrics_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        struct sockaddr_in metrics_addr;
        memset(&metrics_addr, 0, sizeof(metrics_addr));
        metrics_addr.sin_family = AF_INET;
        metrics_addr.sin_port = htons(m_config.metrics_port);
        metrics_addr.sin_addr.s_addr = inet_addr(m_config.bind_address.c_str());
        
        if (bind(metrics_socket, (struct sockaddr*)&metrics_addr, sizeof(metrics_addr)) < 0) {
            log_info("Failed to bind metrics socket");
            closesocket(metrics_socket);
            return;
        }
        
        if (listen(metrics_socket, 5) < 0) {
            log_info("Failed to listen on metrics socket");
            closesocket(metrics_socket);
            return;
        }
        
        while (m_running.load()) {
            SOCKET client_socket = accept(metrics_socket, nullptr, nullptr);
            if (client_socket != INVALID_SOCKET) {
                std::thread metrics_thread([this, client_socket]() {
                    handle_metrics_request(client_socket);
                });
                metrics_thread.detach();
            }
        }
    }
    
    void handle_metrics_request(SOCKET client_socket) {
        char buffer[1024];
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            
            if (std::string(buffer).find("GET /") == 0) {
                std::string response = generate_metrics_html();
                std::string http_response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-Length: " + std::to_string(response.length()) + "\r\n"
                                      "\r\n" + response;
                
                send(client_socket, http_response.c_str(), static_cast<int>(http_response.length()), 0);
            }
        }
        
        closesocket(client_socket);
    }
    
    std::string generate_metrics_html() {
        std::ostringstream html;
        
        html << "<!DOCTYPE html>\n";
        html << "<html>\n";
        html << "<head>\n";
        html << "<title>Streaming Server Metrics</title>\n";
        html << "<meta charset='UTF-8'>\n";
        html << "<style>\n";
        html << "body { font-family: Arial, sans-serif; margin: 20px; }\n";
        html << ".metric { background: #f0f0f0; padding: 15px; margin: 10px 0; border-radius: 5px; }\n";
        html << ".metric h3 { color: #333; margin-top: 0; }\n";
        html << ".stats { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }\n";
        html << ".stat-item { background: #e0e0e0; padding: 10px; border-radius: 3px; }\n";
        html << "</style>\n";
        html << "</head>\n";
        html << "<body>\n";
        html << "<h1>🌐 Streaming Server Metrics</h1>\n";
        
        // Server status
        html << "<div class='metric'>\n";
        html << "<h3>📊 Server Status</h3>\n";
        html << "<div class='stats'>\n";
        html << "<div class='stat-item'><strong>Status:</strong> " << (m_running.load() ? "🟢 Running" : "🔴 Stopped") << "</div>\n";
        html << "<div class='stat-item'><strong>Uptime:</strong> " << "Active" << "</div>\n";
        html << "<div class='stat-item'><strong>Relay Port:</strong> " << m_config.relay_port << "</div>\n";
        html << "<div class='stat-item'><strong>Metrics Port:</strong> " << m_config.metrics_port << "</div>\n";
        html << "</div>\n";
        html << "</div>\n";
        
        // Client statistics
        html << "<div class='metric'>\n";
        html << "<h3>🖥 Connected Clients</h3>\n";
        html << "<div class='stats'>\n";
        
        {
            std::lock_guard<std::mutex> lock(m_clients_mutex);
            html << "<div class='stat-item'><strong>Total Clients:</strong> " << m_clients.size() << "</div>\n";
            
            for (const auto& client : m_clients) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - client.connect_time).count();
                
                html << "<div class='stat-item'>\n";
                html << "<strong>" << client.client_id << "</strong><br>\n";
                html << "Connected: " << duration << "s ago<br>\n";
                html << "Bytes: " << client.bytes_received << "<br>\n";
                html << "Packets: " << client.packets_forwarded;
                html << "</div>\n";
            }
        }
        
        html << "</div>\n";
        html << "</div>\n";
        
        // Global statistics
        html << "<div class='metric'>\n";
        html << "<h3>📈 Global Statistics</h3>\n";
        html << "<div class='stats'>\n";
        html << "<div class='stat-item'><strong>Total Bytes:</strong> " << m_total_bytes.load() << "</div>\n";
        html << "<div class='stat-item'><strong>Total Packets:</strong> " << m_total_packets.load() << "</div>\n";
        html << "</div>\n";
        html << "</div>\n";
        
        html << "</body>\n";
        html << "</html>\n";
        
        return html.str();
    }
    
public:
    explicit StreamingServer() : m_config() {}
    explicit StreamingServer(const Config& config) : m_config(config) {}
    
    ~StreamingServer() {
        stop();
    }
    
    bool start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            log_info("WSAStartup failed");
            return false;
        }
#endif
        
        // Create server socket
        m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_server_socket == INVALID_SOCKET) {
            log_info("Failed to create server socket");
            return false;
        }
        
        int opt = 1;
        setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        // Bind to address
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(m_config.relay_port);
        server_addr.sin_addr.s_addr = inet_addr(m_config.bind_address.c_str());
        
        if (bind(m_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            log_info("Failed to bind server socket");
            closesocket(m_server_socket);
            return false;
        }
        
        if (listen(m_server_socket, 10) < 0) {
            log_info("Failed to listen on server socket");
            closesocket(m_server_socket);
            return false;
        }
        
        m_running.store(true);
        m_server_thread = std::thread(&StreamingServer::server_loop, this);
        
        if (m_config.enable_metrics) {
            m_metrics_thread = std::thread(&StreamingServer::metrics_loop, this);
        }
        
        log_info("🌐 Streaming server started successfully");
        log_info("📡 Relay: rtsp://127.0.0.1:" + std::to_string(m_config.relay_port) + "/live");
        log_info("📊 Metrics: http://127.0.0.1:" + std::to_string(m_config.metrics_port));
        log_info("🎯 Ready to handle client connections");
        
        return true;
    }
    
    void stop() {
        m_running.store(false);
        
        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }
        
        if (m_metrics_thread.joinable()) {
            m_metrics_thread.join();
        }
        
        if (m_server_socket != INVALID_SOCKET) {
            closesocket(m_server_socket);
            m_server_socket = INVALID_SOCKET;
        }
        
#ifdef _WIN32
        WSACleanup();
#endif
        
        log_info("🛑 Streaming server stopped");
    }
};

int main(int argc, char* argv[]) {
    std::cout << "🌐 Streaming Server (Relay/NAT Traversal)\n";
    std::cout << "=====================================\n";
    
    StreamingServer::Config config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.relay_port = std::stoi(argv[++i]);
        } else if (arg == "--metrics" && i + 1 < argc) {
            config.metrics_port = std::stoi(argv[++i]);
        } else if (arg == "--no-metrics") {
            config.enable_metrics = false;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>     Relay port (default: 8555)\n";
            std::cout << "  --metrics <port>  Metrics HTTP port (default: 8080)\n";
            std::cout << "  --no-metrics     Disable metrics server\n";
            std::cout << "  --help           Show this help\n";
            return 0;
        }
    }
    
    std::cout << "🔧 Configuration:\n";
    std::cout << "  Relay Port: " << config.relay_port << "\n";
    std::cout << "  Metrics Port: " << config.metrics_port << "\n";
    std::cout << "  Metrics: " << (config.enable_metrics ? "Enabled" : "Disabled") << "\n";
    std::cout << "=====================================\n";
    
    StreamingServer server(config);
    
    if (!server.start()) {
        std::cerr << "❌ Failed to start streaming server\n";
        return 1;
    }
    
    std::cout << "🌐 Server is running. Press Ctrl+C to stop.\n";
    std::cout << "📡 Connect clients to: rtsp://127.0.0.1:" << std::to_string(config.relay_port) << "/live\n";
    std::cout << "📊 View metrics at: http://127.0.0.1:" << std::to_string(config.metrics_port) << "\n";
    
    // Simple signal handling
    std::cin.get();
    
    server.stop();
    return 0;
}
