module;

#include <vector>
#include <cstdint>
#include <span>

#include "rtp/rtp_packet.hpp"
#include "common/types.hpp"

export module video_streaming.rtp.h264_packetizer;

namespace video_streaming {

export class H264Packetizer {
public:
    static constexpr size_t DEFAULT_MTU = 1400;

    explicit H264Packetizer(uint32_t ssrc, size_t mtu = DEFAULT_MTU);

    // Packetize a full H.264 frame (which may contain multiple NAL units or need fragmentation)
    std::vector<RtpPacket> packetize_frame(const std::vector<uint8_t>& frame_data, uint32_t timestamp);

    // Packetize a single NAL unit
    std::vector<RtpPacket> packetize_nal(std::span<const uint8_t> nal_unit, uint32_t timestamp, bool marker_bit);

private:
    std::vector<RtpPacket> fragment_nal(std::span<const uint8_t> nal_unit, uint32_t timestamp, bool marker_bit);
    RtpPacket create_single_nal_packet(std::span<const uint8_t> nal_unit, uint32_t timestamp, bool marker_bit);

    uint32_t m_ssrc;
    size_t m_mtu;
    uint16_t m_sequence_number = 0;
};

} // namespace video_streaming