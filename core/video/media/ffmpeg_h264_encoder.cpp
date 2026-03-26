module;

#include <memory>
#include <vector>
#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

module video_streaming.media.ffmpeg_h264_encoder;

import video_streaming.media.frame;
import video_streaming.common.types;

namespace video_streaming {

FFmpegH264Encoder::FFmpegH264Encoder() {
    // Initialize FFmpeg
    m_codec = nullptr;
    m_codec_ctx = nullptr;
    m_frame = nullptr;
    m_packet = nullptr;
    m_initialized = false;
}

FFmpegH264Encoder::~FFmpegH264Encoder() {
    cleanup();
}

bool FFmpegH264Encoder::initialize(u32 width, u32 height, u32 bitrate) {
    return setup_codec_context(width, height, bitrate);
}

std::vector<u8> FFmpegH264Encoder::encode_frame(const Frame& frame) {
    if (!m_initialized || !m_frame || !m_packet) {
        return {};
    }
    
    // Convert frame to AVFrame
    AVFrame* av_frame = convert_frame_to_avframe(frame);
    if (!av_frame) {
        return {};
    }
    
    // Send frame to encoder
    int ret = avcodec_send_frame(m_codec_ctx, av_frame);
    if (ret < 0) {
        av_frame_free(&av_frame);
        return {};
    }
    
    // Receive packet from encoder
    ret = avcodec_receive_packet(m_codec_ctx, m_packet);
    av_frame_free(&av_frame);
    
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return {};
    }
    
    if (ret < 0) {
        return {};
    }
    
    // Copy packet data to vector
    std::vector<u8> result(m_packet->data, m_packet->data + m_packet->size);
    av_packet_unref(m_packet);
    
    return result;
}

void FFmpegH264Encoder::cleanup() {
    if (m_codec_ctx) {
        avcodec_free_context(&m_codec_ctx);
        m_codec_ctx = nullptr;
    }
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    m_initialized = false;
}

u32 FFmpegH264Encoder::get_width() const {
    return m_initialized ? m_codec_ctx->width : 0;
}

u32 FFmpegH264Encoder::get_height() const {
    return m_initialized ? m_codec_ctx->height : 0;
}

bool FFmpegH264Encoder::setup_codec_context(u32 width, u32 height, u32 bitrate) {
    m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!m_codec) {
        return false;
    }
    
    m_codec_ctx = avcodec_alloc_context3(m_codec);
    if (!m_codec_ctx) {
        return false;
    }
    
    m_codec_ctx->width = width;
    m_codec_ctx->height = height;
    m_codec_ctx->bit_rate = bitrate;
    m_codec_ctx->time_base = {1, 30};
    m_codec_ctx->framerate = {30, 1};
    m_codec_ctx->gop_size = 30;
    m_codec_ctx->max_b_frames = 0;
    m_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    
    if (avcodec_open2(m_codec_ctx, m_codec, nullptr) < 0) {
        cleanup();
        return false;
    }
    
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    
    if (!m_frame || !m_packet) {
        cleanup();
        return false;
    }
    
    m_initialized = true;
    return true;
}

AVFrame* FFmpegH264Encoder::convert_frame_to_avframe(const Frame& frame) {
    if (!m_frame) {
        return nullptr;
    }
    
    m_frame->width = frame.width;
    m_frame->height = frame.height;
    m_frame->format = AV_PIX_FMT_YUV420P;
    
    if (av_frame_get_buffer(m_frame, 0) < 0) {
        return nullptr;
    }
    
    // Convert RGB24 to YUV420 (simplified conversion)
    // In a real implementation, you'd use sws_scale for proper conversion
    for (u32 y = 0; y < frame.height; ++y) {
        for (u32 x = 0; x < frame.width; ++x) {
            u32 src_idx = (y * frame.width + x) * 3;
            u8 r = frame.data[src_idx];
            u8 g = frame.data[src_idx + 1];
            u8 b = frame.data[src_idx + 2];
            
            // Simple RGB to YUV conversion
            u8 y_val = (0.299 * r) + (0.587 * g) + (0.114 * b);
            u8 u_val = (-0.147 * r) + (-0.289 * g) + (0.436 * b) + 128;
            u8 v_val = (0.615 * r) + (-0.515 * g) + (-0.100 * b) + 128;
            
            u32 dst_idx = y * frame.width + x;
            m_frame->data[0][dst_idx] = y_val;
        }
    }
    
    return m_frame;
}

} // namespace video_streaming
