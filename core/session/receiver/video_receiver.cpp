module;

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
#include <iostream>

#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavformat/avformat.h>

module video_streaming.receiver;

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
    
    std::vector<uint8_t> packet_buffer(m_config.max_frame_size);
    Endpoint sender_endpoint;
    
    while (!m_stop_requested) {
        try {
            // Прием RTP пакета
            Bytes received_bytes(packet_buffer.data(), packet_buffer.size());
            auto bytes_received = m_socket->receive_from(
                received_bytes, packet_buffer.size(), sender_endpoint);
            
            if (bytes_received > 0) {
                std::vector<uint8_t> packet(packet_buffer.begin(), packet_buffer.begin() + bytes_received);
                process_rtp_packet(packet);
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
    auto nal_units = m_depacketizer->process_packet(packet);
    if (nal_units.empty()) {
        return;
    }
    
    // Добавление NAL единиц в jitter buffer
    for (const auto& nal_unit : nal_units) {
        m_jitter_buffer->add_packet(nal_unit);
    }
    
    // Обновление статистики
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.packets_received += nal_units.size();
        m_stats.bytes_received += packet.size();
    }
}

void VideoReceiver::process_jitter_buffer() {
    // Получение готовых NAL единиц из jitter buffer
    auto ready_packets = m_jitter_buffer->get_ready_packets();
    if (ready_packets.empty()) {
        return;
    }
    
    // Сборка кадра из NAL единиц
    auto frame = assemble_frame(ready_packets);
    if (frame) {
        // Добавление кадра в очередь вывода
        {
            std::lock_guard<std::mutex> lock(m_frame_queue_mutex);
            m_frame_queue.push(std::move(frame));
            
            // Ограничение размера очереди
            while (m_frame_queue.size() > 10) {
                m_frame_queue.pop();
            }
        }
        
        m_frame_queue_cv.notify_one();
        
        // Обновление статистики
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.frames_received++;
        
        // Расчет фактического FPS
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
        if (elapsed.count() > 0) {
            m_stats.fps_actual = (m_stats.frames_received * 1000.0) / elapsed.count();
        }
    }
}

std::unique_ptr<Frame> VideoReceiver::assemble_frame(
    const std::vector<std::vector<uint8_t>>& nal_units) {
    
    if (nal_units.empty()) {
        return nullptr;
    }
    
    auto frame = std::make_unique<Frame>();
    
    // Определение размера кадра
    size_t total_size = 0;
    for (const auto& nal_unit : nal_units) {
        total_size += nal_unit.size();
    }
    
    frame->data.resize(total_size);
    // frame->timestamp will be set by decoder
    
    // Копирование NAL единиц в буфер кадра
    size_t offset = 0;
    for (const auto& nal_unit : nal_units) {
        std::copy(nal_unit.begin(), nal_unit.end(), frame->data.begin() + offset);
        offset += nal_unit.size();
    }
    
    return frame;
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
