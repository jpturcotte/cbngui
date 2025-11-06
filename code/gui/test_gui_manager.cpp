#include "gui_manager.h"
#include "gui_renderer.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

/**
 * @brief Example demonstrating basic usage of the GUI Manager
 * 
 * This file shows how to:
 * 1. Initialize the GUI manager with SDL2
 * 2. Handle events and rendering
 * 3. Basic overlay operations
 * 
 * Compile with: g++ -std=c++17 test_gui_manager.cpp gui_manager.cpp gui_renderer.cpp -lSDL2 -o test_gui
 */
class GUIExample {
public:
    GUIExample() : gui_manager_(), is_running_(true) {}
    
    bool Initialize() {
        // Initialize SDL2
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Create window
        window_ = SDL_CreateWindow(
            "Cataclysm-BN GUI Manager Test",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1024, 768,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
        
        if (!window_) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Create renderer
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer_) {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window_);
            return false;
        }
        
        // Configure GUI Manager
        GUIManager::Config config;
        config.enabled = true;
        config.pass_through_input = true;
        config.dpi_scale = 1.0f;
        config.minimize_pause = true;
        config.ini_filename = "imgui_test.ini";
        
        // Initialize GUI Manager
        if (!gui_manager_.Initialize(window_, renderer_, config)) {
            std::cerr << "Failed to initialize GUI Manager: " << gui_manager_.GetLastError() << std::endl;
            SDL_DestroyRenderer(renderer_);
            SDL_DestroyWindow(window_);
            return false;
        }
        
        // Register callbacks
        gui_manager_.RegisterRedrawCallback([this]() {
            std::cout << "GUI redraw requested" << std::endl;
        });
        
        gui_manager_.RegisterResizeCallback([this](int width, int height) {
            std::cout << "Window resized to: " << width << "x" << height << std::endl;
        });
        
        // Open the GUI overlay
        gui_manager_.Open();
        
        return true;
    }
    
    void Run() {
        std::cout << "Starting GUI Manager test application..." << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  O - Open GUI overlay" << std::endl;
        std::cout << "  C - Close GUI overlay" << std::endl;
        std::cout << "  E - Toggle enabled/disabled" << std::endl;
        std::cout << "  Q - Quit application" << std::endl;
        std::cout << "Press keys to test functionality..." << std::endl;
        
        SDL_Event event;
        auto last_time = std::chrono::high_resolution_clock::now();
        const int target_fps = 60;
        const auto target_frame_time = std::chrono::milliseconds(1000 / target_fps);
        
        while (is_running_) {
            // Handle events
            while (SDL_PollEvent(&event)) {
                HandleEvent(event);
            }
            
            // Update timing
            auto current_time = std::chrono::high_resolution_clock::now();
            auto frame_time = current_time - last_time;
            
            // Check if we should limit FPS
            if (frame_time < target_frame_time) {
                std::this_thread::sleep_for(target_frame_time - frame_time);
            }
            
            last_time = std::chrono::high_resolution_clock::now();
            
            // Update GUI Manager
            gui_manager_.Update();
            
            // Clear renderer
            SDL_SetRenderDrawColor(renderer_, 32, 32, 32, 255);
            SDL_RenderClear(renderer_);
            
            // Simulate game rendering (in real game, this would be the actual game rendering)
            RenderGameScene();
            
            // Render GUI overlay
            gui_manager_.Render();
            
            // Present the frame
            SDL_RenderPresent(renderer_);
        }
    }
    
    void Shutdown() {
        std::cout << "Shutting down GUI Manager..." << std::endl;
        
        gui_manager_.Shutdown();
        
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        
        SDL_Quit();
    }

private:
    void HandleEvent(const SDL_Event& event) {
        switch (event.type) {
            case SDL_QUIT:
                is_running_ = false;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_o:
                        gui_manager_.Open();
                        break;
                    case SDLK_c:
                        gui_manager_.Close();
                        break;
                    case SDLK_e:
                        gui_manager_.SetEnabled(!gui_manager_.IsEnabled());
                        break;
                    case SDLK_q:
                        is_running_ = false;
                        break;
                    default:
                        break;
                }
                break;
                
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    gui_manager_.OnWindowResized(event.window.data1, event.window.data2);
                }
                break;
        }
        
        // Always let GUI Manager handle events for input routing
        gui_manager_.HandleEvent(event);
    }
    
    void RenderGameScene() {
        // In a real game, this would render the actual game scene
        // For this example, we'll just draw a simple gradient
        
        // Draw a simple background gradient
        SDL_Rect full_screen = {0, 0, 1024, 768};
        SDL_SetRenderDrawColor(renderer_, 64, 64, 128, 255);
        SDL_RenderFillRect(renderer_, &full_screen);
        
        // Draw some basic UI text (in real implementation, this would use the game's text system)
        // For this example, we'll just set the draw color
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    }
    
    std::unique_ptr<GUIManager> gui_manager_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool is_running_;
};

int main(int argc, char* argv[]) {
    GUIExample example;
    
    if (!example.Initialize()) {
        std::cerr << "Failed to initialize example application" << std::endl;
        return 1;
    }
    
    example.Run();
    example.Shutdown();
    
    return 0;
}