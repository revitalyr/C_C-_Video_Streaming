#pragma once

#include "udp_socket.hpp"
#include "rtp/rtp_packet.hpp"
#include "common/types.hpp"
#include <optional>
#include <atomic>

namespace video_streaming {

class Receiver {
public:
    explicit Receiver(Port port);
    ~Receiver();

    bool start();
    void stop();
    
    // Non-blocking receive
    std::optional<RtpPacket> receive();

private:
    UdpSocket m_socket;
    Port m_port;
    std::atomic<bool> m_running{false};
};

} // namespace video_streaming