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
    SyntheticH264Encoder(int width, int height, int fps, int bitrate);
    ~SyntheticH264Encoder();

    std::vector<EncodedFrame> encode(const Frame& frame);

private:
    void init_encoder(int width, int height, int fps, int bitrate);
    void cleanup_encoder();
    
    AVCodecContext* m_codec_ctx = nullptr;
    AVFrame* m_av_frame = nullptr;
    AVPacket* m_av_packet = nullptr;
    const AVCodec* m_codec = nullptr;
    u64 m_pts = 0;
};

} // namespace video_streaming