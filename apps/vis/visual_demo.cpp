#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <signal.h>

// Define SDL_MAIN_HANDLED to prevent SDL from redefining main()
#define SDL_MAIN_HANDLED
#include <SDL.h>

import video_streaming.sender;


#include <SDL_ttf.h>

import video_streaming.receiver;
import video_streaming.media.frame;

using namespace std::chrono_literals;
using namespace video_streaming;

// Signal handler for graceful shutdown
std::atomic<bool> g_shutdown{false};
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

std::string format_bytes(uint64_t bytes) {
        if (bytes < 1024) return std::to_string(bytes) + " B";
        if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    }


void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", stopping demo...\n";
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    VideoSender::Config sender_config;
    VideoReceiver::Config receiver_config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            sender_config.port = std::stoi(argv[++i]);
            receiver_config.port = sender_config.port;
        } else if (arg == "--loss" && i + 1 < argc) {
            sender_config.packet_loss = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            sender_config.delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            sender_config.jitter_ms = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Visual Video Streaming Demo (SDL2)\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>      UDP port (default: 5000)\n";
            std::cout << "  --loss <percent>   Packet loss rate 0-100 (default: 0)\n";
            std::cout << "  --delay <ms>       Network delay 0-1000ms (default: 0)\n";
            std::cout << "  --jitter <ms>      Network jitter 0-200ms (default: 0)\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " --loss 5 --delay 50\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "❌ SDL Init failed: " << SDL_GetError() << std::endl;
        return -1;
    } 

    if (TTF_Init() != 0) {
        std::cerr << "❌ SDL_ttf Init failed: " << TTF_GetError() << std::endl;
        return -1;
    }
    
    // Create Window & Renderer
    SDL_Window* window = SDL_CreateWindow(
        "Visual Demo (Sender + Receiver)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        sender_config.width, sender_config.height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "❌ SDL Window failed: " << SDL_GetError() << std::endl;
         TTF_Quit();
        SDL_Quit();
        return -1;
    }

      TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!font) {
        std::cerr << "❌ Failed to load font! SDL_ttf Error: " << TTF_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = nullptr;
    int tex_width = 0;
    int tex_height = 0;
    
    // Frame buffer for passing data from receiver thread to main thread
    std::mutex render_mutex;
    std::vector<uint8_t> last_frame_data;
    int last_frame_width = 0;
    int last_frame_height = 0;
    bool new_frame_available = false;

    try {
        // Initialize Sender and Receiver
        VideoSender sender(sender_config);
        VideoReceiver receiver(receiver_config);

        // Set receiver callback to store frame data
        receiver.set_frame_callback([&](const Frame& frame) {
            std::lock_guard<std::mutex> lock(render_mutex);
            
            last_frame_data = frame.data; // Copy data
            last_frame_width = frame.width;
            last_frame_height = frame.height;
            new_frame_available = true;
        });

        // Start streaming
        if (!receiver.start()) {
            std::cerr << "❌ Failed to start receiver" << std::endl;
            throw std::runtime_error("Receiver start failed");
        }
        if (!sender.start()) {
            std::cerr << "❌ Failed to start sender" << std::endl;
            throw std::runtime_error("Sender start failed");
        }

        // Main Loop
        uint32_t frame_count = 0;
        auto last_time = std::chrono::steady_clock::now();
        SDL_Event event;
        while (!g_shutdown.load()) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) g_shutdown.store(true);
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) g_shutdown.store(true);
            }

            {
                std::lock_guard<std::mutex> lock(render_mutex);
                if (new_frame_available) {
                    if (!texture || tex_width != last_frame_width || tex_height != last_frame_height) {
                        if (texture) SDL_DestroyTexture(texture);
                        
                        tex_width = last_frame_width;
                        tex_height = last_frame_height;
                        
                        texture = SDL_CreateTexture(
                            renderer, SDL_PIXELFORMAT_YV12, 
                            SDL_TEXTUREACCESS_STREAMING, tex_width, tex_height
                        );
                        
                        SDL_SetWindowSize(window, tex_width, tex_height);
                    }
                    
                    if (texture && !last_frame_data.empty()) {
                        size_t y_size = tex_width * tex_height;
                        const uint8_t* y_plane = last_frame_data.data();
                        const uint8_t* u_plane = y_plane + y_size;
                        const uint8_t* v_plane = u_plane + y_size + (y_size / 4);
                        
                SDL_UpdateYUVTexture(
                    texture, nullptr,
                    y_plane, tex_width,
                    u_plane, tex_width / 2,
                    v_plane, tex_width / 2
                );
                        
                        new_frame_available = false;
                        frame_count++;
            }
        }
            }
            
            SDL_RenderClear(renderer); // Always clear
                if (texture) {
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                }
            SDL_RenderPresent(renderer);
            
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count() >= 1) {
                std::string title = "Visual Demo (Sender + Receiver) - " + std::to_string(frame_count) + " FPS";
                SDL_SetWindowTitle(window, title.c_str());
                frame_count = 0;
                last_time = now;
            }
            
            SDL_Delay(10);
        }

        // Cleanup
        sender.stop();
        receiver.stop();

    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
    }

    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
     TTF_CloseFont(font);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "🎬 Demo finished." << std::endl;
    
    return 0;
}
