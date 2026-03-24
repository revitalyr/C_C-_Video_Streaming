module;

#include <vector>
#include <cstdint>
#include <chrono>

export module video_streaming.media.frame;

import video_streaming.common.types;

namespace video_streaming {

export enum class PixelFormat {
    YUV420P,
    RGB24,
    NV12,
    Unknown
};

export enum class FrameType {
    IFrame,
    PFrame,
    BFrame,
    Unknown
};

export struct Frame {
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Unknown;
    FrameType type = FrameType::Unknown;
    
    std::vector<uint8_t> data;
    Timestamp timestamp = 0; // Presentation timestamp
    
    bool is_valid() const {
        return width > 0 && height > 0 && !data.empty();
    }
};

export struct EncodedFrame {
    std::vector<uint8_t> data;
    Timestamp timestamp = 0;
    bool is_keyframe = false;
};

export class FrameFactory {
public:
    static Frame create_yuv420p(int width, int height, Timestamp timestamp = 0);
    static Frame create_test_pattern(int width, int height, Timestamp timestamp = 0);
};

export enum class NalType : uint8_t {
    Slice = 1,
    IDR = 5,
    SPS = 7,
    PPS = 8,
    FU_A = 28,
    Unknown = 0
};

export constexpr uint8_t FU_A_INDICATOR = 28;
export constexpr uint8_t FU_A_START_BIT = 0x80;
export constexpr uint8_t FU_A_END_BIT = 0x40;

} // namespace video_streaming