#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>

import video_streaming.pipeline;
import video_streaming.logger;
import video_streaming.common.types;

using namespace video_streaming;

TEST_CASE("Pipeline Configuration and Lifecycle", "[pipeline]") {
    
    SECTION("Default Configuration") {
        PipelineConfig config;
        
        // Verify defaults
        REQUIRE(config.width == 640);
        REQUIRE(config.height == 480);
        REQUIRE(config.fps == 30);
        REQUIRE(config.enable_sender == true);
        REQUIRE(config.enable_receiver == true);
    }
    
    SECTION("Pipeline Lifecycle") {
        PipelineConfig config;
        config.enable_sender = true;
        config.enable_receiver = true;
        // Use different ports to avoid conflict with other tests/services
        config.dest_port = 5006;
        config.src_port = 5006; 
        
        Pipeline pipeline(config);
        
        // Start
        REQUIRE(pipeline.start() == true);
        
        // Run for a short period
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Get metrics
        auto metrics = pipeline.get_metrics();
        // Metrics might still be 0 after short run, but call should succeed
        REQUIRE(metrics.glass_to_glass_ms >= 0.0);
        
        // Stop
        pipeline.stop();
        
        // Restart check (optional, depending on implementation)
        // REQUIRE(pipeline.start() == true);
        // pipeline.stop();
    }
}