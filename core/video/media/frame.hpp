#pragma once

#include "common/types.hpp"

enum class FrameType {
    Unknown = 0,
    IFrame,  // Key frame
    PFrame,  // Predictive frame
    BFrame,  // Bidirectional frame
    Audio
};

enum class PixelFormat {
    Unknown = 0,
    YUV420P,
    YUYV422,
    RGB24,
    RGBA32
};

struct Frame {
    Bytes data;
    Timestamp timestamp;
    FrameType type{FrameType::Unknown};
    PixelFormat format{PixelFormat::Unknown};
    int width{0};
    int height{0};
    int stride{0};
    
    Frame() = default;
    Frame(int w, int h, PixelFormat fmt, Timestamp ts);
    
    bool is_valid() const {
        return !data.empty() && width > 0 && height > 0 && timestamp > 0;
    }
    
    bool is_key_frame() const {
        return type == FrameType::IFrame;
    }
    
    size_t get_frame_size() const {
        switch (format) {
            case PixelFormat::YUV420P:
                return width * height * 3 / 2;
            case PixelFormat::YUYV422:
                return width * height * 2;
            case PixelFormat::RGB24:
                return width * height * 3;
            case PixelFormat::RGBA32:
                return width * height * 4;
            default:
                return data.size();
        }
    }
    
    void allocate_buffer();
    void clear();
};

// Frame factory for creating synthetic test frames
class FrameFactory {
public:
    static Frame create_test_pattern(int width, int height, Timestamp timestamp);
    static Frame create_color_bar(int width, int height, Timestamp timestamp);
    static Frame create_gradient(int width, int height, Timestamp timestamp);
    static Frame create_noise(int width, int height, Timestamp timestamp);

private:
    static void fill_yuv420p(Frame& frame, int width, int height);
    static void generate_color_bar_data(u8* y_plane, u8* u_plane, u8* v_plane, 
                                       int width, int height, int bar_index);
    static void generate_gradient_data(u8* y_plane, u8* u_plane, u8* v_plane, 
                                       int width, int height);
};
