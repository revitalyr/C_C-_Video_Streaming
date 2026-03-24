module;

#include <vector>
#include <cstdint>
#include <span>

export import video_streaming.rtp.packet;
import video_streaming.media.frame; // For NalType
import video_streaming.common.types;
export module video_streaming.rtp.h264_packetizer;

namespace video_streaming {

export class H264Packetizer {
public:
    static constexpr size_t DEFAULT_MTU = 1400;

    explicit H264Packetizer(uint32_t ssrc, size_t mtu = DEFAULT_MTU);

    // Packetize a full H.264 frame (which may contain multiple NAL units or need fragmentation)
    std::vector<RtpPacket> packetize_frame(const std::vector<uint8_t>& frame_data, uint32_t timestamp);

    // Packetize a single NAL unit
    std::vector<RtpPacket> packetize_nalu(const Bytes& nalu, u32 timestamp);

private:
    std::vector<RtpPacket> create_single_nalu_packet(const Bytes& nalu, u32 timestamp);
    std::vector<RtpPacket> create_fu_a_packets(const Bytes& nalu, u32 timestamp);
    std::vector<Bytes> extract_nalus(const Bytes& frame);
    
    NalType get_nal_type(u8 nal_header) const;
    bool is_small_nalu(const Bytes& nalu) const;
    bool is_key_frame(const Bytes& nalu) const;

    uint32_t m_ssrc;
    size_t m_mtu;
    uint16_t m_sequence = 0;
};

} // namespace video_streaming