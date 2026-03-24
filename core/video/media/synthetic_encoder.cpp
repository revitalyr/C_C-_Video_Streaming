module;

#include <cstring>
#include <vector>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

module video_streaming.media.synthetic_encoder;
import video_streaming.media.frame;
import video_streaming.common.types;

namespace video_streaming {

SyntheticH264Encoder::SyntheticH264Encoder(int width, int height, int fps, int bitrate)
{
    init_encoder(width, height, fps, bitrate);
}

SyntheticH264Encoder::~SyntheticH264Encoder() {
    cleanup_encoder();
}

void SyntheticH264Encoder::init_encoder(int width, int height, int fps, int bitrate) {
    m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!m_codec) {
        throw std::runtime_error("H.264 encoder not found");
    }

    m_codec_ctx = avcodec_alloc_context3(m_codec);
    if (!m_codec_ctx) {
        throw std::runtime_error("Could not allocate video codec context");
    }

    m_codec_ctx->bit_rate = bitrate;
    m_codec_ctx->width = width;
    m_codec_ctx->height = height;
    m_codec_ctx->time_base = {1, 1000}; // Time base in milliseconds
    m_codec_ctx->framerate = {fps, 1};
    m_codec_ctx->gop_size = 10;
    m_codec_ctx->max_b_frames = 0; // Low latency
    m_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (m_codec->id == AV_CODEC_ID_H264) {
        av_opt_set(m_codec_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(m_codec_ctx->priv_data, "tune", "zerolatency", 0);
    }

    if (avcodec_open2(m_codec_ctx, m_codec, nullptr) < 0) {
        throw std::runtime_error("Could not open codec");
    }

    m_av_frame = av_frame_alloc();
    m_av_packet = av_packet_alloc();
    if (!m_av_frame || !m_av_packet) {
        throw std::runtime_error("Could not allocate frame or packet");
    }

    m_av_frame->format = m_codec_ctx->pix_fmt;
    m_av_frame->width = m_codec_ctx->width;
    m_av_frame->height = m_codec_ctx->height;

    if (av_frame_get_buffer(m_av_frame, 32) < 0) {
        throw std::runtime_error("Could not allocate the video frame data");
    }
}

void SyntheticH264Encoder::cleanup_encoder() {
    if (m_codec_ctx) avcodec_free_context(&m_codec_ctx);
    if (m_av_frame) av_frame_free(&m_av_frame);
    if (m_av_packet) av_packet_free(&m_av_packet);
}

std::vector<EncodedFrame> SyntheticH264Encoder::encode(const Frame& raw_frame) {
    std::vector<EncodedFrame> encoded_frames;
    
    if (!raw_frame.is_valid()) {
        return encoded_frames;
    }

    // Prepare AVFrame
    // Assuming raw_frame is YUV420P
    if (av_frame_make_writable(m_av_frame) < 0) {
        return encoded_frames;
    }

    // Copy data from Frame to AVFrame
    // Frame data is flat: Y...U...V
    size_t y_size = m_codec_ctx->width * m_codec_ctx->height;
    size_t u_size = y_size / 4;
    
    memcpy(m_av_frame->data[0], raw_frame.data.data(), y_size);
    memcpy(m_av_frame->data[1], raw_frame.data.data() + y_size, u_size);
    memcpy(m_av_frame->data[2], raw_frame.data.data() + y_size + u_size, u_size);

    m_av_frame->pts = raw_frame.timestamp;

    // Send frame to encoder
    if (avcodec_send_frame(m_codec_ctx, m_av_frame) < 0) {
        return encoded_frames;
    }

    // Receive packets
    while (avcodec_receive_packet(m_codec_ctx, m_av_packet) >= 0) {
        EncodedFrame encoded;
        encoded.data.assign(m_av_packet->data, m_av_packet->data + m_av_packet->size);
        encoded.timestamp = m_av_packet->pts;
        encoded.is_keyframe = (m_av_packet->flags & AV_PKT_FLAG_KEY);
        encoded_frames.push_back(std::move(encoded));
        av_packet_unref(m_av_packet);
    }

    return encoded_frames;
}

} // namespace video_streaming
