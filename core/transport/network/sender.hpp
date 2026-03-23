#pragma once

#include "udp_socket.hpp"
#include "rtp/rtp_packet.hpp"
#include "common/types.hpp"

namespace video_streaming {

class Sender {
public:
    Sender(const String& ip, Port port);
    ~Sender();

    bool start();
    void stop();
    bool send(const RtpPacket& packet);

private:
    UdpSocket m_socket;
    Endpoint m_destination;
};

} // namespace video_streaming