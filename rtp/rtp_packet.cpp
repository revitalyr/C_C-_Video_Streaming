#include "rtp_packet.hpp"
#include <cstring>

RtpPacket::RtpPacket(u32 ssrc, u16 sequence, u32 timestamp, u8 payload_type) {
    header.version = RTP_VERSION;
    header.payload_type = payload_type;
    header.sequence_number = sequence;
    header.timestamp = timestamp;
    header.ssrc = ssrc;
}

Bytes RtpPacket::serialize() const {
    Bytes data;
    data.reserve(size());
    
    // Serialize header
    Bytes header_data = RtpSerializer::serialize_header(header);
    data.insert(data.end(), header_data.begin(), header_data.end());
    
    // Add payload
    data.insert(data.end(), payload.begin(), payload.end());
    
    return data;
}

bool RtpPacket::deserialize(const Bytes& data) {
    if (data.size() < RTP_HEADER_SIZE) {
        return false;
    }
    
    header = RtpSerializer::deserialize_header(data);
    
    if (data.size() > RTP_HEADER_SIZE) {
        payload.assign(data.begin() + RTP_HEADER_SIZE, data.end());
    } else {
        payload.clear();
    }
    
    return is_valid();
}

Bytes RtpSerializer::serialize_header(const RtpHeader& header) {
    Bytes data(RTP_HEADER_SIZE, 0);
    
    // First byte: V(2) + P(1) + X(1) + CC(4)
    data[0] = (header.version << 6) | (header.padding << 5) | 
              (header.extension << 4) | header.csrc_count;
    
    // Second byte: M(1) + PT(7)
    data[1] = (header.marker << 7) | header.payload_type;
    
    // Sequence number (16 bits)
    write_u16(data, 2, header.sequence_number);
    
    // Timestamp (32 bits)
    write_u32(data, 4, header.timestamp);
    
    // SSRC (32 bits)
    write_u32(data, 8, header.ssrc);
    
    return data;
}

RtpHeader RtpSerializer::deserialize_header(const Bytes& data) {
    RtpHeader header;
    
    if (data.size() < RTP_HEADER_SIZE) {
        return header;
    }
    
    // Parse first byte
    header.version = (data[0] >> 6) & 0x03;
    header.padding = (data[0] >> 5) & 0x01;
    header.extension = (data[0] >> 4) & 0x01;
    header.csrc_count = data[0] & 0x0F;
    
    // Parse second byte
    header.marker = (data[1] >> 7) & 0x01;
    header.payload_type = data[1] & 0x7F;
    
    // Parse sequence number
    header.sequence_number = read_u16(data, 2);
    
    // Parse timestamp
    header.timestamp = read_u32(data, 4);
    
    // Parse SSRC
    header.ssrc = read_u32(data, 8);
    
    return header;
}

u16 RtpSerializer::read_u16(const Bytes& data, size_t offset) {
    return static_cast<u16>((static_cast<u16>(data[offset]) << 8) | 
                           static_cast<u16>(data[offset + 1]));
}

u32 RtpSerializer::read_u32(const Bytes& data, size_t offset) {
    return (static_cast<u32>(data[offset]) << 24) |
           (static_cast<u32>(data[offset + 1]) << 16) |
           (static_cast<u32>(data[offset + 2]) << 8) |
           static_cast<u32>(data[offset + 3]);
}

void RtpSerializer::write_u16(Bytes& data, size_t offset, u16 value) {
    data[offset] = static_cast<u8>((value >> 8) & 0xFF);
    data[offset + 1] = static_cast<u8>(value & 0xFF);
}

void RtpSerializer::write_u32(Bytes& data, size_t offset, u32 value) {
    data[offset] = static_cast<u8>((value >> 24) & 0xFF);
    data[offset + 1] = static_cast<u8>((value >> 16) & 0xFF);
    data[offset + 2] = static_cast<u8>((value >> 8) & 0xFF);
    data[offset + 3] = static_cast<u8>(value & 0xFF);
}
