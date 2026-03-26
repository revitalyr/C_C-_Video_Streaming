module;

#include <cstring>
#include <vector>
#include <algorithm>

module video_streaming.rtp.h264_packetizer;
import video_streaming.common.types;
import video_streaming.media.frame; // For NalType and constants
import video_streaming.rtp.packet;

namespace video_streaming {

H264Packetizer::H264Packetizer(u32 ssrc, size_t mtu) 
    : m_ssrc(ssrc), m_sequence(0), m_mtu(mtu) {}

std::vector<RtpPacket> H264Packetizer::packetize_nalu(const Bytes& nalu, u32 timestamp) {
    if (nalu.empty()) {
        return {};
    }
    
    if (is_small_nalu(nalu)) {
        return create_single_nalu_packet(nalu, timestamp);
    } else {
        return create_fu_a_packets(nalu, timestamp);
    }
}

std::vector<RtpPacket> H264Packetizer::packetize_frame(const Bytes& frame, u32 timestamp) {
    auto nalus = extract_nalus(frame);
    std::vector<RtpPacket> packets;
    
    for (const auto& nalu : nalus) {
        auto nalu_packets = packetize_nalu(nalu, timestamp);
        packets.insert(packets.end(), nalu_packets.begin(), nalu_packets.end());
    }
    
    // Set marker bit on last packet of frame
    if (!packets.empty()) {
        packets.back().header.marker = 1;
    }
    
    return packets;
}

std::vector<RtpPacket> H264Packetizer::create_single_nalu_packet(const Bytes& nalu, u32 timestamp) {
    std::vector<RtpPacket> packets;
    
    RtpPacket packet(static_cast<int>(m_ssrc), m_sequence++, timestamp, RTP_PAYLOAD_TYPE_H264);
    packet.payload = nalu;
    
    packets.push_back(std::move(packet));
    return packets;
}

std::vector<RtpPacket> H264Packetizer::create_fu_a_packets(const Bytes& nalu, u32 timestamp) {
    std::vector<RtpPacket> packets;
    
    if (nalu.empty()) {
        return packets;
    }
    
    u8 nal_header = nalu[0];
    u8 nal_type = nal_header & 0x1F;
    u8 nal_f = nal_header & 0x80;
    u8 nal_nri = nal_header & 0x60;
    
    // FU Indicator header
    u8 fu_indicator = nal_f | nal_nri | FU_A_INDICATOR;
    
    size_t max_payload = m_mtu - RTP_HEADER_SIZE - 2; // FU indicator + FU header
    size_t offset = 1; // Skip original NAL header
    bool first_packet = true;
    
    while (offset < nalu.size()) {
        size_t chunk_size = std::min(max_payload, nalu.size() - offset);
        bool last_packet = (offset + chunk_size) >= nalu.size();
        
        RtpPacket packet(static_cast<int>(m_ssrc), m_sequence++, timestamp, RTP_PAYLOAD_TYPE_H264);
        packet.payload.resize(2 + chunk_size);
        
        // FU Indicator
        packet.payload[0] = fu_indicator;
        
        // FU Header
        u8 fu_header = nal_type;
        if (first_packet) fu_header |= 0x80; // Start bit
        if (last_packet) fu_header |= 0x40;  // End bit
        
        packet.payload[1] = fu_header;
        
        // Copy payload data
        std::memcpy(&packet.payload[2], &nalu[offset], chunk_size);
        
        // Set marker bit on last packet
        if (last_packet) {
            packet.header.marker = 1;
        }
        
        packets.push_back(std::move(packet));
        
        offset += chunk_size;
        first_packet = false;
    }
    
    return packets;
}

std::vector<Bytes> H264Packetizer::extract_nalus(const Bytes& frame) {
    std::vector<Bytes> nalus;
    
    if (frame.size() < 4) {
        return nalus;
    }
    
    size_t i = 0;
    while (i < frame.size()) {
        // Find NAL start code (0x000001 or 0x00000001)
        size_t start_code_size = 0;
        
        if (i + 3 < frame.size() && 
            frame[i] == 0x00 && frame[i + 1] == 0x00 && 
            frame[i + 2] == 0x00 && frame[i + 3] == 0x01) {
            start_code_size = 4;
        } else if (i + 2 < frame.size() && 
                   frame[i] == 0x00 && frame[i + 1] == 0x00 && 
                   frame[i + 2] == 0x01) {
            start_code_size = 3;
        }
        
        if (start_code_size == 0) {
            break; // No more start codes found
        }
        
        size_t nalu_start = i + start_code_size;
        
        // Find next start code
        i = nalu_start;
        while (i < frame.size()) {
            if (i + 3 < frame.size() && 
                frame[i] == 0x00 && frame[i + 1] == 0x00 && 
                frame[i + 2] == 0x00 && frame[i + 3] == 0x01) {
                break;
            } else if (i + 2 < frame.size() && 
                       frame[i] == 0x00 && frame[i + 1] == 0x00 && 
                       frame[i + 2] == 0x01) {
                break;
            }
            i++;
        }
        
        // Extract NALU (without start code)
        size_t nalu_end = (i < frame.size()) ? i : frame.size();
        if (nalu_end > nalu_start) {
            Bytes nalu(frame.begin() + nalu_start, frame.begin() + nalu_end);
            nalus.push_back(std::move(nalu));
        }
    }
    
    return nalus;
}

NalType H264Packetizer::get_nal_type(u8 nal_header) const {
    return static_cast<NalType>(nal_header & 0x1F);
}

bool H264Packetizer::is_small_nalu(const Bytes& nalu) const {
    return nalu.size() <= (m_mtu - RTP_HEADER_SIZE);
}

bool H264Packetizer::is_key_frame(const Bytes& nalu) const {
    if (nalu.empty()) {
        return false;
    }
    
    NalType type = get_nal_type(nalu[0]);
    return type == NalType::IDR || type == NalType::SPS || type == NalType::PPS;
}

} // namespace video_streaming
