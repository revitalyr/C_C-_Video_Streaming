#pragma once

#include "endpoint.hpp"
#include "common/types.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();
    
    // Non-copyable
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    
    // Movable
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    
    bool is_open() const { return m_socket != INVALID_SOCKET; }
    
    // Socket operations
    bool open();
    void close();
    bool bind(Port port);
    bool bind(const Endpoint& endpoint);
    
    // Send/Receive
    bool send(const Bytes& data, const Endpoint& remote);
    bool send_to(const Bytes& data, const IpAddress& ip, Port port);
    
    int receive(Bytes& buffer, Endpoint& sender);
    int receive_from(Bytes& buffer, size_t max_size, Endpoint& sender);
    
    // Socket options
    bool set_blocking(bool blocking);
    bool set_receive_buffer_size(size_t size);
    bool set_send_buffer_size(size_t size);
    bool set_timeout(int milliseconds);
    
    // Get local endpoint
    Endpoint get_local_endpoint() const;
    
    // Error handling
    String get_last_error() const;
    int get_last_error_code() const;

private:
    bool initialize_winsock();
    void cleanup_winsock();
    
    Endpoint sockaddr_to_endpoint(const struct sockaddr* addr, socklen_t addr_len) const;
    bool endpoint_to_sockaddr(const Endpoint& endpoint, struct sockaddr* addr, socklen_t& addr_len) const;

private:
    SOCKET m_socket{INVALID_SOCKET};
    bool m_initialized{false};
    
    static constexpr size_t DEFAULT_BUFFER_SIZE = 65536; // 64KB
    static constexpr int DEFAULT_TIMEOUT_MS = 1000; // 1 second
};
