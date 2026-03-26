module;

#include <memory>

module video_streaming.network.sender;
import video_streaming.common.types;
import video_streaming.network.udp_socket;
import video_streaming.rtp.packet;

namespace video_streaming {

Sender::Sender(const String& ip, Port port) 
    : m_destination(ip, port), m_socket(std::make_unique<UdpSocket>()) {
}

Sender::~Sender() {
    stop();
}

bool Sender::start() {
    return m_socket->open();
}

void Sender::stop() {
    m_socket->close();
}

bool Sender::send(const RtpPacket& packet) {
    // Serialize RTP packet to bytes
    Bytes data = packet.serialize();
    return m_socket->send(data, m_destination);
}

} // namespace video_streaming