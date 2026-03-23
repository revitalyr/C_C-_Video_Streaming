#pragma once

#include "rtp_packet.hpp"
#include "common/types.hpp"
#include <vector>

// RTCP Packet Types
enum class RtcpPacketType : u8 {
    SenderReport = 200,
    ReceiverReport = 201,
    SourceDescription = 202,
    Bye = 203,
    App = 204
};

// RTCP Header
struct RtcpHeader {
    u8 version : 2;
    u8 padding : 1;
    u8 count : 5;
    RtcpPacketType packet_type;
    u16 length; // In 32-bit words, not including header
    
    RtcpHeader() : version(2), padding(0), count(0), 
                   packet_type(RtcpPacketType::ReceiverReport), length(0) {}
};

// Receiver Report Block
struct RrBlock {
    u32 ssrc;
    u8 fraction_lost;
    u32 packets_lost_24bit : 24;
    u32 highest_sequence;
    u32 jitter;
    u32 last_sr_timestamp;
    u32 delay_since_last_sr;
    
    RrBlock() : ssrc(0), fraction_lost(0), packets_lost_24bit(0),
                 highest_sequence(0), jitter(0), last_sr_timestamp(0), 
                 delay_since_last_sr(0) {}
};

// Sender Report
struct SenderReport {
    u32 ssrc;
    u64 ntp_timestamp;
    u32 rtp_timestamp;
    u32 packet_count;
    u32 octet_count;
    
    SenderReport() : ssrc(0), ntp_timestamp(0), rtp_timestamp(0),
                    packet_count(0), octet_count(0) {}
};

// Receiver Report
struct ReceiverReport {
    u32 ssrc;
    std::vector<RrBlock> report_blocks;
    
    ReceiverReport() : ssrc(0) {}
};

class RtcpPacket {
public:
    RtcpPacket() = default;
    RtcpPacket(RtcpPacketType type, u32 ssrc);
    
    bool is_valid() const;
    Bytes serialize() const;
    bool deserialize(const Bytes& data);
    
    // Accessors
    RtcpPacketType get_type() const { return m_header.packet_type; }
    u32 get_ssrc() const { return m_ssrc; }
    
    // Sender Report accessors
    const SenderReport& get_sender_report() const { return m_sender_report; }
    void set_sender_report(const SenderReport& sr);
    
    // Receiver Report accessors
    const ReceiverReport& get_receiver_report() const { return m_receiver_report; }
    void set_receiver_report(const ReceiverReport& rr);

private:
    Bytes serialize_sender_report() const;
    Bytes serialize_receiver_report() const;
    bool deserialize_sender_report(const Bytes& data);
    bool deserialize_receiver_report(const Bytes& data);

private:
    RtcpHeader m_header;
    u32 m_ssrc{0};
    SenderReport m_sender_report;
    ReceiverReport m_receiver_report;
    bool m_valid{false};
};

// RTCP Compound Packet Builder
class RtcpBuilder {
public:
    RtcpBuilder() = default;
    
    void add_sender_report(const SenderReport& sr);
    void add_receiver_report(const ReceiverReport& rr);
    void add_bye_packet(u32 ssrc);
    
    Bytes build() const;
    void clear();

private:
    std::vector<RtcpPacket> m_packets;
};
