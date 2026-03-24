module;

#include <vector>
#include <cstdint>
#include <optional>
#include <map>

import video_streaming.rtp.packet;
import video_streaming.media.frame;
import video_streaming.common.types;

export module video_streaming.rtp.h264_depacketizer;

namespace video_streaming {

export class H264Depacketizer {
public:
    struct Stats {
        uint64_t packets_processed = 0;
        uint64_t frames_assembled = 0;
        uint64_t packets_lost = 0;
        uint64_t packets_reordered = 0;
    };
    
    struct FuState {
        u16 start_sequence;
        Bytes payload;
        NalType nal_type;
        u8 fu_indicator;
        bool started{false};
    };

    H264Depacketizer() = default;

    // Process an RTP packet and return assembled NAL units if any are complete
    std::vector<EncodedFrame> process_packet(const RtpPacket& packet);

    Stats get_stats() const { return m_stats; }
    void reset();

private:
    void update_statistics(const RtpPacket& packet);
    bool is_packet_in_order(u16 sequence) const;
    
    EncodedFrame process_fu_a_packet(const RtpPacket& packet);
    EncodedFrame process_single_nalu(const RtpPacket& packet);
    
    bool is_fu_a_complete(const FuState& state) const;
    EncodedFrame assemble_fu_a_frame(const FuState& state);

private:
    std::map<u16, FuState> m_fu_sessions; // sequence -> state
    u16 m_expected_sequence{0};
    
    // Stats members
    uint64_t m_lost_packets = 0;
    uint64_t m_reordered_packets = 0;
    uint64_t m_complete_frames = 0;
    
    Stats m_stats;
};

} // namespace video_streaming