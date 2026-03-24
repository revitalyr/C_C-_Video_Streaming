module;

module video_streaming.network.sender;
import video_streaming.common.types;

namespace video_streaming {

Sender::Sender(const String& ip, Port port) 
    : m_destination(ip, port) {
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