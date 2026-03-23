#include "video_receiver.hpp"
#include <chrono>

namespace video_streaming {

VideoReceiver::VideoReceiver(const Config& config)
    : m_config(config),
      m_logger(std::make_unique<Logger>("video_receiver", LogLevel::INFO))
{
    m_logger->info("Initializing VideoReceiver on port {}", config.port);

    try {
        m_receiver = std::make_unique<Receiver>(m_config.port);
        m_jitter_buffer = std::make_unique<JitterBuffer>(m_config.jitter_buffer_size, m_config.jitter_buffer_delay);
        m_depacketizer = std::make_unique<H264Depacketizer>();

        // Init FFmpeg decoder
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!codec) throw std::runtime_error("H.264 decoder not found");

        m_codec_ctx = avcodec_alloc_context3(codec);
        if (!m_codec_ctx) throw std::runtime_error("Could not create codec context");

        if (avcodec_open2(m_codec_ctx, codec, nullptr) < 0) {
            throw std::runtime_error("Could not open codec");
        }

        m_decoded_frame = av_frame_alloc();
        m_packet_for_decoder = av_packet_alloc();
        if (!m_decoded_frame || !m_packet_for_decoder) {
            throw std::runtime_error("Could not allocate frame or packet");
        }

        m_logger->info("VideoReceiver initialized successfully");
    } catch (const std::exception& e) {
        m_logger->error("Failed to initialize VideoReceiver: {}", e.what());
        if (m_decoded_frame) av_frame_free(&m_decoded_frame);
        if (m_packet_for_decoder) av_packet_free(&m_packet_for_decoder);
        if (m_codec_ctx) avcodec_free_context(&m_codec_ctx);
        throw;
    }
}

VideoReceiver::~VideoReceiver() {
    stop();
    av_frame_free(&m_decoded_frame);
    av_packet_free(&m_packet_for_decoder);
    avcodec_free_context(&m_codec_ctx);
}

bool VideoReceiver::start() {
    if (m_running.load()) {
        m_logger->warn("VideoReceiver is already running");
        return false;
    }

    if (!m_receiver->start()) {
        m_logger->error("Failed to start network receiver");
        return false;
    }

    m_running = true;
    m_receiver_thread = std::thread(&VideoReceiver::receive_loop, this);
    m_logger->info("VideoReceiver started");
    return true;
}

void VideoReceiver::stop() {
    if (!m_running.load()) return;

    m_logger->info("Stopping VideoReceiver...");
    m_running = false;
    m_receiver->stop();
    if (m_receiver_thread.joinable()) {
        m_receiver_thread.join();
    }
    m_logger->info("VideoReceiver stopped");
}

void VideoReceiver::set_frame_callback(FrameCallback callback) {
    m_frame_callback = std::move(callback);
}

void VideoReceiver::receive_loop() {
    while (m_running.load()) {
        auto rtp_packet_opt = m_receiver->receive();
        if (!rtp_packet_opt) continue;
        
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_stats.packets_received++;
            m_stats.bytes_received += rtp_packet_opt->size();
        }

        m_jitter_buffer->push(*rtp_packet_opt);

        RtpPacket ready_packet;
        while (m_jitter_buffer->pop(ready_packet)) {
            auto encoded_frames = m_depacketizer->process_packet(ready_packet);

            for (const auto& encoded_frame : encoded_frames) {
                av_packet_unref(m_packet_for_decoder);
                m_packet_for_decoder->data = const_cast<uint8_t*>(encoded_frame.data.data());
                m_packet_for_decoder->size = encoded_frame.data.size();
                m_packet_for_decoder->pts = encoded_frame.timestamp;

                auto decode_start = std::chrono::steady_clock::now();

                if (avcodec_send_packet(m_codec_ctx, m_packet_for_decoder) < 0) continue;

                while (avcodec_receive_frame(m_codec_ctx, m_decoded_frame) == 0) {
                    auto decode_end = std::chrono::steady_clock::now();
                    {
                        std::lock_guard<std::mutex> lock(m_stats_mutex);
                        m_stats.frames_decoded++;
                        m_stats.decoder_latency_ms = std::chrono::duration<double, std::milli>(decode_end - decode_start).count();
                    }

                    if (m_frame_callback) {
                        Frame frame;
                        frame.width = m_decoded_frame->width;
                        frame.height = m_decoded_frame->height;
                        frame.timestamp = m_decoded_frame->pts;
                        m_frame_callback(frame);
                    }
                }
            }
        }
    }
}

VideoReceiver::Stats VideoReceiver::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

} // namespace video_streaming