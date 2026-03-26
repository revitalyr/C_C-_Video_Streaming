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
    : m_width(width), m_height(height), m_fps(fps), m_bitrate(bitrate)
{
    init_encoder(width, height, fps, bitrate);
}

SyntheticH264Encoder::SyntheticH264Encoder(SyntheticH264Encoder&& other) noexcept
    : m_width(other.m_width), 
      m_height(other.m_height), 
      m_fps(other.m_fps), 
      m_bitrate(other.m_bitrate),
      m_codec(other.m_codec), 
      m_codec_ctx(other.m_codec_ctx), 
      m_av_frame(other.m_av_frame), 
      m_av_packet(other.m_av_packet), 
      m_frame_count(other.m_frame_count) {
    
    other.m_codec_ctx = nullptr;
    other.m_av_frame = nullptr;
    other.m_av_packet = nullptr;
    other.m_codec = nullptr;
    other.m_frame_count = 0;
}

SyntheticH264Encoder& SyntheticH264Encoder::operator=(SyntheticH264Encoder&& other) noexcept {
    if (this != &other) {
        cleanup_encoder();
        
        m_width = other.m_width;
        m_height = other.m_height;
        m_fps = other.m_fps;
        m_bitrate = other.m_bitrate;
        m_codec = other.m_codec;
        m_codec_ctx = other.m_codec_ctx;
        m_av_frame = other.m_av_frame;
        m_av_packet = other.m_av_packet;
        m_frame_count = other.m_frame_count;
        
        other.m_codec_ctx = nullptr;
        other.m_av_frame = nullptr;
        other.m_av_packet = nullptr;
        other.m_codec = nullptr;
        other.m_frame_count = 0;
    }
    return *this;
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
        cleanup_encoder();
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
    if (m_codec_ctx) { avcodec_free_context(&m_codec_ctx); m_codec_ctx = nullptr; }
    if (m_av_frame) { av_frame_free(&m_av_frame); m_av_frame = nullptr; }
    if (m_av_packet) { av_packet_free(&m_av_packet); m_av_packet = nullptr; }
    m_codec = nullptr;
}

std::vector<EncodedFrame> SyntheticH264Encoder::encode(const Frame& raw_frame) {
    std::vector<EncodedFrame> encoded_frames;
    
    // Ensure the encoder is fully initialized
    if (!m_codec_ctx || !m_av_frame || !m_av_packet || !m_codec || !raw_frame.is_valid()) {
        return encoded_frames;
    }
    
    if (raw_frame.width != m_width || raw_frame.height != m_height) {
        return encoded_frames;
    }

    // Verify buffer integrity before FFmpeg processing
    if (raw_frame.data.empty() || raw_frame.data.size() < raw_frame.get_frame_size()) {
        return encoded_frames;
    }

    if (av_frame_make_writable(m_av_frame) < 0) {
        return encoded_frames;
    }

    // Copy data plane by plane respecting strides (linesize)
    const uint8_t* src = raw_frame.data.data();
    
    // Y plane - Use raw_frame.stride for robustness
    for (int i = 0; i < m_height; ++i) {
        memcpy(m_av_frame->data[0] + i * m_av_frame->linesize[0], src + i * raw_frame.stride, m_width);
    }
    src += raw_frame.stride * m_height;

    // U and V planes (subsampled)
    int uv_height = m_height / 2;
    int uv_width = m_width / 2;
    int uv_stride = raw_frame.stride / 2;

    for (int i = 0; i < uv_height; ++i) {
        memcpy(m_av_frame->data[1] + i * m_av_frame->linesize[1], src + i * uv_stride, uv_width);
    }
    src += uv_stride * uv_height;

    for (int i = 0; i < uv_height; ++i) {
        memcpy(m_av_frame->data[2] + i * m_av_frame->linesize[2], src + i * uv_stride, uv_width);
    }

    m_av_frame->pts = raw_frame.timestamp;

    // Send frame to encoder
    int ret = avcodec_send_frame(m_codec_ctx, m_av_frame);
    if (ret < 0) {
        return encoded_frames;
    }

    // Receive packets - Handle FFmpeg return codes correctly
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codec_ctx, m_av_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) break;

        EncodedFrame encoded;
        encoded.data.assign(m_av_packet->data, m_av_packet->data + m_av_packet->size);
        encoded.timestamp = m_av_packet->pts;
        encoded.is_keyframe = (m_av_packet->flags & AV_PKT_FLAG_KEY);
        encoded_frames.push_back(std::move(encoded));
        av_packet_unref(m_av_packet);
    }

    return encoded_frames;
}

void SyntheticH264Encoder::set_bitrate(int bitrate) {
    m_bitrate = bitrate;
    if (m_codec_ctx) m_codec_ctx->bit_rate = bitrate;
}

void SyntheticH264Encoder::set_framerate(int fps) {
    m_fps = fps;
    if (m_codec_ctx) m_codec_ctx->framerate = {fps, 1};
}

std::vector<EncodedFrame> SyntheticH264Encoder::flush() {
    std::vector<EncodedFrame> encoded_frames;
    if (!m_codec_ctx || !m_av_packet || !m_codec) return encoded_frames;

    // Enter draining mode by sending NULL frame to the encoder
    int ret = avcodec_send_frame(m_codec_ctx, nullptr);
    if (ret < 0) return encoded_frames;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codec_ctx, m_av_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) break;

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
