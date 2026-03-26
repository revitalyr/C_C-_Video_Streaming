module;

#include <memory>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

export module video_streaming.media.ffmpeg_h264_encoder;

import video_streaming.media.frame;
import video_streaming.common.types;

namespace video_streaming {

export class FFmpegH264Encoder {
private:
    AVCodecContext* m_codec_ctx = nullptr;
    const AVCodec* m_codec = nullptr;
    AVFrame* m_frame = nullptr;
    AVPacket* m_packet = nullptr;
    bool m_initialized = false;
    
public:
    FFmpegH264Encoder();
    ~FFmpegH264Encoder();
    
    bool initialize(u32 width, u32 height, u32 bitrate = 4000000);
    std::vector<u8> encode_frame(const Frame& frame);
    void cleanup();
    
    bool is_initialized() const { return m_initialized; }
    u32 get_width() const;
    u32 get_height() const;
    
private:
    bool setup_codec_context(u32 width, u32 height, u32 bitrate);
    AVFrame* convert_frame_to_avframe(const Frame& frame);
};

} // namespace video_streaming
