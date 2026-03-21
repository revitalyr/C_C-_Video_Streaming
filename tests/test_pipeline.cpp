#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

#include "../network/receiver.hpp"
#include "../network/sender.hpp"
#include "../rtp/rtp_packet.hpp"

import video_streaming.pipeline;
import video_streaming.logger;

using namespace video_streaming;

TEST_CASE("Pipeline Lifecycle", "[pipeline]") {
    PipelineConfig config;
    config.width = 320;
    config.height = 240;
    config.fps = 15;
    
    Pipeline pipeline(config);
    
    SECTION("Start and Stop") {
        REQUIRE(pipeline.start() == true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pipeline.stop();
    }

    SECTION("Restart") {
        REQUIRE(pipeline.start() == true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pipeline.stop();
        REQUIRE(pipeline.start() == true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pipeline.stop();
    }
}

TEST_CASE("Sender and Receiver Unit", "[network]") {
    // Basic loopback test
    const int test_port = 15000;
    Receiver receiver(test_port);
    Sender sender("127.0.0.1", test_port);

    REQUIRE(receiver.start() == true);
    REQUIRE(sender.start() == true);

    RtpPacket packet;
    packet.header.sequence_number = 123;
    packet.header.timestamp = 456789;
    packet.payload = {0x01, 0x02, 0x03};

    REQUIRE(sender.send(packet) == true);

    // Wait for packet
    int retries = 100;
    std::optional<RtpPacket> received;
    while (retries-- > 0) {
        received = receiver.receive();
        if (received) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(received.has_value());
    REQUIRE(received->header.sequence_number == 123);
    REQUIRE(received->payload == packet.payload);
}