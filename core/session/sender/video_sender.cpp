module;

#include <chrono>
#include <algorithm>
#include <thread>
#include <random>
#include <memory>

module video_streaming.sender;
import video_streaming.network.endpoint;
import video_streaming.media.frame;
import video_streaming.common.types;
import video_streaming.rtp.packet; // Explicitly required for RtpPacket usage

namespace video_streaming {

VideoSender::VideoSender(const Config& config)
    : m_config(config)
    , m_logger(std::make_unique<Logger>("video_sender", LogLevel::INFO))
    , m_rng(std::random_device{}())
{
    m_logger->info("Initializing VideoSender with config: port={}, fps={}, {}x{}", 
        config.port, config.fps, config.width, config.height);
    
    try {
        // Инициализация компонентов
        m_encoder = std::make_unique<SyntheticH264Encoder>(
            config.width, config.height, config.fps, config.bitrate);
        
        m_socket = std::make_unique<UdpSocket>();
        m_packetizer = std::make_unique<H264Packetizer>(0x12345678);
        
        m_logger->info("VideoSender initialized successfully");
    } catch (const std::exception& e) {
        m_logger->error("Failed to initialize VideoSender: {}", e.what());
        throw;
    }
}

VideoSender::~VideoSender() {
    stop();
}

bool VideoSender::start() {
    if (m_running.load()) {
        m_logger->warn("VideoSender is already running");
        return false;
    }
    
    try {
        // Настройка UDP сокета
        Endpoint destination(m_config.destination_ip, m_config.port);
        if (!m_socket->open()) {
            m_logger->error("Failed to open UDP socket");
            return false;
        }
        
        m_socket->set_blocking(false);
        m_socket->set_send_buffer_size(1024 * 1024); // 1MB
        
        // Сброс статистики
        {
            std::lock_guard<std::mutex> lock(m_stats_mutex);
            m_stats = Stats{};
        }
        
        m_start_time = std::chrono::steady_clock::now();
        m_last_frame_time = m_start_time;
        m_stop_requested = false;
        
        // Запуск рабочего потока
        m_sender_thread = std::thread(&VideoSender::sender_loop, this);
        m_running = true;
        
        m_logger->info("VideoSender started successfully");
        return true;
    } catch (const std::exception& e) {
        m_logger->error("Failed to start VideoSender: {}", e.what());
        return false;
    }
}

void VideoSender::stop() {
    if (!m_running.load()) {
        return;
    }
    
    m_logger->info("Stopping VideoSender...");
    
    m_stop_requested = true;
    m_queue_cv.notify_all();
    
    if (m_sender_thread.joinable()) {
        m_sender_thread.join();
    }
    
    m_socket->close();
    m_running = false;
    
    // Вывод финальной статистики
    auto final_stats = get_stats();
    m_logger->info("VideoSender stopped. Final stats: frames={}, packets={}, bytes={}, fps={:.2f}", 
        final_stats.frames_sent, final_stats.packets_sent, 
        final_stats.bytes_sent, final_stats.fps_actual);
}

VideoSender::Stats VideoSender::get_stats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
}

void VideoSender::sender_loop() {
    m_logger->info("VideoSender thread started");
    
    const auto frame_interval = std::chrono::milliseconds(1000 / m_config.fps);
    
    while (!m_stop_requested) {
        auto frame_start = std::chrono::steady_clock::now();
        
        try {
            // Генерация кадра
            auto frame = generate_frame();
            if (!frame) {
                m_logger->error("Failed to generate frame");
                continue;
            }
            
            // Отправка кадра
            if (send_frame(*frame)) {
                // Обновление статистики
                std::lock_guard<std::mutex> lock(m_stats_mutex);
                m_stats.frames_sent++;
                
                // Расчет фактического FPS
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
                if (elapsed.count() > 0) {
                    m_stats.fps_actual = (m_stats.frames_sent * 1000.0) / elapsed.count();
                }
            }
            
        } catch (const std::exception& e) {
            m_logger->error("Error in sender loop: {}", e.what());
        }
        
        // Контроль частоты кадров
        auto frame_end = std::chrono::steady_clock::now();
        auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
        
        if (frame_time < frame_interval) {
            std::this_thread::sleep_for(frame_interval - frame_time);
        }
    }
    
    m_logger->info("VideoSender thread stopped");
}

std::unique_ptr<Frame> VideoSender::generate_frame() {
    auto frame = std::make_unique<video_streaming::Frame>();
    
    // Заполнение метаданных кадра
    frame->width = m_config.width;
    frame->height = m_config.height;
    frame->format = PixelFormat::YUV420P;
    frame->timestamp = static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start_time).count());
    
    // Генерация АНИМИРОВАННОГО градиентного контента
    const size_t y_size = frame->width * frame->height;
    const size_t uv_size = y_size / 4; // Правильно для YUV420P
    const size_t total_size = y_size + uv_size * 2; // Y + U + V planes
    
    frame->data.resize(total_size);
    
    // Анимация градиента на основе времени
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time).count();
    float phase = (elapsed % 4000) / 4000.0f * 2.0f * M_PI;
    
    // Создание анимированного градиента
    uint8_t* y_plane = frame->data.data();
    uint8_t* u_plane = y_plane + y_size;
    uint8_t* v_plane = u_plane + uv_size;
    
    for (int y = 0; y < frame->height; ++y) {
        for (int x = 0; x < frame->width; ++x) {
            // Анимированный градиент с цветовыми переходами
            float gradient_factor = (float)x / frame->width;
            float time_factor = (sin(phase + y * 0.01f) + 1.0f) * 0.5f;
            
            // Y компонент: анимированный градиент
            uint8_t r = static_cast<uint8_t>((sin(phase) * 0.5f + 0.5f) * 100 + 100);
            uint8_t g = static_cast<uint8_t>((sin(phase + 2.0f) * 0.5f + 0.5f) * 100 + 100);
            uint8_t b = static_cast<uint8_t>((sin(phase + 4.0f) * 0.5f + 0.5f) * 100 + 100);
            
            // Конвертация RGB в Y для градиента
            y_plane[y * frame->width + x] = static_cast<uint8_t>(
                0.299f * r + 0.587f * g + 0.114f * b
            );
        }
    }
    
    // UV компоненты: цветовая информация для градиента
    for (int i = 0; i < frame->height; ++i) {
        for (int j = 0; j < frame->width / 2; ++j) {
            int idx = i * frame->width / 2 + j;
            float gradient_factor = (float)j / (frame->width / 2);
            
            // Анимированные UV компоненты
            u_plane[idx] = static_cast<uint8_t>((sin(phase + 2.0f) * 0.5f + 0.5f) * 50 + 128);
            v_plane[idx] = static_cast<uint8_t>((sin(phase + 4.0f) * 0.5f + 0.5f) * 50 + 128);
        }
    }
    
    return frame;
}

bool VideoSender::send_frame(const Frame& frame) {
    auto encode_start = std::chrono::steady_clock::now();
    
    // Кодирование кадра в H.264
    auto encoded_data = m_encoder->encode(frame);
    if (encoded_data.empty()) {
        m_logger->error("Failed to encode frame");
        return false;
    }
    
    auto encode_end = std::chrono::steady_clock::now();
    auto encode_time = std::chrono::duration_cast<std::chrono::milliseconds>(encode_end - encode_start);
    
    auto packetize_start = std::chrono::steady_clock::now();
    
    // Пакетизация в RTP
    std::vector<RtpPacket> all_packets;
    for (const auto& enc_frame : encoded_data) {
        auto packets = m_packetizer->packetize_frame(enc_frame.data, static_cast<u32>(enc_frame.timestamp));
        all_packets.insert(all_packets.end(), packets.begin(), packets.end());
    }
    
    if (all_packets.empty()) {
        return false; // Warning already logged if encoder failed, but empty packetization is odd
    }
    
    auto network_start = std::chrono::steady_clock::now();

    // Отправка RTP пакетов
    Endpoint destination(m_config.destination_ip, m_config.port);
    size_t packets_sent = 0;
    size_t bytes_sent = 0;
    std::uniform_real_distribution<double> loss_dist(0.0, 100.0);
    
    for (const auto& packet : all_packets) {
        // Network Simulation: Packet Loss
        if (m_config.packet_loss > 0.0 && loss_dist(m_rng) < m_config.packet_loss) {
            continue;
        }

        // Network Simulation: Delay & Jitter
        if (m_config.delay_ms > 0 || m_config.jitter_ms > 0) {
            std::uniform_int_distribution<int> jitter_dist(-m_config.jitter_ms, m_config.jitter_ms);
            int total_delay = std::max(0, m_config.delay_ms + jitter_dist(m_rng));
            if (total_delay > 0) std::this_thread::sleep_for(std::chrono::milliseconds(total_delay));
        }

        // Send full RTP packet (header + payload)
        auto serialized_packet = packet.serialize();
        if (m_socket->send(serialized_packet, destination)) {
            packets_sent++;
            bytes_sent += serialized_packet.size();
        } else {
            m_logger->warn("Failed to send RTP packet");
        }
    }
    
    auto network_end = std::chrono::steady_clock::now();
    auto network_time = std::chrono::duration_cast<std::chrono::milliseconds>(network_end - network_start);
    
    // Обновление статистики
    {
        std::lock_guard<std::mutex> lock(m_stats_mutex);
        m_stats.packets_sent += packets_sent;
        m_stats.bytes_sent += bytes_sent;
        m_stats.encoding_time = encode_time;
        m_stats.network_time = network_time;
    }
    
    m_logger->debug("Sent frame: {} packets, {} bytes, encode: {}ms, network: {}ms", 
        packets_sent, bytes_sent, encode_time.count(), network_time.count());
    
    return packets_sent > 0;
}

} // namespace video_streaming
