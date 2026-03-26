module;

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <iostream>
#include <string>
#include <vector>

module video_streaming.network.udp_socket;
import video_streaming.common.types;

namespace video_streaming {

#ifdef _WIN32
static bool winsock_initialized = false;

bool UdpSocket::initialize_winsock() {
    if (!winsock_initialized) {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        winsock_initialized = (result == 0);
        return winsock_initialized;
    }
    return true;
}

void UdpSocket::cleanup_winsock() {
    if (winsock_initialized) {
        WSACleanup();
        winsock_initialized = false;
    }
}
#else
bool UdpSocket::initialize_winsock() { return true; }
void UdpSocket::cleanup_winsock() {}
#endif

UdpSocket::UdpSocket() 
    : m_socket(INVALID_SOCKET), m_initialized(false) {
    initialize_winsock();
}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::is_open() const {
    // Debug: Check if this pointer is valid
    if (!this) {
        return false;
    }
    // Debug: Check if m_socket is properly initialized
    if (m_socket == INVALID_SOCKET) {
        return false;
    }
    return true;
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : m_socket(other.m_socket), m_initialized(other.m_initialized) {
    other.m_socket = INVALID_SOCKET;
    other.m_initialized = false;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        m_socket = other.m_socket;
        m_initialized = other.m_initialized;
        other.m_socket = INVALID_SOCKET;
        other.m_initialized = false;
    }
    return *this;
}

bool UdpSocket::open() {
    if (is_open()) {
        return true;
    }
    
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return is_open();
}

void UdpSocket::close() {
    if (is_open()) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = INVALID_SOCKET;
    }
}

bool UdpSocket::bind(Port port) {
    return bind(Endpoint("0.0.0.0", port));
}

bool UdpSocket::bind(const Endpoint& endpoint) {
    if (!is_open() && !open()) {
        return false;
    }
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    if (!endpoint_to_sockaddr(endpoint, reinterpret_cast<struct sockaddr*>(&addr), addr_len)) {
        return false;
    }
    
    int result = ::bind(m_socket, reinterpret_cast<struct sockaddr*>(&addr), addr_len);
    return result == 0;
}

bool UdpSocket::send(const Bytes& data, const Endpoint& remote) {
    if (!is_open()) {
        return false;
    }
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (!endpoint_to_sockaddr(remote, reinterpret_cast<struct sockaddr*>(&addr), addr_len)) {
        return false;
    }
    
    int bytes_sent = sendto(m_socket,
                           reinterpret_cast<const char*>(data.data()),
                           static_cast<int>(data.size()),
                           0,
                           reinterpret_cast<struct sockaddr*>(&addr),
                           addr_len);
    
    return bytes_sent == static_cast<int>(data.size());
}

bool UdpSocket::send_to(const Bytes& data, const IpAddress& ip, Port port) {
    return send(data, Endpoint(ip, port));
}

int UdpSocket::receive(Bytes& buffer, Endpoint& sender) {
    return receive_from(buffer, DEFAULT_BUFFER_SIZE, sender);
}

int UdpSocket::receive_from(Bytes& buffer, size_t max_size, Endpoint& sender) {
    if (!is_open()) {
        return -1;
    }
    
    buffer.resize(max_size);
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    int bytes_received = recvfrom(m_socket,
                                  reinterpret_cast<char*>(buffer.data()),
                                  static_cast<int>(max_size),
                                  0,
                                  reinterpret_cast<struct sockaddr*>(&addr),
                                  &addr_len);
    
    if (bytes_received > 0) {
        buffer.resize(bytes_received);
        sender = sockaddr_to_endpoint(reinterpret_cast<struct sockaddr*>(&addr), addr_len);
    } else {
        buffer.clear();
    }
    
    return bytes_received;
}

bool UdpSocket::set_blocking(bool blocking) {
    if (!is_open()) {
        return false;
    }
    
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    int result = ioctlsocket(m_socket, FIONBIO, &mode);
    return result == 0;
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags == -1) return false;
    
    int result = fcntl(m_socket, F_SETFL, 
                      blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
    return result != -1;
#endif
}

bool UdpSocket::set_receive_buffer_size(size_t size) {
    if (!is_open()) {
        return false;
    }
    
    int buffer_size = static_cast<int>(size);
    int result = setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, 
                           reinterpret_cast<char*>(&buffer_size), sizeof(buffer_size));
    return result == 0;
}

bool UdpSocket::set_send_buffer_size(size_t size) {
    if (!is_open()) {
        return false;
    }
    
    int buffer_size = static_cast<int>(size);
    int result = setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, 
                           reinterpret_cast<char*>(&buffer_size), sizeof(buffer_size));
    return result == 0;
}

bool UdpSocket::set_timeout(int milliseconds) {
    if (!is_open()) {
        return false;
    }
    
#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(milliseconds);
    int result1 = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, 
                            reinterpret_cast<char*>(&timeout), sizeof(timeout));
    int result2 = setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, 
                            reinterpret_cast<char*>(&timeout), sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    
    int result1 = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, 
                            reinterpret_cast<char*>(&tv), sizeof(tv));
    int result2 = setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, 
                            reinterpret_cast<char*>(&tv), sizeof(tv));
#endif
    
    return result1 == 0 && result2 == 0;
}

Endpoint UdpSocket::get_local_endpoint() const {
    if (!is_open()) {
        return Endpoint();
    }
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    int result = getsockname(m_socket, reinterpret_cast<struct sockaddr*>(&addr), &addr_len);
    if (result != 0) {
        return Endpoint();
    }
    
    return sockaddr_to_endpoint(reinterpret_cast<struct sockaddr*>(&addr), addr_len);
}

String UdpSocket::get_last_error() const {
#ifdef _WIN32
    int error_code = WSAGetLastError();
    char error_msg[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, error_code, 0, error_msg, sizeof(error_msg), nullptr);
    return String(error_msg);
#else
    return String(strerror(errno));
#endif
}

int UdpSocket::get_last_error_code() const {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

Endpoint UdpSocket::sockaddr_to_endpoint(const struct sockaddr* addr, socklen_t addr_len) const {
    if (addr->sa_family != AF_INET || addr_len < sizeof(struct sockaddr_in)) {
        return Endpoint();
    }
    
    const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(addr);
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(sin->sin_addr), ip_str, INET_ADDRSTRLEN);
    
    return Endpoint(String(ip_str), ntohs(sin->sin_port));
}

bool UdpSocket::endpoint_to_sockaddr(const Endpoint& endpoint, struct sockaddr* addr, socklen_t& addr_len) const {
    if (!endpoint.is_valid()) {
        return false;
    }
    
    struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(addr);
    addr_len = sizeof(struct sockaddr_in);
    
    sin->sin_family = AF_INET;
    sin->sin_port = htons(endpoint.port);
    
    int result = inet_pton(AF_INET, endpoint.ip_address.c_str(), &(sin->sin_addr));
    return result == 1;
}

} // namespace video_streaming
