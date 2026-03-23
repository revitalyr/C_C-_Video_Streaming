#pragma once

#include "rtp_packet.hpp"
#include <vector>

// H.264 NAL Unit types
enum class NalType : u8 {
    Unspecified = 0,
    Slice = 1,
    SliceDPA = 2,
    SliceDPB = 3,
    SliceDPC = 4,
    IDR = 5,
    SEI = 6,
    SPS = 7,
    PPS = 8,
    AUD = 9,
    EndSequence = 10,
    EndStream = 11,
    Filler = 12,
    SPSEXT = 13,
    Prefix = 14,
    SubsetSPS = 15,
    DPS = 16,
    Reserved17 = 17,
    Reserved18 = 18,
    Reserved19 = 19,
    Reserved20 = 20,
    Reserved21 = 21,
    Reserved22 = 22,
    Reserved23 = 23,
    STAPA = 24,
    STAPB = 25,
    MTAP16 = 26,
    MTAP24 = 27,
    FU_A = 28,
    FU_B = 29
};

class H264Packetizer {
public:
    explicit H264Packetizer(u32 ssrc, size_t mtu = DEFAULT_MTU);
    
    std::vector<RtpPacket> packetize_nalu(const Bytes& nalu, u32 timestamp);
    std::vector<RtpPacket> packetize_frame(const Bytes& frame, u32 timestamp);
    
    void set_mtu(size_t mtu) { m_mtu = mtu; }
    size_t get_mtu() const { return m_mtu; }
    
    u16 get_next_sequence() { return m_sequence++; }

private:
    std::vector<RtpPacket> create_single_nalu_packet(const Bytes& nalu, u32 timestamp);
    std::vector<RtpPacket> create_fu_a_packets(const Bytes& nalu, u32 timestamp);
    std::vector<Bytes> extract_nalus(const Bytes& frame);
    
    NalType get_nal_type(u8 nal_header) const;
    bool is_small_nalu(const Bytes& nalu) const;
    bool is_key_frame(const Bytes& nalu) const;

private:
    u32 m_ssrc;
    u16 m_sequence;
    size_t m_mtu;
    
    static constexpr u8 FU_A_INDICATOR = 28;
    static constexpr size_t MAX_PAYLOAD_SIZE = DEFAULT_MTU - RTP_HEADER_SIZE - 2; // FU headers
};
