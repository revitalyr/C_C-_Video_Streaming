#pragma once

#include "common/types.hpp"

struct RtpHeader {
    u8 version : 2;
    u8 padding : 1;
    u8 extension : 1;
    u8 csrc_count : 4;
    u8 marker : 1;
    u8 payload_type : 7;
    
    u16 sequence_number;
    u32 timestamp;
    u32 ssrc;
    
    RtpHeader() : version(RTP_VERSION), padding(0), extension(0), 
                  csrc_count(0), marker(0), payload_type(RTP_PAYLOAD_TYPE_H264),
                  sequence_number(0), timestamp(0), ssrc(0) {}
};

struct RtpPacket {
    RtpHeader header;
    Bytes payload;
    
    RtpPacket() = default;
    RtpPacket(u32 ssrc, u16 sequence, u32 timestamp, u8 payload_type);
    
    Bytes serialize() const;
    bool deserialize(const Bytes& data);
    
    size_t size() const { return RTP_HEADER_SIZE + payload.size(); }
    
    bool is_valid() const {
        return header.version == RTP_VERSION && 
               header.payload_type == RTP_PAYLOAD_TYPE_H264;
    }
};

// RTP serialization utilities
class RtpSerializer {
public:
    static Bytes serialize_header(const RtpHeader& header);
    static RtpHeader deserialize_header(const Bytes& data);
    
private:
    static u16 read_u16(const Bytes& data, size_t offset);
    static u32 read_u32(const Bytes& data, size_t offset);
    static void write_u16(Bytes& data, size_t offset, u16 value);
    static void write_u32(Bytes& data, size_t offset, u32 value);
};
