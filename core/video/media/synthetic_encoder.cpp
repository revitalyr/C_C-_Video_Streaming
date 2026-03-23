#include "synthetic_encoder.hpp"
#include <cstring>

SyntheticH264Encoder::SyntheticH264Encoder(int width, int height, int fps, int bitrate)
    : m_width(width), m_height(height), m_fps(fps), m_bitrate(bitrate) {}

std::vector<Frame> SyntheticH264Encoder::encode(const Frame& raw_frame) {
    std::vector<Frame> encoded_frames;
    
    if (!raw_frame.is_valid()) {
        return encoded_frames;
    }
    
    m_last_timestamp = raw_frame.timestamp;
    
    // Create synthetic NAL units based on frame type
    if (m_frame_count % 30 == 0) { // Every 30 frames = key frame
        // SPS
        encoded_frames.push_back(create_synthetic_nalu(raw_frame, NalType::SPS));
        // PPS  
        encoded_frames.push_back(create_synthetic_nalu(raw_frame, NalType::PPS));
        // IDR
        encoded_frames.push_back(create_synthetic_nalu(raw_frame, NalType::IDR));
    } else {
        // P-frame
        encoded_frames.push_back(create_synthetic_nalu(raw_frame, NalType::Slice));
    }
    
    m_frame_count++;
    return encoded_frames;
}

std::vector<Frame> SyntheticH264Encoder::flush() {
    return {}; // Synthetic encoder doesn't buffer frames
}

Frame SyntheticH264Encoder::create_synthetic_nalu(const Frame& input, NalType type) {
    Frame encoded_frame;
    encoded_frame.timestamp = input.timestamp;
    encoded_frame.width = input.width;
    encoded_frame.height = input.height;
    
    // Create NALU header
    Bytes nalu_header = create_nalu_header(type);
    
    // Create synthetic payload based on type
    Bytes payload;
    
    switch (type) {
        case NalType::SPS:
            payload = {0x00, 0x16, // Profile, level
                      0x4D, 0x40, 0x1E, // Width/height info
                      0x88, 0x90, 0x00}; // Misc
            encoded_frame.type = FrameType::IFrame;
            break;
            
        case NalType::PPS:
            payload = {0x06, 0x80, 0xC0, 0x28}; // PPS data
            encoded_frame.type = FrameType::IFrame;
            break;
            
        case NalType::IDR:
            payload.resize(input.data.size() / 10); // Compressed representation
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = input.data[i * 10] ^ 0x55; // Simple "compression"
            }
            encoded_frame.type = FrameType::IFrame;
            break;
            
        case NalType::Slice:
            payload.resize(input.data.size() / 20); // Higher compression for P-frames
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = input.data[i * 20] ^ 0xAA; // Different pattern
            }
            encoded_frame.type = FrameType::PFrame;
            break;
            
        default:
            payload = {0x00};
            encoded_frame.type = FrameType::Unknown;
            break;
    }
    
    // Add start code + NALU header + payload
    add_start_code(encoded_frame.data);
    encoded_frame.data.insert(encoded_frame.data.end(), nalu_header.begin(), nalu_header.end());
    encoded_frame.data.insert(encoded_frame.data.end(), payload.begin(), payload.end());
    
    return encoded_frame;
}

Bytes SyntheticH264Encoder::create_nalu_header(NalType type) {
    u8 header = static_cast<u8>(type) | 0x80; // Set forbidden bit to 0
    return {header};
}

void SyntheticH264Encoder::add_start_code(Bytes& data) {
    // Add H.264 start code (0x000001)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x01);
}
