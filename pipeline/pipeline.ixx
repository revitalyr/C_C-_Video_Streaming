module;

#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>

#include "../common/types.hpp"

// Components headers
#include "../media/frame.hpp"
#include "../media/synthetic_encoder.hpp"
#include "../rtp/h264_packetizer.hpp"
#include "../network/sender.hpp"
#include "../network/receiver.hpp"
#include "../jitter/jitter_buffer.hpp"
#include "../rtp/h264_depacketizer.hpp"

export module video_streaming.pipeline;

import video_streaming.logger;
import video_streaming.interfaces;

namespace video_streaming {

export struct PipelineConfig {
    int width = 640;
    int height = 480;
    int fps = 30;
    int bitrate = 1000000;
    String dest_ip = "127.0.0.1";
    Port dest_port = 5004;
    Port src_port = 5004;
    size_t jitter_size = 50;
    Milliseconds jitter_delay{50};
    bool enable_sender = true;
    bool enable_receiver = true;
};

export struct PipelineMetrics {
    double glass_to_glass_ms = 0.0;
    double jitter_buffer_depth_ms = 0.0;
};

export class Pipeline {
public:
    Pipeline(const PipelineConfig& config) 
        : m_config(config),
          m_encoder(config.width, config.height, config.fps, config.bitrate),
          m_packetizer(12345), // Random SSRC
          m_sender(config.dest_ip, config.dest_port),
          m_receiver(config.src_port),
          m_jitter_buffer(config.jitter_size, config.jitter_delay)
    {}

    ~Pipeline() {
        stop();
    }

    bool start() {
        if (m_running) return false;
        
        auto& logger = LoggerManager::instance();
        logger.get_logger("pipeline")->info("Starting pipeline...");

        if (m_config.enable_sender && !m_sender.start()) {
            logger.get_logger("pipeline")->error("Failed to start sender");
            return false;
        }
        if (m_config.enable_receiver && !m_receiver.start()) {
            logger.get_logger("pipeline")->error("Failed to start receiver");
            return false;
        }

        m_running = true;
        if (m_config.enable_sender) {
            m_capture_thread = std::thread(&Pipeline::capture_loop, this);
        }
        if (m_config.enable_receiver) {
            m_process_thread = std::thread(&Pipeline::process_loop, this);
        }

        return true;
    }

    void stop() {
        if (!m_running) return;
        m_running = false;
        
        if (m_capture_thread.joinable()) m_capture_thread.join();
        if (m_process_thread.joinable()) m_process_thread.join();
        
        m_sender.stop();
        m_receiver.stop();
        LoggerManager::instance().get_logger("pipeline")->info("Pipeline stopped");
    }

    PipelineMetrics get_metrics() const {
        return m_metrics;
    }

private:
    void capture_loop() {
        // Generate and send frames
        auto frame_duration = std::chrono::milliseconds(1000 / m_config.fps);
        Timestamp current_ts = 0;

        while (m_running) {
            auto start = std::chrono::steady_clock::now();

            // 1. Source (Synthetic)
            Frame frame = FrameFactory::create_test_pattern(m_config.width, m_config.height, current_ts);

            // 2. Encode
            auto encoded_frames = m_encoder.encode(frame);

            // 3. Packetize & Send
            for (const auto& enc_frame : encoded_frames) {
                auto packets = m_packetizer.packetize_frame(enc_frame.data, static_cast<u32>(enc_frame.timestamp));
                for (const auto& pkt : packets) {
                    m_sender.send(pkt);
                }
            }

            current_ts += 90000 / m_config.fps; // 90kHz clock
            std::this_thread::sleep_until(start + frame_duration);
        }
    }

    void process_loop() {
        while (m_running) {
            // 1. Receive
            auto packet_opt = m_receiver.receive();
            if (packet_opt) {
                m_jitter_buffer.push(*packet_opt);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            // 2. Pop from Jitter Buffer & Depacketize (Simplified logic for demo)
            RtpPacket pkt;
            if (m_jitter_buffer.pop(pkt)) {
                auto frames = m_depacketizer.process_packet(pkt);
                // In a real pipeline, we would decode 'frames' here
                
                // Simple metrics update
                m_metrics.jitter_buffer_depth_ms = static_cast<double>(m_config.jitter_delay.count());
                // Glass-to-glass latency calculation requires sync clocks or RTT estimation, skipping for now or using dummy
                m_metrics.glass_to_glass_ms = 100.0; // Dummy value for demonstration
            }
        }
    }

    PipelineConfig m_config;
    std::atomic<bool> m_running{false};
    PipelineMetrics m_metrics;
    
    // Pipeline components
    SyntheticH264Encoder m_encoder;
    H264Packetizer m_packetizer;
    Sender m_sender;
    Receiver m_receiver;
    JitterBuffer m_jitter_buffer;
    H264Depacketizer m_depacketizer;

    // Threads
    std::thread m_capture_thread;
    std::thread m_process_thread;
};

} // namespace video_streaming