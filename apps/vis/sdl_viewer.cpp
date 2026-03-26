#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

// Define SDL_MAIN_HANDLED to prevent SDL from redefining main()
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

import video_streaming.receiver;
import video_streaming.media.frame;

using namespace video_streaming;

int main(int argc, char* argv[]) {
    int port = 5000;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    } else {
        std::cout << "Usage: sdl_viewer [port]\n";
        std::cout << "Using default port " << port << "\n";
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "❌ SDL Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Create Window & Renderer
    SDL_Window* window = SDL_CreateWindow(
        "Video Stream Viewer (Core + SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, // Initial size, will resize on first frame
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "❌ SDL Window failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = nullptr;
    int tex_width = 0;
    int tex_height = 0;

    // Initialize Receiver
    VideoReceiver::Config config;
    config.port = static_cast<uint16_t>(port);
    VideoReceiver receiver(config);

    // Mutex for thread-safe texture updates
    std::mutex render_mutex;
    
    // Set callback to handle incoming frames
    receiver.set_frame_callback([&](const Frame& frame) {
        std::lock_guard<std::mutex> lock(render_mutex);
        
        // Recreate texture if dimensions change
        if (!texture || tex_width != frame.width || tex_height != frame.height) {
            if (texture) SDL_DestroyTexture(texture);
            
            tex_width = frame.width;
            tex_height = frame.height;
            
            texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_YV12, // YUV 4:2:0 planar
                SDL_TEXTUREACCESS_STREAMING,
                tex_width, tex_height
            );
            
            SDL_SetWindowSize(window, tex_width, tex_height);
        }

        if (texture) {
            // Calculate plane sizes
            size_t y_size = tex_width * tex_height;
            size_t uv_size = y_size / 4;

            // Update texture
            // Frame.data is YUV420P (Y plane, then U plane, then V plane)
            // SDL_UpdateYUVTexture expects pointers to start of each plane
            const uint8_t* y_plane = frame.data.data();
            const uint8_t* u_plane = y_plane + y_size;
            const uint8_t* v_plane = u_plane + uv_size;

            SDL_UpdateYUVTexture(
                texture, nullptr,
                y_plane, tex_width,       // Y
                u_plane, tex_width / 2,   // U
                v_plane, tex_width / 2    // V
            );

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }
    });

    if (!receiver.start()) {
        std::cerr << "❌ Failed to start receiver" << std::endl;
        return -1;
    }

    // Main Loop
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        SDL_Delay(10);
    }

    receiver.stop();
    if (texture) SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}