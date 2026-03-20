#pragma once

#include "frame.hpp"
#include "../common/types.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class FFmpegH264Encoder {
public:
    FFmpegH264Encoder(int width, int height, int fps, int bitrate = DEFAULT_BITRATE);
    ~FFmpegH264Encoder();
    
    // Non-copyable
    FFmpegH264Encoder(const FFmpegH264Encoder&) = delete;
    FFmpegH264Encoder& operator=(const FFmpegH264Encoder&) = delete;
    
    // Movable
    FFmpegH264Encoder(FFmpegH264Encoder&& other) noexcept;
    FFmpegH264Encoder& operator=(FFmpegH264Encoder&& other) noexcept;
    
    bool is_initialized() const { return m_context != nullptr; }
    
    // Encode frame - returns multiple encoded packets
    std::vector<Frame> encode(const Frame& raw_frame);
    
    // Flush encoder - returns remaining buffered packets
    std::vector<Frame> flush();
    
    // Configuration
    void set_bitrate(int bitrate);
    void set_framerate(int fps);
    void set_preset(const String& preset);
    void set_tune(const String& tune);
    
    // Getters
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    int get_fps() const { return m_fps; }
    int get_bitrate() const { return m_bitrate; }

private:
    bool initialize();
    void cleanup();
    bool setup_frame();
    Frame create_encoded_frame(AVPacket* packet);
    bool convert_frame(const Frame& input);
    
private:
    // Encoder parameters
    int m_width;
    int m_height;
    int m_fps;
    int m_bitrate;
    String m_preset{"veryfast"};
    String m_tune{"zerolatency"};
    
    // FFmpeg structures
    AVCodecContext* m_context{nullptr};
    AVFrame* m_frame{nullptr};
    AVPacket* m_packet{nullptr};
    SwsContext* m_sws_context{nullptr};
    
    // Frame conversion buffer
    Bytes m_conversion_buffer;
    
    // State
    u64 m_frame_count{0};
    bool m_initialized{false};
};
