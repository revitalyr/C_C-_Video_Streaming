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
    std::vector<uint8_t> data;
    Timestamp timestamp = 0; // Presentation timestamp
    FrameType type = FrameType::Unknown;
    PixelFormat format = PixelFormat::Unknown;
    int width = 0;
    int height = 0;
    int stride = 0;

    Frame() = default;
    Frame(int w, int h, PixelFormat fmt, Timestamp ts);
    
    size_t get_frame_size() const {
        switch (format) {
            case PixelFormat::YUV420P:
                return static_cast<size_t>(width) * height * 3 / 2;
            case PixelFormat::RGB24:
                return static_cast<size_t>(width) * height * 3;
            default:
                return data.size();
        }
    }

    void allocate_buffer();
    void clear();

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
    static Frame create_test_pattern(int width, int height, Timestamp timestamp = 0);
    static Frame create_color_bar(int width, int height, Timestamp timestamp = 0);
    static Frame create_gradient(int width, int height, Timestamp timestamp = 0);
    static Frame create_noise(int width, int height, Timestamp timestamp = 0);

private:
    static void fill_yuv420p(Frame& frame, int width, int height);
    static void generate_color_bar_data(u8* y_plane, u8* u_plane, u8* v_plane, 
                                       int width, int height, int bar_index);
    static void generate_gradient_data(u8* y_plane, u8* u_plane, u8* v_plane, 
                                       int width, int height);
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