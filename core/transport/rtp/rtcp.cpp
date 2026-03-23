#include "rtcp.hpp"
#include <cstring>

RtcpPacket::RtcpPacket(RtcpPacketType type, u32 ssrc) 
    : m_ssrc(ssrc) {
    m_header.version = 2;
    m_header.padding = 0;
    m_header.count = 0;
    m_header.packet_type = type;
    m_header.length = 0;
    m_valid = true;
}

bool RtcpPacket::is_valid() const {
    return m_valid && 
           m_header.version == 2 &&
           (m_header.length * 4 + 4) <= 1500; // Max RTCP packet size
}

Bytes RtcpPacket::serialize() const {
    switch (m_header.packet_type) {
        case RtcpPacketType::SenderReport:
            return serialize_sender_report();
        case RtcpPacketType::ReceiverReport:
            return serialize_receiver_report();
        default:
            return {};
    }
}

bool RtcpPacket::deserialize(const Bytes& data) {
    if (data.size() < 8) { // Minimum RTCP header size
        return false;
    }
    
    // Parse header
    m_header.version = (data[0] >> 6) & 0x03;
    m_header.padding = (data[0] >> 5) & 0x01;
    m_header.count = data[0] & 0x1F;
    m_header.packet_type = static_cast<RtcpPacketType>(data[1]);
    m_header.length = (static_cast<u16>(data[2]) << 8) | data[3];
    
    // Validate header
    if (m_header.version != 2) {
        return false;
    }
    
    size_t expected_size = (m_header.length + 1) * 4;
    if (data.size() < expected_size) {
        return false;
    }
    
    // Extract SSRC
    m_ssrc = (static_cast<u32>(data[4]) << 24) |
              (static_cast<u32>(data[5]) << 16) |
              (static_cast<u32>(data[6]) << 8) |
              static_cast<u32>(data[7]);
    
    // Parse based on type
    switch (m_header.packet_type) {
        case RtcpPacketType::SenderReport:
            return deserialize_sender_report(data);
        case RtcpPacketType::ReceiverReport:
            return deserialize_receiver_report(data);
        default:
            m_valid = false;
            return false;
    }
}

void RtcpPacket::set_sender_report(const SenderReport& sr) {
    m_sender_report = sr;
    m_header.packet_type = RtcpPacketType::SenderReport;
    m_header.count = 0;
    m_header.length = 6; // SR header + 5 32-bit words
    m_ssrc = sr.ssrc;
    m_valid = true;
}

void RtcpPacket::set_receiver_report(const ReceiverReport& rr) {
    m_receiver_report = rr;
    m_header.packet_type = RtcpPacketType::ReceiverReport;
    m_header.count = static_cast<u8>(rr.report_blocks.size());
    m_header.length = 1 + (6 * rr.report_blocks.size()); // RR header + blocks
    m_ssrc = rr.ssrc;
    m_valid = true;
}

Bytes RtcpPacket::serialize_sender_report() const {
    Bytes data;
    data.reserve(28); // SR packet size
    
    // Header (8 bytes)
    data.push_back((m_header.version << 6) | (m_header.padding << 5) | m_header.count);
    data.push_back(static_cast<u8>(m_header.packet_type));
    data.push_back((m_header.length >> 8) & 0xFF);
    data.push_back(m_header.length & 0xFF);
    
    // SSRC (4 bytes)
    data.push_back((m_sender_report.ssrc >> 24) & 0xFF);
    data.push_back((m_sender_report.ssrc >> 16) & 0xFF);
    data.push_back((m_sender_report.ssrc >> 8) & 0xFF);
    data.push_back(m_sender_report.ssrc & 0xFF);
    
    // NTP timestamp (8 bytes)
    u64 ntp = m_sender_report.ntp_timestamp;
    data.push_back((ntp >> 56) & 0xFF);
    data.push_back((ntp >> 48) & 0xFF);
    data.push_back((ntp >> 40) & 0xFF);
    data.push_back((ntp >> 32) & 0xFF);
    data.push_back((ntp >> 24) & 0xFF);
    data.push_back((ntp >> 16) & 0xFF);
    data.push_back((ntp >> 8) & 0xFF);
    data.push_back(ntp & 0xFF);
    
    // RTP timestamp (4 bytes)
    u32 rtp = m_sender_report.rtp_timestamp;
    data.push_back((rtp >> 24) & 0xFF);
    data.push_back((rtp >> 16) & 0xFF);
    data.push_back((rtp >> 8) & 0xFF);
    data.push_back(rtp & 0xFF);
    
    // Packet count (4 bytes)
    u32 count = m_sender_report.packet_count;
    data.push_back((count >> 24) & 0xFF);
    data.push_back((count >> 16) & 0xFF);
    data.push_back((count >> 8) & 0xFF);
    data.push_back(count & 0xFF);
    
    // Octet count (4 bytes)
    u32 octets = m_sender_report.octet_count;
    data.push_back((octets >> 24) & 0xFF);
    data.push_back((octets >> 16) & 0xFF);
    data.push_back((octets >> 8) & 0xFF);
    data.push_back(octets & 0xFF);
    
    return data;
}

Bytes RtcpPacket::serialize_receiver_report() const {
    Bytes data;
    data.reserve(8 + 24 * m_receiver_report.report_blocks.size());
    
    // Header (8 bytes)
    data.push_back((m_header.version << 6) | (m_header.padding << 5) | m_header.count);
    data.push_back(static_cast<u8>(m_header.packet_type));
    data.push_back((m_header.length >> 8) & 0xFF);
    data.push_back(m_header.length & 0xFF);
    
    // SSRC (4 bytes)
    data.push_back((m_receiver_report.ssrc >> 24) & 0xFF);
    data.push_back((m_receiver_report.ssrc >> 16) & 0xFF);
    data.push_back((m_receiver_report.ssrc >> 8) & 0xFF);
    data.push_back(m_receiver_report.ssrc & 0xFF);
    
    // Report blocks
    for (const auto& block : m_receiver_report.report_blocks) {
        // SSRC (4 bytes)
        data.push_back((block.ssrc >> 24) & 0xFF);
        data.push_back((block.ssrc >> 16) & 0xFF);
        data.push_back((block.ssrc >> 8) & 0xFF);
        data.push_back(block.ssrc & 0xFF);
        
        // Fraction lost + cumulative lost (4 bytes)
        data.push_back(block.fraction_lost);
        data.push_back((block.packets_lost_24bit >> 16) & 0xFF);
        data.push_back((block.packets_lost_24bit >> 8) & 0xFF);
        data.push_back(block.packets_lost_24bit & 0xFF);
        
        // Highest sequence (4 bytes)
        data.push_back((block.highest_sequence >> 24) & 0xFF);
        data.push_back((block.highest_sequence >> 16) & 0xFF);
        data.push_back((block.highest_sequence >> 8) & 0xFF);
        data.push_back(block.highest_sequence & 0xFF);
        
        // Jitter (4 bytes)
        data.push_back((block.jitter >> 24) & 0xFF);
        data.push_back((block.jitter >> 16) & 0xFF);
        data.push_back((block.jitter >> 8) & 0xFF);
        data.push_back(block.jitter & 0xFF);
        
        // Last SR timestamp (4 bytes)
        data.push_back((block.last_sr_timestamp >> 24) & 0xFF);
        data.push_back((block.last_sr_timestamp >> 16) & 0xFF);
        data.push_back((block.last_sr_timestamp >> 8) & 0xFF);
        data.push_back(block.last_sr_timestamp & 0xFF);
        
        // Delay since last SR (4 bytes)
        data.push_back((block.delay_since_last_sr >> 24) & 0xFF);
        data.push_back((block.delay_since_last_sr >> 16) & 0xFF);
        data.push_back((block.delay_since_last_sr >> 8) & 0xFF);
        data.push_back(block.delay_since_last_sr & 0xFF);
    }
    
    return data;
}

bool RtcpPacket::deserialize_sender_report(const Bytes& data) {
    if (data.size() < 28) { // Minimum SR packet size
        return false;
    }
    
    size_t offset = 8; // Skip header and SSRC
    
    // NTP timestamp
    m_sender_report.ntp_timestamp = 
        (static_cast<u64>(data[offset]) << 56) |
        (static_cast<u64>(data[offset + 1]) << 48) |
        (static_cast<u64>(data[offset + 2]) << 40) |
        (static_cast<u64>(data[offset + 3]) << 32) |
        (static_cast<u64>(data[offset + 4]) << 24) |
        (static_cast<u64>(data[offset + 5]) << 16) |
        (static_cast<u64>(data[offset + 6]) << 8) |
        static_cast<u64>(data[offset + 7]);
    offset += 8;
    
    // RTP timestamp
    m_sender_report.rtp_timestamp = 
        (static_cast<u32>(data[offset]) << 24) |
        (static_cast<u32>(data[offset + 1]) << 16) |
        (static_cast<u32>(data[offset + 2]) << 8) |
        static_cast<u32>(data[offset + 3]);
    offset += 4;
    
    // Packet count
    m_sender_report.packet_count = 
        (static_cast<u32>(data[offset]) << 24) |
        (static_cast<u32>(data[offset + 1]) << 16) |
        (static_cast<u32>(data[offset + 2]) << 8) |
        static_cast<u32>(data[offset + 3]);
    offset += 4;
    
    // Octet count
    m_sender_report.octet_count = 
        (static_cast<u32>(data[offset]) << 24) |
        (static_cast<u32>(data[offset + 1]) << 16) |
        (static_cast<u32>(data[offset + 2]) << 8) |
        static_cast<u32>(data[offset + 3]);
    
    m_sender_report.ssrc = m_ssrc;
    m_valid = true;
    return true;
}

bool RtcpPacket::deserialize_receiver_report(const Bytes& data) {
    size_t expected_size = 8 + (24 * m_header.count);
    if (data.size() < expected_size) {
        return false;
    }
    
    m_receiver_report.ssrc = m_ssrc;
    m_receiver_report.report_blocks.clear();
    
    size_t offset = 8; // Skip header and SSRC
    
    for (u8 i = 0; i < m_header.count; ++i) {
        RrBlock block;
        
        // SSRC
        block.ssrc = 
            (static_cast<u32>(data[offset]) << 24) |
            (static_cast<u32>(data[offset + 1]) << 16) |
            (static_cast<u32>(data[offset + 2]) << 8) |
            static_cast<u32>(data[offset + 3]);
        offset += 4;
        
        // Fraction lost + cumulative lost
        block.fraction_lost = data[offset];
        block.packets_lost_24bit = 
            (static_cast<u32>(data[offset + 1]) << 16) |
            (static_cast<u32>(data[offset + 2]) << 8) |
            static_cast<u32>(data[offset + 3]);
        offset += 4;
        
        // Highest sequence
        block.highest_sequence = 
            (static_cast<u32>(data[offset]) << 24) |
            (static_cast<u32>(data[offset + 1]) << 16) |
            (static_cast<u32>(data[offset + 2]) << 8) |
            static_cast<u32>(data[offset + 3]);
        offset += 4;
        
        // Jitter
        block.jitter = 
            (static_cast<u32>(data[offset]) << 24) |
            (static_cast<u32>(data[offset + 1]) << 16) |
            (static_cast<u32>(data[offset + 2]) << 8) |
            static_cast<u32>(data[offset + 3]);
        offset += 4;
        
        // Last SR timestamp
        block.last_sr_timestamp = 
            (static_cast<u32>(data[offset]) << 24) |
            (static_cast<u32>(data[offset + 1]) << 16) |
            (static_cast<u32>(data[offset + 2]) << 8) |
            static_cast<u32>(data[offset + 3]);
        offset += 4;
        
        // Delay since last SR
        block.delay_since_last_sr = 
            (static_cast<u32>(data[offset]) << 24) |
            (static_cast<u32>(data[offset + 1]) << 16) |
            (static_cast<u32>(data[offset + 2]) << 8) |
            static_cast<u32>(data[offset + 3]);
        offset += 4;
        
        m_receiver_report.report_blocks.push_back(block);
    }
    
    m_valid = true;
    return true;
}

void RtcpBuilder::add_sender_report(const SenderReport& sr) {
    RtcpPacket packet(RtcpPacketType::SenderReport, sr.ssrc);
    packet.set_sender_report(sr);
    m_packets.push_back(std::move(packet));
}

void RtcpBuilder::add_receiver_report(const ReceiverReport& rr) {
    RtcpPacket packet(RtcpPacketType::ReceiverReport, rr.ssrc);
    packet.set_receiver_report(rr);
    m_packets.push_back(std::move(packet));
}

void RtcpBuilder::add_bye_packet(u32 ssrc) {
    RtcpPacket packet(RtcpPacketType::Bye, ssrc);
    m_packets.push_back(std::move(packet));
}

Bytes RtcpBuilder::build() const {
    Bytes compound;
    
    for (const auto& packet : m_packets) {
        auto packet_data = packet.serialize();
        compound.insert(compound.end(), packet_data.begin(), packet_data.end());
    }
    
    return compound;
}

void RtcpBuilder::clear() {
    m_packets.clear();
}
