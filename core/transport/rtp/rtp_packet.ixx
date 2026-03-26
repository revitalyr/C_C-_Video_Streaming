module;

#include <cstdint>
#include <vector>
#include <span>

export module video_streaming.rtp.packet;

import video_streaming.common.types;

namespace video_streaming {

export struct RtpHeader {
    uint8_t version : 2;
    uint8_t padding : 1;
    uint8_t extension : 1;
    uint8_t csrc_count : 4;
    uint8_t marker : 1;
    uint8_t payload_type : 7;
    
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
    
    RtpHeader() : version(RTP_VERSION), padding(0), extension(0), 
                  csrc_count(0), marker(0), payload_type(RTP_PAYLOAD_TYPE_H264),
                  sequence_number(0), timestamp(0), ssrc(0) {}
};

export struct RtpPacket {
    RtpHeader header;
    Bytes payload;
    
    RtpPacket() = default;
    RtpPacket(int ssrc, uint16_t sequence, uint32_t timestamp, uint8_t payload_type);
    
    // Serialize full packet (header + payload)
    Bytes serialize() const;
    
    // Deserialize full packet
    bool deserialize(std::span<const uint8_t> data);
    
    size_t size() const { return RTP_HEADER_SIZE + payload.size(); }
    
    bool is_valid() const;
};

// RTP serialization utilities
export class RtpSerializer {
public:
    static Bytes serialize_header(const RtpHeader& header);
    static RtpHeader deserialize_header(const Bytes& data);
    
    static u16 read_u16(const Bytes& data, size_t offset);
    static u32 read_u32(const Bytes& data, size_t offset);
    static void write_u16(Bytes& data, size_t offset, u16 value);
    static void write_u32(Bytes& data, size_t offset, u32 value);
};

} // namespace video_streaming