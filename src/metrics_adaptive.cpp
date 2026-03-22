#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
using SOCKET = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

class MetricsAdaptiveStreaming {
public:
    struct AdaptiveConfig {
        double min_bitrate = 100000.0;  // 100 kbps
        double max_bitrate = 5000000.0; // 5 Mbps
        double target_bitrate = 1000000.0; // 1 Mbps
        double adjustment_step = 0.1;        // 10% adjustment
        int measurement_window = 5;             // 5 seconds window
        int keyframe_interval = 30;            // Every 30 frames
    };

    struct StreamMetrics {
        uint64_t total_bytes = 0;
        uint64_t total_packets = 0;
        uint64_t lost_packets = 0;
        double current_bitrate = 0.0;
        double average_bitrate = 0.0;
        std::chrono::steady_clock::time_point last_update;
        uint32_t frame_count = 0;
        uint32_t keyframe_count = 0;
    };
    
private:
    StreamMetrics m_metrics;
    AdaptiveConfig m_config;
    std::atomic<bool> m_running{false};
    std::thread m_monitoring_thread;
    std::thread m_adaptive_thread;
    mutable std::mutex m_metrics_mutex;
    std::string m_log_file = "streaming_metrics.log";
    
    void log_info(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::cout << "[" << std::put_time(&tm, "%H:%M:%S") << "] 📊 " << message << "\n";
    }
    
    void log_to_file(const std::string& message) {
        std::ofstream log_file(m_log_file, std::ios::app);
        if (log_file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);
            
            log_file << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] " << message << "\n";
        }
    }
    
    void monitoring_loop() {
        log_info("Metrics monitoring started");
        
        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            std::lock_guard<std::mutex> lock(m_metrics_mutex);
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - m_metrics.last_update).count();
            
            if (elapsed >= m_config.measurement_window) {
                update_bitrate_measurement();
                m_metrics.last_update = now;
            }
            
            // Log current metrics every 10 seconds
            if (elapsed % 10 == 0) {
                log_current_metrics();
            }
        }
    }
    
    void update_bitrate_measurement() {
        // Calculate current bitrate based on recent data
        if (m_metrics.total_packets > 0) {
            m_metrics.current_bitrate = (m_metrics.total_bytes * 8.0) / 
                                     (m_config.measurement_window * 1000.0); // bits per second
            
            // Update average bitrate (exponential moving average)
            double alpha = 0.1; // Smoothing factor
            if (m_metrics.average_bitrate == 0.0) {
                m_metrics.average_bitrate = m_metrics.current_bitrate;
            } else {
                m_metrics.average_bitrate = alpha * m_metrics.current_bitrate + 
                                      (1.0 - alpha) * m_metrics.average_bitrate;
            }
        }
    }
    
    void adaptive_loop() {
        log_info("Adaptive streaming started");
        
        while (m_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(m_config.measurement_window));
            
            std::lock_guard<std::mutex> lock(m_metrics_mutex);
            
            // Analyze network conditions and adjust bitrate
            analyze_network_conditions();
            apply_adaptive_adjustments();
        }
    }
    
    void analyze_network_conditions() {
        double packet_loss_rate = 0.0;
        if (m_metrics.total_packets > 0) {
            packet_loss_rate = (double)m_metrics.lost_packets / m_metrics.total_packets;
        }
        
        log_to_file("Network analysis - Loss rate: " + std::to_string(packet_loss_rate * 100) + 
                   "%, Current bitrate: " + std::to_string(m_metrics.current_bitrate / 1000) + " kbps");
        
        // Determine network quality
        std::string quality;
        if (packet_loss_rate < 0.01) {
            quality = "Excellent";
        } else if (packet_loss_rate < 0.05) {
            quality = "Good";
        } else if (packet_loss_rate < 0.1) {
            quality = "Fair";
        } else {
            quality = "Poor";
        }
        
        log_info("Network quality: " + quality + " (" + std::to_string(packet_loss_rate * 100) + "% loss)");
    }
    
    void apply_adaptive_adjustments() {
        double adjustment = 0.0;
        
        // Adjust based on packet loss
        double packet_loss_rate = 0.0;
        if (m_metrics.total_packets > 0) {
            packet_loss_rate = (double)m_metrics.lost_packets / m_metrics.total_packets;
        }
        
        if (packet_loss_rate > 0.05) {
            // High packet loss - reduce bitrate
            adjustment = -m_config.adjustment_step;
            log_to_file("High packet loss detected - reducing bitrate");
        } else if (packet_loss_rate < 0.01 && m_metrics.current_bitrate < m_config.target_bitrate) {
            // Good conditions - can increase bitrate
            adjustment = m_config.adjustment_step;
            log_to_file("Good network conditions - increasing bitrate");
        }
        
        // Apply adjustment within bounds
        double new_bitrate = m_config.target_bitrate * (1.0 + adjustment);
        new_bitrate = std::max(m_config.min_bitrate, std::min(m_config.max_bitrate, new_bitrate));
        
        if (std::abs(new_bitrate - m_config.target_bitrate) > 1000) {
            m_config.target_bitrate = new_bitrate;
            log_info("Bitrate adjusted to: " + std::to_string(new_bitrate / 1000) + " kbps");
            
            // Here you would send the new bitrate to the encoder
            // send_bitrate_to_encoder(new_bitrate);
        }
    }
    
    void log_current_metrics() {
        std::ostringstream metrics;
        metrics << "Metrics Update:\n";
        metrics << "  Total Bytes: " << m_metrics.total_bytes << "\n";
        metrics << "  Total Packets: " << m_metrics.total_packets << "\n";
        metrics << "  Lost Packets: " << m_metrics.lost_packets << "\n";
        metrics << "  Current Bitrate: " << std::fixed << std::setprecision(1) 
                 << (m_metrics.current_bitrate / 1000) << " kbps\n";
        metrics << "  Average Bitrate: " << std::fixed << std::setprecision(1) 
                 << (m_metrics.average_bitrate / 1000) << " kbps\n";
        metrics << "  Frame Count: " << m_metrics.frame_count << "\n";
        metrics << "  Keyframe Count: " << m_metrics.keyframe_count << "\n";
        
        log_info(metrics.str());
        log_to_file(metrics.str());
    }
    
public:
    explicit MetricsAdaptiveStreaming() : m_config() {}
    explicit MetricsAdaptiveStreaming(const AdaptiveConfig& config) : m_config(config) {}
    
    ~MetricsAdaptiveStreaming() {
        stop();
    }
    
    bool start() {
        log_info("Metrics and Adaptive Streaming system starting");
        
        m_metrics.last_update = std::chrono::steady_clock::now();
        m_running.store(true);
        
        m_monitoring_thread = std::thread(&MetricsAdaptiveStreaming::monitoring_loop, this);
        m_adaptive_thread = std::thread(&MetricsAdaptiveStreaming::adaptive_loop, this);
        
        log_info("📊 Metrics monitoring active");
        log_info("🔄 Adaptive streaming enabled");
        log_info("📝 Logging to: " + m_log_file);
        
        return true;
    }
    
    void stop() {
        m_running.store(false);
        
        if (m_monitoring_thread.joinable()) {
            m_monitoring_thread.join();
        }
        
        if (m_adaptive_thread.joinable()) {
            m_adaptive_thread.join();
        }
        
        log_info("📊 Metrics and Adaptive Streaming stopped");
    }
    
    // Public interface for external components
    void update_packet_stats(uint64_t bytes, uint32_t packets, uint64_t lost = 0) {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_metrics.total_bytes += bytes;
        m_metrics.total_packets += packets;
        m_metrics.lost_packets += lost;
        m_metrics.frame_count++;
        
        // Simulate keyframe detection (every keyframe_interval frames)
        if (m_metrics.frame_count % m_config.keyframe_interval == 0) {
            m_metrics.keyframe_count++;
        }
    }
    
    StreamMetrics get_current_metrics() const {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        return m_metrics;
    }
    
    AdaptiveConfig get_config() const {
        return m_config;
    }
    
    void set_config(const AdaptiveConfig& config) {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        m_config = config;
        log_info("Configuration updated - Target bitrate: " + std::to_string(config.target_bitrate / 1000) + " kbps");
    }
    
    std::string generate_metrics_report() const {
        std::lock_guard<std::mutex> lock(m_metrics_mutex);
        
        std::ostringstream report;
        report << "=== STREAMING METRICS REPORT ===\n";
        report << "Total Bytes Transferred: " << m_metrics.total_bytes << " bytes (" 
                << std::fixed << std::setprecision(2) << (m_metrics.total_bytes / 1024.0 / 1024.0) << " MB)\n";
        report << "Total Packets: " << m_metrics.total_packets << "\n";
        report << "Lost Packets: " << m_metrics.lost_packets << "\n";
        report << "Packet Loss Rate: " << std::fixed << std::setprecision(2);
        
        if (m_metrics.total_packets > 0) {
            report << ((double)m_metrics.lost_packets / m_metrics.total_packets * 100) << "%\n";
        } else {
            report << "0.00%\n";
        }
        
        report << "Current Bitrate: " << std::fixed << std::setprecision(1) 
                << (m_metrics.current_bitrate / 1000) << " kbps\n";
        report << "Average Bitrate: " << std::fixed << std::setprecision(1) 
                << (m_metrics.average_bitrate / 1000) << " kbps\n";
        report << "Total Frames: " << m_metrics.frame_count << "\n";
        report << "Keyframes: " << m_metrics.keyframe_count << "\n";
        report << "Keyframe Interval: " << (m_metrics.frame_count > 0 ? m_metrics.frame_count / m_metrics.keyframe_count : 0) << "\n";
        report << "================================\n";
        
        return report.str();
    }
};

int main(int argc, char* argv[]) {
    std::cout << "📊 Metrics + Adaptive Streaming System\n";
    std::cout << "=====================================\n";
    
    MetricsAdaptiveStreaming::AdaptiveConfig config;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--min-bitrate" && i + 1 < argc) {
            config.min_bitrate = std::stod(argv[++i]) * 1000; // Convert to bps
        } else if (arg == "--max-bitrate" && i + 1 < argc) {
            config.max_bitrate = std::stod(argv[++i]) * 1000;
        } else if (arg == "--target-bitrate" && i + 1 < argc) {
            config.target_bitrate = std::stod(argv[++i]) * 1000;
        } else if (arg == "--window" && i + 1 < argc) {
            config.measurement_window = std::stoi(argv[++i]);
        } else if (arg == "--log-file" && i + 1 < argc) {
            config.measurement_window = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --min-bitrate <kbps>    Minimum bitrate (default: 100)\n";
            std::cout << "  --max-bitrate <kbps>    Maximum bitrate (default: 5000)\n";
            std::cout << "  --target-bitrate <kbps>  Target bitrate (default: 1000)\n";
            std::cout << "  --window <seconds>         Measurement window (default: 5)\n";
            std::cout << "  --log-file <path>        Log file path (default: streaming_metrics.log)\n";
            std::cout << "  --help                    Show this help\n";
            return 0;
        }
    }
    
    std::cout << "🔧 Configuration:\n";
    std::cout << "  Min Bitrate: " << (config.min_bitrate / 1000) << " kbps\n";
    std::cout << "  Max Bitrate: " << (config.max_bitrate / 1000) << " kbps\n";
    std::cout << "  Target Bitrate: " << (config.target_bitrate / 1000) << " kbps\n";
    std::cout << "  Measurement Window: " << config.measurement_window << " seconds\n";
    std::cout << "=====================================\n";
    
    MetricsAdaptiveStreaming metrics_system(config);
    
    if (!metrics_system.start()) {
        std::cerr << "❌ Failed to start metrics system\n";
        return 1;
    }
    
    std::cout << "📊 Metrics system is running. Press Ctrl+C to stop.\n";
    std::cout << "🔄 Adaptive streaming adjustments will be made automatically.\n";
    
    // Simulate some metrics updates for demonstration
    std::thread simulation_thread([&metrics_system]() {
        for (int i = 0; i < 100; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Simulate packet statistics
            uint64_t bytes = 1000 + (rand() % 500);
            uint32_t packets = 10 + (rand() % 5);
            uint64_t lost = (rand() % 100) < 5 ? 1 : 0;
            
            metrics_system.update_packet_stats(bytes, packets, lost);
            
            if (i % 10 == 0) {
                std::cout << "\n" << metrics_system.generate_metrics_report();
            }
        }
    });
    
    simulation_thread.detach();
    
    // Simple signal handling
    std::cin.get();
    
    std::cout << "\n" << metrics_system.generate_metrics_report();
    
    metrics_system.stop();
    return 0;
}
