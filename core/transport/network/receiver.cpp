#include "receiver.hpp"

namespace video_streaming {

Receiver::Receiver(Port port) 
    : m_port(port) {
}

Receiver::~Receiver() {
    stop();
}

bool Receiver::start() {
    if (!m_socket.open()) return false;
    if (!m_socket.bind(m_port)) return false;
    m_socket.set_blocking(false); // Non-blocking mode
    m_running = true;
    return true;
}

void Receiver::stop() {
    m_running = false;
    m_socket.close();
}

std::optional<RtpPacket> Receiver::receive() {
    if (!m_running) return std::nullopt;

    Bytes buffer;
    Endpoint sender;
    // Use a reasonable MTU size or larger for reception
    int bytes_read = m_socket.receive_from(buffer, 2048, sender);

    if (bytes_read > 0) {
        RtpPacket packet;
        if (packet.deserialize(buffer)) {
            return packet;
        }
    }
    return std::nullopt;
}

} // namespace video_streaming