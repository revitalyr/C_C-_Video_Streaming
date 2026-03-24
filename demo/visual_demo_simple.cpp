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
#include <SDL_ttf.h>

using namespace std::chrono_literals;

// Signal handler for graceful shutdown
std::atomic<bool> g_shutdown{false};
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

void signal_handler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", stopping demo...\n";
    g_shutdown.store(true);
}

std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    double packet_loss = 0.0;
    int delay_ms = 0;
    int jitter_ms = 0;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--loss" && i + 1 < argc) {
            packet_loss = std::stod(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--jitter" && i + 1 < argc) {
            jitter_ms = std::stoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Visual Video Streaming Demo (SDL2)\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
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
        SDL_Quit();
        return -1;
    }
    
    // Create Window & Renderer
    SDL_Window* window = SDL_CreateWindow(
        "Visual Demo - Simple Test Pattern",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
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
    
    // Metrics
    std::atomic<uint64_t> frames_sent{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> packets_lost{0};
    std::atomic<double> latency_ms{0.0};
    
    // Start demo thread
    std::thread demo_thread([&]() {
        auto start_time = std::chrono::steady_clock::now();
        auto last_metrics_time = start_time;
        
        while (!g_shutdown.load()) {
            // Simulate frame sending
            frames_sent++;
            bytes_sent += 1024; // Simulate 1KB per frame
            
            // Simulate packet loss
            if (packet_loss > 0 && (rand() % 100) < packet_loss) {
                packets_lost++;
            }
            
            // Simulate latency
            latency_ms = delay_ms + (jitter_ms > 0 ? (rand() % jitter_ms) : 0);
            
            auto now = std::chrono::steady_clock::now();
            
            // Print metrics every 2 seconds
            if (now - last_metrics_time >= 2s) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
                
                uint64_t frames = frames_sent.load();
                uint64_t bytes = bytes_sent.load();
                uint64_t lost = packets_lost.load();
                double latency = latency_ms.load();
                
                double fps = elapsed.count() > 0 ? (frames * 1000.0) / elapsed.count() : 0.0;
                double mbps = elapsed.count() > 0 ? (bytes * 8.0 / 1024 / 1024) / (elapsed.count() / 1000.0) : 0.0;
                double loss_rate = frames > 0 ? (lost * 100.0) / frames : 0.0;
                
                std::cout << "📹 Frames: " << frames 
                          << " | 🎬 FPS: " << std::fixed << std::setprecision(1) << fps
                          << " | 📊 Bitrate: " << std::setprecision(2) << mbps << " Mbps"
                          << " | 💾 Sent: " << format_bytes(bytes)
                          << " | 📉 Loss: " << std::setprecision(1) << loss_rate << "%"
                          << " | ⏱️ Latency: " << std::setprecision(1) << latency << "ms" << std::endl;
                
                last_metrics_time = now;
            }
            
            std::this_thread::sleep_for(40ms); // 25 FPS
        }
    });
    
    // Main render loop
    SDL_Event event;
    int frame_counter = 0;
    
    while (!g_shutdown.load()) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) g_shutdown.store(true);
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) g_shutdown.store(true);
        }
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Draw animated pattern
        int time = frame_counter % 100;
        for (int y = 0; y < SCREEN_HEIGHT; y += 20) {
            for (int x = 0; x < SCREEN_WIDTH; x += 20) {
                int color = ((x + y + time) % 100) * 2;
                SDL_SetRenderDrawColor(renderer, color, color * 2, 255 - color, 255);
                SDL_Rect rect = {x, y, 18, 18};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        
        // Draw metrics text
        SDL_Color textColor = {255, 255, 255, 255};
        std::string metrics_text = "Frames: " + std::to_string(frames_sent.load()) + 
                                 " | Loss: " + std::to_string(packet_loss) + "%" +
                                 " | Latency: " + std::to_string((int)latency_ms.load()) + "ms";
        
        SDL_Surface* textSurface = TTF_RenderText_Solid(font, metrics_text.c_str(), textColor);
        if (textSurface) {
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            if (textTexture) {
                SDL_Rect textRect = {10, 10, textSurface->w, textSurface->h};
                SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
                SDL_DestroyTexture(textTexture);
            }
            SDL_FreeSurface(textSurface);
        }
        
        SDL_RenderPresent(renderer);
        frame_counter++;
        
        std::this_thread::sleep_for(33ms); // ~30 FPS
    }
    
    // Cleanup
    if (demo_thread.joinable()) {
        demo_thread.join();
    }
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    
    std::cout << "🎬 Demo finished." << std::endl;
    return 0;
}
