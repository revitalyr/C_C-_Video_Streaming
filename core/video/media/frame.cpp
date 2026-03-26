module;

#include <cstring>
#include <random>
#include <cmath>

module video_streaming.media.frame;

import video_streaming.common.types;

namespace video_streaming {

Frame::Frame(int w, int h, PixelFormat fmt, Timestamp ts)
    : timestamp(ts), format(fmt), width(w), height(h) {
    allocate_buffer();
}

void Frame::allocate_buffer() {
    size_t size = get_frame_size();
    data.resize(size);
    stride = width;
}

void Frame::clear() {
    data.clear();
    width = height = stride = 0;
    type = FrameType::Unknown;
    format = PixelFormat::Unknown;
    timestamp = 0;
}

Frame FrameFactory::create_test_pattern(int width, int height, Timestamp timestamp) {
    Frame frame(width, height, PixelFormat::YUV420P, timestamp);
    frame.type = FrameType::IFrame;
    fill_yuv420p(frame, width, height);
    return frame;
}

Frame FrameFactory::create_color_bar(int width, int height, Timestamp timestamp) {
    Frame frame(width, height, PixelFormat::YUV420P, timestamp);
    frame.type = FrameType::IFrame;
    
    // Create 8 color bars
    int bar_width = width / 8;
    
    u8* y_plane = frame.data.data();
    u8* u_plane = y_plane + width * height;
    u8* v_plane = u_plane + (width * height) / 4;
    
    for (int bar = 0; bar < 8; ++bar) {
        int x_start = bar * bar_width;
        int x_end = std::min((bar + 1) * bar_width, width);
        
        generate_color_bar_data(y_plane, u_plane, v_plane, width, height, bar);
    }
    
    return frame;
}

Frame FrameFactory::create_gradient(int width, int height, Timestamp timestamp) {
    Frame frame(width, height, PixelFormat::YUV420P, timestamp);
    frame.type = FrameType::PFrame;
    generate_gradient_data(frame.data.data(), 
                         frame.data.data() + width * height,
                         frame.data.data() + width * height + (width * height) / 4,
                         width, height);
    return frame;
}

Frame FrameFactory::create_noise(int width, int height, Timestamp timestamp) {
    Frame frame(width, height, PixelFormat::YUV420P, timestamp);
    frame.type = FrameType::PFrame;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : frame.data) {
        byte = static_cast<u8>(dis(gen));
    }
    
    return frame;
}

void FrameFactory::fill_yuv420p(Frame& frame, int width, int height) {
    u8* y_plane = frame.data.data();
    u8* u_plane = y_plane + width * height;
    u8* v_plane = u_plane + (width * height) / 4;
    
    // Fill Y plane with gradient
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            y_plane[y * width + x] = static_cast<u8>((x + y) / 2 % 256);
        }
    }
    
    // Fill U and V planes with constant values
    std::fill_n(u_plane, (width * height) / 4, 128);
    std::fill_n(v_plane, (width * height) / 4, 128);
}

void FrameFactory::generate_color_bar_data(u8* y_plane, u8* u_plane, u8* v_plane, 
                                          int width, int height, int bar_index) {
    // Standard color bar values (Y, U, V)
    static const u8 color_bars[8][3] = {
        {235, 128, 128}, // White
        {210, 16,  146}, // Yellow
        {170, 166, 16},  // Cyan
        {145, 54,  34},  // Green
        {107, 202, 222}, // Magenta
        {82,  90,  240}, // Red
        {41,  240, 110}, // Blue
        {16,  128, 128}  // Black
    };
    
    u8 y = color_bars[bar_index][0];
    u8 u = color_bars[bar_index][1];
    u8 v = color_bars[bar_index][2];
    
    int bar_width = width / 8;
    int x_start = bar_index * bar_width;
    int x_end = std::min((bar_index + 1) * bar_width, width);
    
    // Fill Y plane
    for (int py = 0; py < height; ++py) {
        for (int px = x_start; px < x_end; ++px) {
            y_plane[py * width + px] = y;
        }
    }
    
    // Fill U and V planes (subsampled)
    int uv_width = width / 2;
    int uv_height = height / 2;
    int uv_x_start = x_start / 2;
    int uv_x_end = x_end / 2;
    
    for (int py = 0; py < uv_height; ++py) {
        for (int px = uv_x_start; px < uv_x_end; ++px) {
            u_plane[py * uv_width + px] = u;
            v_plane[py * uv_width + px] = v;
        }
    }
}

void FrameFactory::generate_gradient_data(u8* y_plane, u8* u_plane, u8* v_plane, 
                                         int width, int height) {
    // Y plane: diagonal gradient
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            y_plane[y * width + x] = static_cast<u8>(
                (x * 255 / width + y * 255 / height) / 2
            );
        }
    }
    
    // U and V planes: circular gradient
    int uv_width = width / 2;
    int uv_height = height / 2;
    int center_x = uv_width / 2;
    int center_y = uv_height / 2;
    int max_radius = std::min(center_x, center_y);
    
    for (int y = 0; y < uv_height; ++y) {
        for (int x = 0; x < uv_width; ++x) {
            int dx = x - center_x;
            int dy = y - center_y;
            int distance = static_cast<int>(std::sqrt(dx * dx + dy * dy));
            
            u8 u_val = static_cast<u8>(128 + (distance * 127 / max_radius));
            u8 v_val = static_cast<u8>(128 - (distance * 127 / max_radius));
            
            u_plane[y * uv_width + x] = u_val;
            v_plane[y * uv_width + x] = v_val;
        }
    }
}

} // namespace video_streaming
