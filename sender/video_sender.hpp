#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "../common/logger.hpp"
#include "../media/frame.hpp"
#include "../media/synthetic_encoder.hpp"
#include "../network/udp_socket.hpp"
#include "../rtp/h264_packetizer.hpp"

namespace video_streaming {

class VideoSender {
public:
    struct Config {
        uint16_t port = 5000;
        std::string destination_ip = "127.0.0.1";
        int fps = 30;
        int width = 1920;
        int height = 1080;
        int bitrate = 4000000; // 4 Mbps
    };

    explicit VideoSender(const Config& config);
    ~VideoSender();

    // Основные методы управления
    bool start();
    void stop();
    bool is_running() const noexcept { return m_running.load(); }

    // Статистика
    struct Stats {
        uint64_t frames_sent = 0;
        uint64_t packets_sent = 0;
        uint64_t bytes_sent = 0;
        double fps_actual = 0.0;
        std::chrono::milliseconds encoding_time{0};
        std::chrono::milliseconds network_time{0};
    };
    
    Stats get_stats() const;

private:
    // Основной рабочий цикл
    void sender_loop();
    
    // Генерация видео кадров
    std::unique_ptr<VideoFrame> generate_frame();
    
    // Отправка кадра
    bool send_frame(const VideoFrame& frame);

private:
    Config m_config;
    std::unique_ptr<Logger> m_logger;
    
    // Компоненты
    std::unique_ptr<SyntheticH264Encoder> m_encoder;
    std::unique_ptr<UdpSocket> m_socket;
    std::unique_ptr<H264Packetizer> m_packetizer;
    
    // Управление потоками
    std::thread m_sender_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    
    // Очередь кадров
    std::queue<std::unique_ptr<VideoFrame>> m_frame_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Статистика
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    
    // Тайминги
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_frame_time;
};

} // namespace video_streaming
