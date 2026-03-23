#pragma once

#include "frame.hpp"
#include "common/types.hpp"
#include "rtp/h264_packetizer.hpp"

class SyntheticH264Encoder {
public:
    SyntheticH264Encoder(int width, int height, int fps, int bitrate = DEFAULT_BITRATE);
    ~SyntheticH264Encoder() = default;
    
    // Non-copyable
    SyntheticH264Encoder(const SyntheticH264Encoder&) = delete;
    SyntheticH264Encoder& operator=(const SyntheticH264Encoder&) = delete;
    
    // Movable
    SyntheticH264Encoder(SyntheticH264Encoder&&) = default;
    SyntheticH264Encoder& operator=(SyntheticH264Encoder&&) = default;
    
    bool is_initialized() const { return true; }
    
    // Encode frame - returns multiple encoded packets
    std::vector<Frame> encode(const Frame& raw_frame);
    
    // Flush encoder - returns remaining buffered packets
    std::vector<Frame> flush();
    
    // Configuration
    void set_bitrate(int bitrate) { m_bitrate = bitrate; }
    void set_framerate(int fps) { m_fps = fps; }
    
    // Getters
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    int get_fps() const { return m_fps; }
    int get_bitrate() const { return m_bitrate; }

private:
    Frame create_synthetic_nalu(const Frame& input, NalType type);
    Bytes create_nalu_header(NalType type);
    void add_start_code(Bytes& data);
    
private:
    // Encoder parameters
    int m_width;
    int m_height;
    int m_fps;
    int m_bitrate;
    
    // State
    u64 m_frame_count{0};
    Timestamp m_last_timestamp{0};
};
