module;

#include <vector>
#include <memory>
#include <vector>

extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVCodec;
}

export module video_streaming.media.synthetic_encoder;
import video_streaming.media.frame;
import video_streaming.common.types;

namespace video_streaming {

export class SyntheticH264Encoder {
public:
    SyntheticH264Encoder(int width, int height, int fps, int bitrate = 4000000);
    ~SyntheticH264Encoder();
    
    // Non-copyable
    SyntheticH264Encoder(const SyntheticH264Encoder&) = delete;
    SyntheticH264Encoder& operator=(const SyntheticH264Encoder&) = delete;
    
    // Movable
    SyntheticH264Encoder(SyntheticH264Encoder&& other) noexcept;
    SyntheticH264Encoder& operator=(SyntheticH264Encoder&& other) noexcept;
    
    bool is_initialized() const { return true; }

    std::vector<EncodedFrame> encode(const Frame& frame);
    std::vector<EncodedFrame> flush();
    
    // Configuration
    void set_bitrate(int bitrate);
    void set_framerate(int fps);
    
    // Getters
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }
    int get_fps() const { return m_fps; }
    int get_bitrate() const { return m_bitrate; }

private:
    void init_encoder(int width, int height, int fps, int bitrate);
    void cleanup_encoder();
    
    int m_width{0};
    int m_height{0};
    int m_fps{0};
    int m_bitrate{0};
    AVCodecContext* m_codec_ctx = nullptr;
    AVFrame* m_av_frame = nullptr;
    AVPacket* m_av_packet = nullptr;
    const AVCodec* m_codec = nullptr;
    u64 m_frame_count{0};
};

} // namespace video_streaming