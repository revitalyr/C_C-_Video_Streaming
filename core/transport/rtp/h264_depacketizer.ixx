module;

#include <vector>
#include <cstdint>
#include <optional>
#include <map>

#include "rtp/rtp_packet.hpp"

export module video_streaming.rtp.h264_depacketizer;

namespace video_streaming {

export struct EncodedFrame {
    std::vector<uint8_t> data;
    uint32_t timestamp;
    bool is_keyframe;
};

export class H264Depacketizer {
public:
    struct Stats {
        uint64_t packets_processed = 0;
        uint64_t frames_assembled = 0;
        uint64_t packets_lost = 0;
        uint64_t packets_reordered = 0;
    };

    H264Depacketizer() = default;

    // Process an RTP packet and return assembled NAL units if any are complete
    std::vector<EncodedFrame> process_packet(const RtpPacket& packet);

    Stats get_stats() const { return m_stats; }

private:
    std::map<uint32_t, std::vector<uint8_t>> m_fragments; // timestamp -> pending data
    Stats m_stats;
};

} // namespace video_streaming