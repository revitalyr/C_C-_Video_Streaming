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

// Import coroutine modules
import video_streaming.async.coroutine_frame_generator;
import video_streaming.async.coroutine_network_sender;
import video_streaming.async.coroutine_receiver;

using namespace video_streaming::async;

// Global shutdown flag
std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down..." << std::endl;
    g_shutdown.store(true);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CoroutineNetworkSender::Config sender_config;
    CoroutineReceiver::Config receiver_config;
    
    // Set default configurations
    sender_config.destination_ip = "127.0.0.1";
    receiver_config.bind_ip = "0.0.0.0";
    sender_config.port = 5000;
    receiver_config.port = 5000;
    sender_config.fps = 30;
    sender_config.width = 1920;
    sender_config.height = 1080;
    sender_config.bitrate = 4000000;
    
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
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --port <port>        UDP port (default: 5000)\n";
            std::cout << "  --loss <percent>     Packet loss percentage (default: 0)\n";
            std::cout << "  --delay <ms>         Network delay in milliseconds (default: 0)\n";
            std::cout << "  --jitter <ms>        Network jitter in milliseconds (default: 0)\n";
            std::cout << "Examples:\n";
            std::cout << "  " << argv[0] << " --loss 5 --delay 50\n";
            return 0;
        }
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "❌ SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        std::cerr << "❌ SDL_ttf initialization failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    
    // Create window and renderer
    SDL_Window* window = SDL_CreateWindow(
        "Coroutine Visual Demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        sender_config.width, sender_config.height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window) {
        std::cerr << "❌ Window creation failed: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "❌ Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    
    // Load font
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24);
    if (!font) {
        std::cerr << "❌ Font loading failed: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "✅ Coroutine Visual Demo initialized successfully" << std::endl;
    
    // Initialize coroutine components
    CoroutineFrameGenerator frame_generator({
        sender_config.width, 
        sender_config.height, 
        sender_config.fps
    });
    
    CoroutineNetworkSender network_sender(sender_config);
    CoroutineReceiver network_receiver(receiver_config);
    
    // Frame data for rendering
    std::vector<uint8_t> last_frame_data;
    int last_frame_width = 0;
    int last_frame_height = 0;
    bool new_frame_available = false;
    std::mutex frame_mutex;
    
    // Statistics
    uint32_t frames_rendered = 0;
    uint32_t frames_sent = 0;
    uint32_t frames_received = 0;
    
    // Set receiver callback
    network_receiver.set_frame_callback([&](const Frame& frame) {
        std::lock_guard<std::mutex> lock(frame_mutex);
        last_frame_data = frame.data;
        last_frame_width = frame.width;
        last_frame_height = frame.height;
        new_frame_available = true;
        frames_received++;
        
        std::cout << "📹 CALLBACK: Received frame " << frames_received 
                  << ": " << frame.width << "x" << frame.height 
                  << " size: " << frame.data.size() << " bytes" << std::endl;
    });
    
    // Start network components
    if (!network_sender.start() || !network_receiver.start()) {
        std::cerr << "❌ Failed to start network components" << std::endl;
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    
    std::cout << "🚀 Starting main coroutine loop..." << std::endl;
    
    // Start frame generator coroutine
    auto frame_gen = frame_generator.generate_frames();
    
    // Main SDL loop with coroutine integration
    SDL_Event event;
    auto last_time = std::chrono::steady_clock::now();
    auto last_send_time = last_time;
    
    while (!g_shutdown.load()) {
        // Handle SDL events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) g_shutdown.store(true);
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) g_shutdown.store(true);
        }
        
        auto now = std::chrono::steady_clock::now();
        
        // Generate and send frame at configured FPS
        auto frame_interval = std::chrono::milliseconds(1000 / sender_config.fps);
        if (now - last_send_time >= frame_interval) {
            // Get next frame from generator
            auto frame_opt = frame_gen.next();
            if (frame_opt) {
                auto& frame = *frame_opt;
                
                // Send frame asynchronously
                auto send_task = network_sender.send_frame_async(*frame);
                
                // For now, we'll get the result synchronously
                // In a real implementation, we'd have a proper async task scheduler
                bool success = send_task.get();
                if (success) {
                    frames_sent++;
                    std::cout << "📤 Sent frame " << frames_sent << std::endl;
                }
                
                last_send_time = now;
            }
        }
        
        // Receive frames asynchronously
        auto receive_task = network_receiver.receive_frame_async();
        if (receive_task.is_ready()) {
            receive_task.get(); // Process the result
        }
        
        // Render frame
        SDL_RenderClear(renderer);
        
        {
            std::lock_guard<std::mutex> lock(frame_mutex);
            if (new_frame_available && !last_frame_data.empty()) {
                // Create texture from frame data
                SDL_Texture* texture = SDL_CreateTexture(
                    renderer, 
                    SDL_PIXELFORMAT_YV12,
                    SDL_TEXTUREACCESS_STREAMING,
                    last_frame_width, 
                    last_frame_height
                );
                
                if (texture) {
                    // Update texture with frame data
                    const size_t y_size = last_frame_width * last_frame_height;
                    const size_t uv_size = y_size / 4;
                    
                    uint8_t* y_plane = last_frame_data.data();
                    uint8_t* u_plane = y_plane + y_size;
                    uint8_t* v_plane = u_plane + uv_size;
                    
                    SDL_UpdateYUVTexture(
                        texture, nullptr,
                        y_plane, last_frame_width,
                        u_plane, last_frame_width / 2,
                        v_plane, last_frame_width / 2
                    );
                    
                    // Render texture
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_DestroyTexture(texture);
                    
                    frames_rendered++;
                    new_frame_available = false;
                }
            }
        }
        
        // Render statistics
        if (font) {
            std::string stats_text = "Sent: " + std::to_string(frames_sent) + 
                                   " | Received: " + std::to_string(frames_received) +
                                   " | Rendered: " + std::to_string(frames_rendered);
            
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* surface = TTF_RenderText_Solid(font, stats_text.c_str(), white);
            if (surface) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                if (texture) {
                    SDL_Rect rect = {10, 10, surface->w, surface->h};
                    SDL_RenderCopy(renderer, texture, nullptr, &rect);
                    SDL_DestroyTexture(texture);
                }
                SDL_FreeSurface(surface);
            }
        }
        
        SDL_RenderPresent(renderer);
        
        // Update window title every second
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_time).count() >= 1) {
            std::string title = "Coroutine Visual Demo - Sent: " + std::to_string(frames_sent) + 
                              " | Received: " + std::to_string(frames_received) +
                              " | FPS: " + std::to_string(frames_rendered);
            SDL_SetWindowTitle(window, title.c_str());
            frames_rendered = 0;
            last_time = now;
        }
        
        // Small delay to prevent 100% CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::cout << "🛑 Shutting down coroutine demo..." << std::endl;
    
    // Cleanup
    network_sender.stop();
    network_receiver.stop();
    
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    
    std::cout << "✅ Coroutine Visual Demo stopped successfully" << std::endl;
    return 0;
}
