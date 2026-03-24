#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <cmath>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <vector>
#include <stdexcept>
#include <signal.h>
#include <random>

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
    std::cout << "\n🛑 Signal " << signal << " received, stopping demo..." << std::endl;
    g_shutdown.store(true);
}

// Helper function to render animated gradient background with packet loss visualization
void render_gradient_background(SDL_Renderer* renderer, int width, int height, auto start_time, 
                           double packet_loss_rate, uint32_t packets_received, uint32_t packets_lost) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    
    // Create smooth color transition over 4 seconds
    float phase = (elapsed % 4000) / 4000.0f * 2.0f * M_PI;
    
    // Calculate RGB values with smooth sine wave transitions (more subtle)
    uint8_t r = static_cast<uint8_t>((sin(phase) * 0.3f + 0.5f) * 20 + 15);  // Dark red to medium red
    uint8_t g = static_cast<uint8_t>((sin(phase + 2.0f) * 0.3f + 0.5f) * 20 + 15);  // Dark green to medium green  
    uint8_t b = static_cast<uint8_t>((sin(phase + 4.0f) * 0.3f + 0.5f) * 30 + 20);  // Dark blue to medium blue
    
    // Create semi-transparent gradient effect
    for (int y = 0; y < height; y += 2) {
        float gradient_factor = (float)y / height;
        uint8_t gradient_r = static_cast<uint8_t>(r * (1.0f - gradient_factor * 0.2f));
        uint8_t gradient_g = static_cast<uint8_t>(g * (1.0f - gradient_factor * 0.2f));
        uint8_t gradient_b = static_cast<uint8_t>(b * (1.0f - gradient_factor * 0.2f));
        
        // Make background semi-transparent (alpha = 100)
        SDL_SetRenderDrawColor(renderer, gradient_r, gradient_g, gradient_b, 100);
        SDL_RenderDrawLine(renderer, 0, y, width, y);
        SDL_RenderDrawLine(renderer, 0, y + 1, width, y + 1);
    }
    
    // Visualize packet loss as red X marks
    if (packet_loss_rate > 0 && packets_received > 0) {
        double actual_loss = (double)packets_lost / (packets_received + packets_lost) * 100.0;
        int loss_marks = static_cast<int>(actual_loss / 10.0); // One X per 10% loss
        
        for (int i = 0; i < loss_marks && i < 10; i++) {
            int x = width - 30 - (i * 25);
            int y = 30 + (i % 2) * 20;
            
            // Draw red X for packet loss
            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 200);
            SDL_RenderDrawLine(renderer, x - 8, y - 8, x + 8, y + 8);
            SDL_RenderDrawLine(renderer, x - 8, y + 8, x + 8, y - 8);
        }
    }
    
    // Draw network status indicator
    int status_y = height - 40;
    if (packet_loss_rate == 0) {
        // Green circle for good connection (using lines)
        SDL_SetRenderDrawColor(renderer, 50, 255, 50, 200);
        SDL_RenderDrawLine(renderer, 25, status_y - 5, 35, status_y + 5);
        SDL_RenderDrawLine(renderer, 25, status_y + 5, 35, status_y - 5);
    } else {
        // Red X for packet loss (using lines)
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 200);
        SDL_RenderDrawLine(renderer, 25, status_y - 5, 35, status_y + 5);
        SDL_RenderDrawLine(renderer, 25, status_y + 5, 35, status_y - 5);
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    VideoSender::Config sender_config;
    VideoReceiver::Config receiver_config;
    
    // Set default configurations
    sender_config.destination_ip = "127.0.0.1";  // Send to localhost
    receiver_config.bind_ip = "0.0.0.0";           // Listen on all interfaces
    
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
    
    // Text overlay cache (create only when parameters change)
    SDL_Texture* loss_texture = nullptr;
    SDL_Texture* delay_texture = nullptr;
    SDL_Texture* jitter_texture = nullptr;
    SDL_Rect loss_rect = {10, 10, 0, 0};
    SDL_Rect delay_rect = {10, 40, 0, 0};
    SDL_Rect jitter_rect = {10, 70, 0, 0};
    
    // Animation state for gradient background
    auto gradient_start = std::chrono::steady_clock::now();
    
    // Network statistics for visualization
    uint32_t packets_received_total = 0;
    uint32_t packets_lost_total = 0;
    auto stats_update_time = std::chrono::steady_clock::now();

    try {
        // Initialize Sender and Receiver
        VideoSender sender(sender_config);
        VideoReceiver receiver(receiver_config);

        // Set receiver callback to store frame data and collect stats
        receiver.set_frame_callback([&](const Frame& frame) {
        std::lock_guard<std::mutex> lock(render_mutex);
        std::cout << "📹 Received frame: " << frame.width << "x" << frame.height 
                  << " size: " << frame.data.size() << " bytes" << std::endl;
        last_frame_data = frame.data; // Copy data
        last_frame_width = frame.width;
        last_frame_height = frame.height;
        new_frame_available = true;
        
        // Update statistics for visualization
        packets_received_total++;
        
        // Simulate packet loss based on configuration
        if (sender_config.packet_loss > 0) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<> loss_dist(0.0, 100.0);
            
            if (loss_dist(gen) < sender_config.packet_loss) {
                packets_lost_total++;
            }
        }
    });

        // Start streaming
        std::cout << "🔧 Starting receiver on port " << receiver_config.port << std::endl;
        if (!receiver.start()) {
            std::cerr << "❌ Failed to start receiver" << std::endl;
            throw std::runtime_error("Receiver start failed");
        }
        
        std::cout << "🔧 Starting sender to " << sender_config.destination_ip 
                  << ":" << sender_config.port << std::endl;
        if (!sender.start()) {
            std::cerr << "❌ Failed to start sender" << std::endl;
            throw std::runtime_error("Sender start failed");
        }
        
        std::cout << "✅ Both sender and receiver started!" << std::endl;

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
                    std::cout << "🎬 Rendering frame #" << frame_count << std::endl;
                    
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
                        size_t uv_size = y_size / 4;
                        const uint8_t* y_plane = last_frame_data.data();
                        const uint8_t* u_plane = y_plane + y_size;
                        const uint8_t* v_plane = u_plane + uv_size;
                        
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
            
            // Render animated gradient background with packet loss visualization
            int window_width, window_height;
            SDL_GetWindowSize(window, &window_width, &window_height);
            render_gradient_background(renderer, window_width, window_height, gradient_start, 
                                     sender_config.packet_loss, packets_received_total, packets_lost_total);
            
            if (texture) {
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            }
                
                // Render network metrics overlay (create once, reuse)
                if (font && (!loss_texture || !delay_texture || !jitter_texture)) {
                    // Cleanup old textures
                    if (loss_texture) SDL_DestroyTexture(loss_texture);
                    if (delay_texture) SDL_DestroyTexture(delay_texture);
                    if (jitter_texture) SDL_DestroyTexture(jitter_texture);
                    
                    SDL_Color red = {255, 100, 100, 255};
                    SDL_Color yellow = {255, 255, 100, 255};
                    SDL_Color white = {255, 255, 255, 255};
                    
                    // Create text surfaces
                    std::string loss_text = "Loss: " + std::to_string(sender_config.packet_loss) + "%";
                    std::string delay_text = "Delay: " + std::to_string(sender_config.delay_ms) + "ms";
                    std::string jitter_text = "Jitter: " + std::to_string(sender_config.jitter_ms) + "ms";
                    
                    SDL_Surface* loss_surface = TTF_RenderText_Solid(font, loss_text.c_str(), red);
                    SDL_Surface* delay_surface = TTF_RenderText_Solid(font, delay_text.c_str(), yellow);
                    SDL_Surface* jitter_surface = TTF_RenderText_Solid(font, jitter_text.c_str(), white);
                    
                    if (loss_surface && delay_surface && jitter_surface) {
                        // Create textures
                        loss_texture = SDL_CreateTextureFromSurface(renderer, loss_surface);
                        delay_texture = SDL_CreateTextureFromSurface(renderer, delay_surface);
                        jitter_texture = SDL_CreateTextureFromSurface(renderer, jitter_surface);
                        
                        if (loss_texture && delay_texture && jitter_texture) {
                            // Set rectangles
                            loss_rect = {10, 10, loss_surface->w, loss_surface->h};
                            delay_rect = {10, 40, delay_surface->w, delay_surface->h};
                            jitter_rect = {10, 70, jitter_surface->w, jitter_surface->h};
                        }
                        
                        // Cleanup surfaces
                        SDL_FreeSurface(loss_surface);
                        SDL_FreeSurface(delay_surface);
                        SDL_FreeSurface(jitter_surface);
                    }
                }
                
                // Render cached text textures
                if (loss_texture && delay_texture && jitter_texture) {
                    SDL_RenderCopy(renderer, loss_texture, nullptr, &loss_rect);
                    SDL_RenderCopy(renderer, delay_texture, nullptr, &delay_rect);
                    SDL_RenderCopy(renderer, jitter_texture, nullptr, &jitter_rect);
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
    
    // Cleanup text overlay textures
    if (loss_texture) SDL_DestroyTexture(loss_texture);
    if (delay_texture) SDL_DestroyTexture(delay_texture);
    if (jitter_texture) SDL_DestroyTexture(jitter_texture);
    
    SDL_DestroyRenderer(renderer);
    TTF_CloseFont(font);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "🎬 Demo finished." << std::endl;
    
    return 0;
}
