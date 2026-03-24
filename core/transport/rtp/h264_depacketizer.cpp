module;

#include <cstring>
#include <vector>

module video_streaming.rtp.h264_depacketizer;
import video_streaming.common.types;
import video_streaming.media.frame; // For NalType
import video_streaming.rtp.packet;

namespace video_streaming {

std::vector<EncodedFrame> H264Depacketizer::process_packet(const RtpPacket& packet) {
    std::vector<EncodedFrame> complete_frames;
    
    if (!packet.is_valid() || packet.payload.empty()) {
        return complete_frames;
    }
    
    update_statistics(packet);
    
    u8 payload_type = packet.payload[0] & 0x1F;
    
    if (payload_type == static_cast<u8>(NalType::FU_A)) {
        // FU-A packet - needs reassembly
        auto frame = process_fu_a_packet(packet);
        if (!frame.data.empty()) {
            complete_frames.push_back(std::move(frame));
        }
    } else {
        // Single NALU packet
        auto frame = process_single_nalu(packet);
        if (!frame.data.empty()) {
            complete_frames.push_back(std::move(frame));
        }
    }
    
    return complete_frames;
}

EncodedFrame H264Depacketizer::process_single_nalu(const RtpPacket& packet) {
    EncodedFrame frame;
    frame.timestamp = packet.header.timestamp;
    
    // Add H.264 start code
    frame.data.push_back(0x00);
    frame.data.push_back(0x00);
    frame.data.push_back(0x01);
    
    // Add NALU payload
    frame.data.insert(frame.data.end(), packet.payload.begin(), packet.payload.end());
    
    m_complete_frames++;
    return frame;
}

EncodedFrame H264Depacketizer::process_fu_a_packet(const RtpPacket& packet) {
    if (packet.payload.size() < 2) {
        return {}; // Invalid FU-A packet
    }
    
    u8 fu_indicator = packet.payload[0];
    u8 fu_header = packet.payload[1];
    
    bool start_bit = (fu_header & 0x80) != 0;
    bool end_bit = (fu_header & 0x40) != 0;
    NalType nal_type = static_cast<NalType>(fu_header & 0x1F);
    
    u16 sequence = packet.header.sequence_number;
    
    // Find or create FU session
    FuState& state = m_fu_sessions[sequence];
    
    if (start_bit) {
        // Start of new FU-A sequence
        state.start_sequence = sequence;
        state.payload.clear();
        state.nal_type = nal_type;
        state.fu_indicator = fu_indicator;
        state.started = true;
        
        // Add FU indicator (reconstructed NAL header)
        u8 nal_header = (fu_indicator & 0xE0) | static_cast<u8>(nal_type);
        state.payload.push_back(nal_header);
    } else if (!state.started) {
        // FU-A packet without start - discard
        return {};
    }
    
    // Add payload data (skip FU indicator and header)
    state.payload.insert(state.payload.end(), 
                      packet.payload.begin() + 2, 
                      packet.payload.end());
    
    if (end_bit) {
        // End of FU-A sequence - assemble complete frame
        if (is_fu_a_complete(state)) {
            auto frame = assemble_fu_a_frame(state);
            m_complete_frames++;
            
            // Clean up session
            m_fu_sessions.erase(sequence);
            return frame;
        }
    }
    
    return {}; // Frame not complete yet
}

bool H264Depacketizer::is_fu_a_complete(const FuState& state) const {
    return state.started && !state.payload.empty();
}

EncodedFrame H264Depacketizer::assemble_fu_a_frame(const FuState& state) {
    EncodedFrame frame;
    // Timestamp could be tracked in FuState or derived
    // For now we don't have it in state, would need to store it
    
    // Add H.264 start code
    frame.data.push_back(0x00);
    frame.data.push_back(0x00);
    frame.data.push_back(0x01);
    
    // Add reassembled payload
    frame.data.insert(frame.data.end(), state.payload.begin(), state.payload.end());
    
    return frame;
}

void H264Depacketizer::update_statistics(const RtpPacket& packet) {
    u16 sequence = packet.header.sequence_number;
    
    if (!is_packet_in_order(sequence)) {
        m_reordered_packets++;
    }
    
    // Check for packet loss
    if (sequence != m_expected_sequence) {
        u16 diff = (sequence > m_expected_sequence) ? 
                   (sequence - m_expected_sequence) : 
                   (0x10000 - m_expected_sequence + sequence);
        
        if (diff > 1) {
            m_lost_packets += diff - 1;
        }
    }
    
    m_expected_sequence = sequence + 1;
}

bool H264Depacketizer::is_packet_in_order(u16 sequence) const {
    return sequence == m_expected_sequence;
}

void H264Depacketizer::reset() {
    m_fu_sessions.clear();
    m_expected_sequence = 0;
    m_lost_packets = 0;
    m_reordered_packets = 0;
    m_complete_frames = 0;
}

} // namespace video_streaming
