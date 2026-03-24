module;

#include <memory>

export module video_streaming.network.sender;

import video_streaming.common.types;
import video_streaming.network.udp_socket;
import video_streaming.network.endpoint;
import video_streaming.rtp.packet;

namespace video_streaming {

export class Sender {
public:
    Sender(const IpAddress& ip, Port port);
    ~Sender();

    // Start sender
    bool start();
    
    // Stop sender
    void stop();
    
    // Send a single RTP packet
    bool send(const RtpPacket& packet);

private:
    Endpoint m_destination;
    std::unique_ptr<UdpSocket> m_socket;
};

} // namespace video_streaming