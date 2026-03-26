module;

#include <cstring>
#include <vector>
#include <cstdint>
#include <arpa/inet.h>

module video_streaming.rtp.rtcp;

import video_streaming.common.types;

namespace video_streaming {

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
    return m_valid;
}

RtcpHeader RtcpPacket::get_header() const {
    return m_header;
}

u32 RtcpPacket::get_ssrc() const {
    return m_ssrc;
}

const std::vector<u8>& RtcpPacket::get_payload() const {
    return m_payload;
}

void RtcpPacket::add_payload(const std::vector<u8>& data) {
    m_payload.insert(m_payload.end(), data.begin(), data.end());
    m_header.length = (m_payload.size() + 4) / 4; // Convert to 32-bit words
}

std::vector<u8> RtcpPacket::serialize() const {
    std::vector<u8> result;
    
    // Add header
    u8 byte1 = (m_header.version << 6) | (m_header.padding << 5) | m_header.count;
    result.push_back(byte1);
    result.push_back(static_cast<u8>(m_header.packet_type));
    
    // Add length (16-bit, network byte order)
    u16 length_net = htons(m_header.length);
    result.push_back((length_net >> 8) & 0xFF);
    result.push_back(length_net & 0xFF);
    
    // Add SSRC
    u32 ssrc_net = htonl(m_ssrc);
    for (int i = 0; i < 4; ++i) {
        result.push_back((ssrc_net >> (24 - i * 8)) & 0xFF);
    }
    
    // Add payload
    result.insert(result.end(), m_payload.begin(), m_payload.end());
    
    return result;
}

bool RtcpPacket::deserialize(const std::vector<u8>& data) {
    if (data.size() < 8) {
        return false;
    }
    
    // Parse header
    u8 byte1 = data[0];
    m_header.version = (byte1 >> 6) & 0x03;
    m_header.padding = (byte1 >> 5) & 0x01;
    m_header.count = byte1 & 0x1F;
    m_header.packet_type = static_cast<RtcpPacketType>(data[1]);
    m_header.length = (data[2] << 8) | data[3];
    
    // Parse SSRC
    m_ssrc = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
    
    // Parse payload
    size_t payload_size = m_header.length * 4 - 4; // Convert from 32-bit words and subtract header
    if (data.size() >= 8 + payload_size) {
        m_payload.assign(data.begin() + 8, data.begin() + 8 + payload_size);
    }
    
    m_valid = true;
    return true;
}

} // namespace video_streaming
