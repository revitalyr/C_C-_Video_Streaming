module;

#include <cstdint>
#include <vector>
#include <span>
#include <cstring>

module video_streaming.rtp.packet;

import video_streaming.common.types;

namespace video_streaming {

RtpPacket::RtpPacket(int ssrc, uint16_t sequence, uint32_t timestamp, uint8_t payload_type) {
    header.version = 2;
    header.padding = 0;
    header.extension = 0;
    header.csrc_count = 0;
    header.marker = 0;
    header.payload_type = payload_type;
    header.sequence_number = sequence;
    header.timestamp = timestamp;
    header.ssrc = static_cast<uint32_t>(ssrc);
}

Bytes RtpPacket::serialize() const {
    Bytes result;
    result.resize(size());
    
    // Serialize header
    result[0] = (header.version << 6) | (header.padding << 5) | 
                (header.extension << 4) | header.csrc_count;
    result[1] = (header.marker << 7) | header.payload_type;
    
    // Write sequence number (big-endian)
    result[2] = (header.sequence_number >> 8) & 0xFF;
    result[3] = header.sequence_number & 0xFF;
    
    // Write timestamp (big-endian)
    result[4] = (header.timestamp >> 24) & 0xFF;
    result[5] = (header.timestamp >> 16) & 0xFF;
    result[6] = (header.timestamp >> 8) & 0xFF;
    result[7] = header.timestamp & 0xFF;
    
    // Write SSRC (big-endian)
    result[8] = (header.ssrc >> 24) & 0xFF;
    result[9] = (header.ssrc >> 16) & 0xFF;
    result[10] = (header.ssrc >> 8) & 0xFF;
    result[11] = header.ssrc & 0xFF;
    
    // Copy payload
    std::memcpy(result.data() + 12, payload.data(), payload.size());
    
    return result;
}

bool RtpPacket::deserialize(std::span<const uint8_t> data) {
    if (data.size() < 12) return false; // Minimum RTP header size
    
    // Parse header
    header.version = (data[0] >> 6) & 0x03;
    header.padding = (data[0] >> 5) & 0x01;
    header.extension = (data[0] >> 4) & 0x01;
    header.csrc_count = data[0] & 0x0F;
    header.marker = (data[1] >> 7) & 0x01;
    header.payload_type = data[1] & 0x7F;
    
    header.sequence_number = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    header.timestamp = (static_cast<uint32_t>(data[4]) << 24) | 
                     (static_cast<uint32_t>(data[5]) << 16) |
                     (static_cast<uint32_t>(data[6]) << 8) | 
                     static_cast<uint32_t>(data[7]);
    header.ssrc = (static_cast<uint32_t>(data[8]) << 24) | 
                  (static_cast<uint32_t>(data[9]) << 16) |
                  (static_cast<uint32_t>(data[10]) << 8) | 
                  static_cast<uint32_t>(data[11]);
    
    // Copy payload
    size_t payload_size = data.size() - 12;
    payload.resize(payload_size);
    std::memcpy(payload.data(), data.data() + 12, payload_size);
    
    return true;
}

bool RtpPacket::is_valid() const {
    return header.version == 2 && !payload.empty();
}

// RtpSerializer implementation
Bytes RtpSerializer::serialize_header(const RtpHeader& header) {
    Bytes result(12);
    
    result[0] = (header.version << 6) | (header.padding << 5) | 
                (header.extension << 4) | header.csrc_count;
    result[1] = (header.marker << 7) | header.payload_type;
    
    result[2] = (header.sequence_number >> 8) & 0xFF;
    result[3] = header.sequence_number & 0xFF;
    
    result[4] = (header.timestamp >> 24) & 0xFF;
    result[5] = (header.timestamp >> 16) & 0xFF;
    result[6] = (header.timestamp >> 8) & 0xFF;
    result[7] = header.timestamp & 0xFF;
    
    result[8] = (header.ssrc >> 24) & 0xFF;
    result[9] = (header.ssrc >> 16) & 0xFF;
    result[10] = (header.ssrc >> 8) & 0xFF;
    result[11] = header.ssrc & 0xFF;
    
    return result;
}

RtpHeader RtpSerializer::deserialize_header(const Bytes& data) {
    RtpHeader header{};
    
    if (data.size() < 12) return header;
    
    header.version = (data[0] >> 6) & 0x03;
    header.padding = (data[0] >> 5) & 0x01;
    header.extension = (data[0] >> 4) & 0x01;
    header.csrc_count = data[0] & 0x0F;
    header.marker = (data[1] >> 7) & 0x01;
    header.payload_type = data[1] & 0x7F;
    
    header.sequence_number = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    header.timestamp = (static_cast<uint32_t>(data[4]) << 24) | 
                     (static_cast<uint32_t>(data[5]) << 16) |
                     (static_cast<uint32_t>(data[6]) << 8) | 
                     static_cast<uint32_t>(data[7]);
    header.ssrc = (static_cast<uint32_t>(data[8]) << 24) | 
                  (static_cast<uint32_t>(data[9]) << 16) |
                  (static_cast<uint32_t>(data[10]) << 8) | 
                  static_cast<uint32_t>(data[11]);
    
    return header;
}

u16 RtpSerializer::read_u16(const Bytes& data, size_t offset) {
    if (offset + 2 > data.size()) return 0;
    return (static_cast<u16>(data[offset]) << 8) | data[offset + 1];
}

u32 RtpSerializer::read_u32(const Bytes& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    return (static_cast<u32>(data[offset]) << 24) | 
           (static_cast<u32>(data[offset + 1]) << 16) |
           (static_cast<u32>(data[offset + 2]) << 8) | 
           static_cast<u32>(data[offset + 3]);
}

void RtpSerializer::write_u16(Bytes& data, size_t offset, u16 value) {
    if (offset + 2 > data.size()) return;
    data[offset] = (value >> 8) & 0xFF;
    data[offset + 1] = value & 0xFF;
}

void RtpSerializer::write_u32(Bytes& data, size_t offset, u32 value) {
    if (offset + 4 > data.size()) return;
    data[offset] = (value >> 24) & 0xFF;
    data[offset + 1] = (value >> 16) & 0xFF;
    data[offset + 2] = (value >> 8) & 0xFF;
    data[offset + 3] = value & 0xFF;
}

} // namespace video_streaming
