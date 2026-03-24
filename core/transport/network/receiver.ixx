module;

#include <optional>
#include <memory>

export module video_streaming.network.receiver;

import video_streaming.common.types;
import video_streaming.network.udp_socket;
import video_streaming.rtp.packet;

namespace video_streaming {

export class Receiver {
public:
    explicit Receiver(Port port);
    ~Receiver();

    // Start receiving
    bool start();
    
    // Stop receiving
    void stop();
    
    // Receive a single RTP packet (blocking or non-blocking depending on socket)
    std::optional<RtpPacket> receive();
    
    Port get_port() const { return m_port; }

private:
    Port m_port;
    std::unique_ptr<UdpSocket> m_socket;
};

} // namespace video_streaming