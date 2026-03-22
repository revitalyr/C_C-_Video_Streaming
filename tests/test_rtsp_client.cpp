#include "../rtp/rtsp_client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace video_streaming;

void test_real_wowza_stream() {
    std::cout << "\n=== Testing Real Wowza RTSP Stream ===\n";
    
    RTSPClient::Config config;
    config.rtsp_url = "rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2";
    config.output_file = "real_wowza_stream.rtp";
    config.max_packets = 5000;
    config.enable_logging = true;
    config.rtp_port = 5000;
    
    RTSPClient client(config);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to Wowza stream\n";
        return;
    }
    
    if (!client.start_receiving()) {
        std::cerr << "Failed to start receiving\n";
        return;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    client.stop_receiving();
    client.disconnect();
}

void test_real_ipvm_camera() {
    std::cout << "\n=== Testing Real IPVM Camera ===\n";
    
    RTSPClient::Config config;
    config.rtsp_url = "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast";
    config.output_file = "real_ipvm_camera.rtp";
    config.max_packets = 3000;
    config.enable_logging = true;
    config.rtp_port = 5002;
    
    RTSPClient client(config);
    
    if (!client.connect()) {
        std::cerr << "Failed to connect to IPVM camera\n";
        return;
    }
    
    if (!client.start_receiving()) {
        std::cerr << "Failed to start receiving\n";
        return;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(20));
    
    client.stop_receiving();
    client.disconnect();
}

void test_real_bunny_stream() {
    std::cout << "\n=== Testing Real Big Buck Bunny Stream ===\n";
    
    RTSPClient::Config config;
    config.rtsp_url = "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov";
    config.output_file = "real_bunny_stream.rtp";
    config.max_packets = 2000;
    config.enable_logging = true;
    config.rtp_port = 5004;
    
    RTSPClient client(config);
    
    if (!client.connect()) return;
    if (!client.start_receiving()) return;
    
    std::this_thread::sleep_for(std::chrono::seconds(15));
    
    client.stop_receiving();
    client.disconnect();
}

void run_custom_test(const std::string& url, const std::string& output_file, int duration) {
    RTSPClient::Config config;
    config.rtsp_url = url;
    config.output_file = output_file;
    config.max_packets = 100000;
    config.enable_logging = true;
    config.rtp_port = 5000;
    config.timeout_ms = 10000;

    RTSPClient client(config);
    
    if (!client.connect()) return;
    if (!client.start_receiving()) return;
    
    std::cout << "Receiving for " << duration << " seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    
    client.stop_receiving();
    client.disconnect();
}

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--url" && i + 1 < argc) {
            run_custom_test(argv[i+1], "output.rtp", 10);
            return 0;
        }
    }

    test_real_wowza_stream();
    return 0;
}