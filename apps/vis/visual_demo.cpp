/*
 * Video Stream Player with Network Simulation
 * 
 * Technical specifications:
 * - FFmpeg-based video decoding (H.264, VP9, AV1 support)
 * - SDL2 hardware-accelerated rendering
 * - Network condition simulation (packet loss, delay, jitter)
 * - Real-time performance metrics
 * 
 * Supported codecs: H.264, H.265, VP8, VP9, AV1
 * Output format: RGB24 (8-bit per channel)
 * Frame rate: 30 FPS target with adaptive limiting
 */

#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <cstdlib>
#include <signal.h>
#include <optional>
#include <random>

// FFmpeg includes for video decoding
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

import video_streaming.media.frame;
import video_streaming.common.types;
import video_streaming.network.endpoint;
import video_streaming.async.coroutine_frame_generator;
import video_streaming.async.coroutine_network_sender;
import video_streaming.async.coroutine_receiver;

using namespace video_streaming::async;

// Video file decoder class
class VideoFileDecoder {
private:
    AVFormatContext* format_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    const AVCodec* codec_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
    int video_stream_index_ = -1;
    std::string filename_;
    
public:
    explicit VideoFileDecoder(const std::string& filename) : filename_(filename) {}
    
    ~VideoFileDecoder() {
        cleanup();
    }
    
    // Initialize FFmpeg decoder and allocate resources
    bool initialize() {
        avformat_network_init();
        
        if (avformat_open_input(&format_ctx_, filename_.c_str(), nullptr, nullptr) < 0) {
            std::cerr << "ERROR: Could not open video file: " << filename_ << std::endl;
            return false;
        }
        
        if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
            std::cerr << "ERROR: Could not find stream information" << std::endl;
            return false;
        }
        
        // Locate video stream
        video_stream_index_ = -1;
        for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
            if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index_ = i;
                break;
            }
        }
        
        if (video_stream_index_ == -1) {
            std::cerr << "ERROR: Could not find video stream" << std::endl;
            return false;
        }
        
        AVCodecParameters* codec_params = format_ctx_->streams[video_stream_index_]->codecpar;
        
        codec_ = avcodec_find_decoder(codec_params->codec_id);
        if (!codec_) {
            std::cerr << "ERROR: Unsupported codec" << std::endl;
            return false;
        }
        
        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_) {
            std::cerr << "ERROR: Could not allocate codec context" << std::endl;
            return false;
        }
        
        if (avcodec_parameters_to_context(codec_ctx_, codec_params) < 0) {
            std::cerr << "ERROR: Could not copy codec parameters" << std::endl;
            return false;
        }
        
        if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
            std::cerr << "ERROR: Could not open codec" << std::endl;
            return false;
        }
        
        frame_ = av_frame_alloc();
        packet_ = av_packet_alloc();
        
        if (!frame_ || !packet_) {
            std::cerr << "ERROR: Could not allocate frame or packet" << std::endl;
            return false;
        }
        
        // Initialize pixel format conversion context
        sws_ctx_ = sws_getContext(
            codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
            codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        std::cout << "INFO: Video decoder initialized: " << codec_ctx_->width << "x" << codec_ctx_->height 
                  << " codec: " << avcodec_get_name(codec_->id) << std::endl;
        
        return true;
    }
    
    // Decode next frame from video file
    std::optional<video_streaming::Frame> read_frame() {
        while (av_read_frame(format_ctx_, packet_) >= 0) {
            if (packet_->stream_index != video_stream_index_) {
                av_packet_unref(packet_);
                continue;
            }
            
            if (avcodec_send_packet(codec_ctx_, packet_) < 0) {
                av_packet_unref(packet_);
                return std::nullopt;
            }
            
            int ret = avcodec_receive_frame(codec_ctx_, frame_);
            av_packet_unref(packet_);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                return std::nullopt;
            }
            
            // Convert frame to RGB24 format
            AVFrame* rgb_frame = av_frame_alloc();
            rgb_frame->format = AV_PIX_FMT_RGB24;
            rgb_frame->width = codec_ctx_->width;
            rgb_frame->height = codec_ctx_->height;
            
            if (av_image_alloc(rgb_frame->data, rgb_frame->linesize, 
                              rgb_frame->width, rgb_frame->height, 
                              AV_PIX_FMT_RGB24, 32) < 0) {
                av_frame_free(&rgb_frame);
                return std::nullopt;
            }
            
            sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0, frame_->height,
                      rgb_frame->data, rgb_frame->linesize);
            
            video_streaming::Frame result_frame;
            result_frame.width = rgb_frame->width;
            result_frame.height = rgb_frame->height;
            result_frame.format = video_streaming::PixelFormat::RGB24;
            result_frame.timestamp = static_cast<uint32_t>(frame_->pts);
            
            size_t rgb_size = rgb_frame->width * rgb_frame->height * 3;
            result_frame.data.assign(rgb_frame->data[0], rgb_frame->data[0] + rgb_size);
            
            av_freep(&rgb_frame->data[0]);
            av_frame_free(&rgb_frame);
            
            return result_frame;
        }
        return std::nullopt;
    }
    
    // Release FFmpeg resources
    void cleanup() {
        if (sws_ctx_) { sws_freeContext(sws_ctx_); sws_ctx_ = nullptr; }
        if (frame_) av_frame_free(&frame_);
        if (packet_) av_packet_free(&packet_);
        if (codec_ctx_) avcodec_free_context(&codec_ctx_);
        if (format_ctx_) avformat_close_input(&format_ctx_);
    }
};

// Application state management
std::atomic<bool> running{true};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    running = false;
}

int main(int argc, char* argv[]) {
    // Default video source - BigBuckBunny test video
    std::string video_url = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";
    
    // Network simulation parameters
    double packet_loss = 0.0;      // Packet loss percentage (0-100)
    int network_delay = 0;         // Base network delay in milliseconds
    int network_jitter = 0;        // Random delay variation in milliseconds
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--url" && i + 1 < argc) video_url = argv[++i];
        else if (arg == "--loss" && i + 1 < argc) packet_loss = std::stod(argv[++i]);
        else if (arg == "--delay" && i + 1 < argc) network_delay = std::stoi(argv[++i]);
        else if (arg == "--jitter" && i + 1 < argc) network_jitter = std::stoi(argv[++i]);
        else if (arg == "--help") {
            std::cout << "Video Stream Player with Network Simulation\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --url <url>        Video file URL or path\n";
            std::cout << "  --loss <percent>   Packet loss percentage (0-100)\n";
            std::cout << "  --delay <ms>       Network delay in milliseconds\n";
            std::cout << "  --jitter <ms>      Network jitter in milliseconds\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " --url video.mp4\n";
            std::cout << "  " << argv[0] << " --loss 5 --delay 50 --jitter 10\n";
            return 0;
        }
    }
    
    // Initialize signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize video decoder
    VideoFileDecoder decoder(video_url);
    if (!decoder.initialize()) {
        std::cerr << "FATAL: Failed to initialize video decoder" << std::endl;
        return 1;
    }
    
    // Initialize SDL2 for video output
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "FATAL: SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Initialize SDL_ttf for text overlay
    if (TTF_Init() < 0) {
        std::cerr << "FATAL: SDL_ttf initialization failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create display window and renderer
    SDL_Window* window = SDL_CreateWindow("Video Stream Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    
    // Network simulation random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> loss_dist(0.0, 100.0);
    std::uniform_int_distribution<> jitter_dist(0, std::max(1, network_jitter));
    
    // Performance metrics
    int frame_count = 0, frames_dropped = 0;
    auto fps_start_time = std::chrono::steady_clock::now();
    
    // Main rendering loop
    while (running) {
        // Process SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }
        
        // Simulate network packet loss
        if (packet_loss > 0.0 && loss_dist(gen) < packet_loss) {
            frames_dropped++;
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // Skip frame timing
            continue; 
        }
        
        // Apply network delay
        if (network_delay > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(network_delay));
        }
        
        // Apply network jitter
        if (network_jitter > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(jitter_dist(gen)));
        }
        
        // Decode next video frame
        auto video_frame = decoder.read_frame();
        if (!video_frame) {
            decoder.initialize(); // Restart video on EOF
            continue;
        }
        
        // Clear framebuffer
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Render video frame
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, video_frame->width, video_frame->height);
        if (texture) {
            SDL_UpdateTexture(texture, nullptr, video_frame->data.data(), video_frame->width * 3);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_DestroyTexture(texture);
        }
        
        // Render performance overlay
        if (font) {
            frame_count++;
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - fps_start_time).count();
            double fps = elapsed > 0 ? (double)frame_count / elapsed : 0.0;
            
            std::string info = "FPS: " + std::to_string((int)fps) + 
                            " | Loss: " + std::to_string(packet_loss) + "%" +
                            " | Dropped: " + std::to_string(frames_dropped);
            
            SDL_Surface* surf = TTF_RenderText_Blended(font, info.c_str(), {255, 255, 255, 255});
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_Rect r = {10, 10, surf->w, surf->h};
                
                // Render background for text visibility
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_Rect bg = {r.x - 5, r.y - 5, r.w + 10, r.h + 10};
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
                SDL_RenderFillRect(renderer, &bg);
                
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
                SDL_FreeSurface(surf);
            }
        }
        
        // Present frame to display
        SDL_RenderPresent(renderer);
        
        // Frame rate limiting (target 30 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Cleanup resources
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}