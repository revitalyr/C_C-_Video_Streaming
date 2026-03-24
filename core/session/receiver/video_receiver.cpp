module;

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>
#include <optional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavformat/avformat.h>
}

module video_streaming.receiver;
import video_streaming.network.endpoint;
import video_streaming.interfaces;
import video_streaming.common.types;
import video_streaming.rtp.packet;

namespace video_streaming {

VideoReceiver::VideoReceiver(const Config& config)
    : m_config(config)
    , m_logger(std::make_unique<Logger>("video_receiver", LogLevel::INFO))
{
    m_logger->info("Initializing VideoReceiver with config: port={}, jitter_buffer_size={}", 
        config.port, config.jitter_buffer_size);
    
    try {
        // Инициализация компонентов
        m_socket = std::make_unique<UdpSocket>();
        m_depacketizer = std::make_unique<H264Depacketizer>();
        m_jitter_buffer = std::make_unique<JitterBuffer>(config.jitter_buffer_size);
        
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
    if (m_decoded_frame) av_frame_free(&m_decoded_frame);
    if (m_packet_for_decoder) av_packet_free(&m_packet_for_decoder);
    if (m_codec_ctx) avcodec_free_context(&m_codec_ctx);
    if (m_format_ctx) {
        avformat_close_input(&m_format_ctx);
    }
}

bool VideoReceiver::start() {
    if (m_running.load()) {
        m_logger->warn("VideoReceiver is already running");
        return false;
    }
    
    try {
        // Настройка UDP сокета для приема
        Endpoint bind_endpoint(m_config.bind_ip, m_config.port);
        if (!m_socket->open()) {
            m_logger->error("Failed to open UDP socket");
            return false;
        }
        
        if (!m_socket->bind(bind_endpoint)) {
            m_logger->error("Failed to bind UDP socket to {}:{}", 
                m_config.bind_ip, m_config.port);
            return false;
        }
        
        m_socket->set_blocking(false);
        m_socket->set_receive_buffer_size(2 * 1024 * 1024); // 2MB
        
        // Сброс статистики
        reset_stats();
        
        m_start_time = std::chrono::steady_clock::now();
        m_last_stats_update = m_start_time;
        m_stop_requested = false;
        
        // Запуск рабочего потока        
        m_receiver_thread = std::thread(&VideoReceiver::receive_loop, this);
        m_running = true;
        
        m_logger->info("VideoReceiver started successfully on {}:{}", 
            m_config.bind_ip, m_config.port);
        return true;
    } catch (const std::exception& e) {
        m_logger->error("Failed to start VideoReceiver: {}", e.what());
        return false;
    }
}

void VideoReceiver::stop() {
    if (!m_running.load()) {
        return;
    }
    
    m_logger->info("Stopping VideoReceiver...");
    
    m_stop_requested = true;
    
    if (m_receiver_thread.joinable()) {
        m_receiver_thread.join();
    }
    
    m_socket->close();
    
    m_running = false;
    
    // Вывод финальной статистики
    auto final_stats = get_stats();
    m_logger->info("VideoReceiver stopped. Final stats: frames={}, packets={}, loss={:.2f}%, fps={:.2f}", 
        final_stats.frames_received, final_stats.packets_received, 
        final_stats.packet_loss_rate * 100.0, final_stats.fps_actual);
}

void VideoReceiver::set_frame_callback(FrameCallback callback) {
    m_frame_callback = std::move(callback);
}

VideoReceiver::Stats VideoReceiver::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void VideoReceiver::reset_stats() {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    m_stats = Stats{};
}

void VideoReceiver::receive_loop() {
    m_logger->info("VideoReceiver thread started");
    
    Endpoint sender_endpoint;
    
    while (!m_stop_requested) {
        try {
            // Прием RTP пакета
            Bytes received_data;
            auto bytes_received = m_socket->receive_from(
                received_data, m_config.max_frame_size, sender_endpoint);
            
            if (bytes_received > 0) {
                process_rtp_packet(received_data);
            } else if (bytes_received < 0) {
                // Ошибка приема
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Проверка jitter buffer на наличие готовых кадров
            process_jitter_buffer();
            
        } catch (const std::exception& e) {
            m_logger->error("Error in receiver loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    m_logger->info("VideoReceiver thread stopped");
}
 
void VideoReceiver::process_rtp_packet(const std::vector<uint8_t>& packet) {
    // Депакетизация RTP
    RtpPacket rtp_packet;
    // packet is vector, deserialize takes span
    if (!rtp_packet.deserialize(packet)) { 
        return;
    }
    
    // Correct flow: Push RTP packet to Jitter Buffer for reordering
    m_jitter_buffer->push(rtp_packet);
    
    // Depacketization happens in process_jitter_buffer() after ordering
    
    // Обновление статистики
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.packets_received++;
        m_stats.bytes_received += packet.size();
    }
}

void VideoReceiver::process_jitter_buffer() {
    // Получение готовых NAL единиц из jitter buffer
    // Logic refactored to use pop() and depacketizer
    RtpPacket packet;
    
    while (m_jitter_buffer->pop(packet)) {
        auto frames = m_depacketizer->process_packet(packet);
        
        for (const auto& encoded_frame : frames) {
            if (encoded_frame.data.empty()) continue;

            // Prepare AVPacket for decoder
            av_packet_unref(m_packet_for_decoder);
            m_packet_for_decoder->data = const_cast<uint8_t*>(encoded_frame.data.data());
            m_packet_for_decoder->size = static_cast<int>(encoded_frame.data.size());
            m_packet_for_decoder->pts = encoded_frame.timestamp;
            
            // Send to decoder
            int ret = avcodec_send_packet(m_codec_ctx, m_packet_for_decoder);
            if (ret < 0) {
                m_logger->warn("Error sending packet to decoder: {}", ret);
                continue;
            }
            
            // Receive decoded frames
            while (avcodec_receive_frame(m_codec_ctx, m_decoded_frame) == 0) {
                // Create Frame from AVFrame (YUV420P)
                auto frame = std::make_unique<Frame>();
                frame->width = m_decoded_frame->width;
                frame->height = m_decoded_frame->height;
                frame->format = PixelFormat::YUV420P;
                frame->timestamp = m_decoded_frame->pts;
                
                // Copy data from AVFrame to Frame (flat buffer for YUV420P)
                int width = frame->width;
                int height = frame->height;
                size_t y_size = width * height;
                size_t uv_size = y_size / 4;
                frame->data.resize(y_size + 2 * uv_size);
                
                uint8_t* dst_y = frame->data.data();
                uint8_t* dst_u = dst_y + y_size;
                uint8_t* dst_v = dst_u + uv_size;
                
                // Copy Y plane
                av_image_copy_plane(dst_y, width, m_decoded_frame->data[0], m_decoded_frame->linesize[0], width, height);
                
                // Copy U plane
                av_image_copy_plane(dst_u, width/2, m_decoded_frame->data[1], m_decoded_frame->linesize[1], width/2, height/2);
                
                // Copy V plane
                av_image_copy_plane(dst_v, width/2, m_decoded_frame->data[2], m_decoded_frame->linesize[2], width/2, height/2);
                
                // Notify callback
                if (m_frame_callback) {
                    m_frame_callback(*frame);
                }
                
                // Update stats
                {
                    std::lock_guard<std::mutex> lock(m_stats_mutex);
                    m_stats.frames_decoded++;
                    m_stats.frames_received++;
                    
                    // Recalculate FPS
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
                    if (elapsed.count() > 0) {
                        m_stats.fps_actual = (m_stats.frames_decoded * 1000.0) / elapsed.count();
                    }
                }
            }
        }
    }
}

void VideoReceiver::update_stats() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_stats_update);
    
    if (elapsed.count() >= 1) { // Обновляем каждую секунду
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        
        // Получение статистики из компонентов
        auto jitter_stats = m_jitter_buffer->get_stats();
        auto depacketizer_stats = m_depacketizer->get_stats();
        
        // Обновление статистики потерь
        m_stats.packets_lost = depacketizer_stats.packets_lost;
        m_stats.packets_reordered = depacketizer_stats.packets_reordered;
        m_stats.jitter_buffer_delay = jitter_stats.average_delay;
        
        // Расчет процента потерь
        if (m_total_packets_expected > 0) {
            m_stats.packet_loss_rate = static_cast<double>(m_stats.packets_lost) / m_total_packets_expected;
        }
        
        m_last_stats_update = now;
    }
}

} // namespace video_streaming
