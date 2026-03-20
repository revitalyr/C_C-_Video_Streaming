#pragma once

#include "rtp_packet.hpp"
#include "h264_packetizer.hpp"
#include <map>
#include <vector>

class H264Depacketizer {
public:
    H264Depacketizer() = default;
    ~H264Depacketizer() = default;
    
    // Process incoming RTP packet and return complete frames
    std::vector<Bytes> process_packet(const RtpPacket& packet);
    
    // Get statistics
    size_t get_lost_packets() const { return m_lost_packets; }
    size_t get_reordered_packets() const { return m_reordered_packets; }
    size_t get_complete_frames() const { return m_complete_frames; }
    
    // Reset state
    void reset();

private:
    struct FuState {
        u16 start_sequence;
        Bytes payload;
        bool started{false};
        NalType nal_type{NalType::Unspecified};
        u8 fu_indicator{0};
    };
    
    Bytes process_single_nalu(const RtpPacket& packet);
    Bytes process_fu_a_packet(const RtpPacket& packet);
    bool is_fu_a_complete(const FuState& state) const;
    Bytes assemble_fu_a_frame(const FuState& state);
    
    void update_statistics(const RtpPacket& packet);
    bool is_packet_in_order(u16 sequence) const;

private:
    // FU-A reassembly state
    std::map<u16, FuState> m_fu_sessions;
    
    // Expected sequence number
    u16 m_expected_sequence{0};
    
    // Statistics
    size_t m_lost_packets{0};
    size_t m_reordered_packets{0};
    size_t m_complete_frames{0};
    
    // Configuration
    static constexpr size_t MAX_FU_SESSIONS = 8;
    static constexpr Milliseconds FU_SESSION_TIMEOUT{5000};
};
