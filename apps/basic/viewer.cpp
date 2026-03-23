#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <mutex>
#include <iomanip>
#include "media/frame.hpp"

import video_streaming.receiver;

using namespace std::chrono_literals;
using namespace video_streaming;

std::atomic<bool> g_shutdown{false};
void signal_handler(int signal) {
    std::cout << "\n🛑 Signal " << signal << " received, stopping viewer...\n";
    g_shutdown.store(true);
}

void print_metrics(VideoReceiver& viewer, std::chrono::steady_clock::duration elapsed) {
    static uint64_t last_bytes = 0;
    
    auto stats = viewer.get_stats();
    
    double seconds_elapsed = std::chrono::duration<double>(elapsed).count();
    double mbps = (stats.bytes_received - last_bytes) * 8.0 / (2.0 * 1024 * 1024);
    // FPS is already calculated in stats by the receiver
    double fps = stats.fps_actual;

    std::cout << "📹 Pkts Rcvd: " << stats.packets_received
              << " | 🎬 Frames Decoded: " << stats.frames_received
              << " | 🎬 FPS: " << std::fixed << std::setprecision(1) << fps
              << " | 📊 Bitrate: " << std::setprecision(2) << mbps << " Mbps"
              << " | 📉 Loss: " << std::setprecision(2) << stats.packet_loss_rate * 100.0 << "%"
              << " | ⏱️ Latency: " << std::setprecision(1) << stats.decoder_latency_ms << "ms\n";

    last_bytes = stats.bytes_received;
}

int main(int argc, char* argv[]) {
    VideoReceiver::Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Video Viewer - Core-based H.264 receiver\n";
            std::cout << "Usage: " << argv[0] << " [--port <port>]\n";
            return 0;
        }
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "🎬 Starting Core Video Viewer...\n";
    std::cout << "📡 Listening on port " << config.port << "\n";

    try {
        VideoReceiver viewer(config);
        // We don't need pixel data for the basic viewer, so we set an empty callback
        // or just ignore the data to keep the decoder pipeline running.
        // The receiver requires the pipeline to run to update stats.
        viewer.set_frame_callback([](const Frame&){});

        if (!viewer.start()) {
            std::cerr << "❌ Failed to start VideoReceiver\n";
            return 1;
        }

        auto start_time = std::chrono::steady_clock::now();
        auto last_metrics_time = start_time;

        while (!g_shutdown.load()) {
            // Just wait and print metrics; stats are updated in background thread
            if (std::chrono::steady_clock::now() - last_metrics_time >= 2s) {
                print_metrics(viewer, std::chrono::steady_clock::now() - start_time);
                last_metrics_time = std::chrono::steady_clock::now();
            }
            std::this_thread::sleep_for(100ms);
        }

        viewer.stop();
        std::cout << "🎬 Viewer stopped.\n";

    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
