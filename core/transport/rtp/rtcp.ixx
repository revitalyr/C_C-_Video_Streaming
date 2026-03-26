module;

#include <cstdint>
#include <vector>

export module video_streaming.rtp.rtcp;

import video_streaming.common.types;

namespace video_streaming {

export enum class RtcpPacketType : u8 {
    SR = 200,  // Sender Report
    RR = 201,  // Receiver Report
    SDES = 202, // Source Description
    BYE = 203,  // Goodbye
    APP = 204   // Application-defined
};

export struct RtcpHeader {
    u8 version : 2;
    u8 padding : 1;
    u8 count : 5;
    RtcpPacketType packet_type;
    u16 length;
};

export class RtcpPacket {
private:
    RtcpHeader m_header;
    u32 m_ssrc;
    std::vector<u8> m_payload;
    bool m_valid = false;

public:
    RtcpPacket(RtcpPacketType type, u32 ssrc);
    
    bool is_valid() const;
    RtcpHeader get_header() const;
    u32 get_ssrc() const;
    const std::vector<u8>& get_payload() const;
    
    void add_payload(const std::vector<u8>& data);
    std::vector<u8> serialize() const;
    bool deserialize(const std::vector<u8>& data);
};

} // namespace video_streaming
