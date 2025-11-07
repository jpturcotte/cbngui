#include "overlay_manager.h"
#include "imgui.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

class GUIExample {
public:
    GUIExample() : overlay_manager_(std::make_unique<OverlayManager>()), is_running_(true) {}

    bool Initialize() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::cerr << "Failed to initialize SDL2: " << SDL_GetError() << std::endl;
            return false;
        }

        window_ = SDL_CreateWindow(
            "Cataclysm-BN Overlay Manager Test",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1024, 768,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );

        if (!window_) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            return false;
        }

        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer_) {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window_);
            return false;
        }

        OverlayManager::Config config;
        config.enabled = true;
        config.pass_through_input = true;
        config.dpi_scale = 1.0f;
        config.minimize_pause = true;
        config.ini_filename = "imgui_test.ini";

        if (!overlay_manager_->Initialize(window_, renderer_, config)) {
            std::cerr << "Failed to initialize Overlay Manager: " << overlay_manager_->GetLastError() << std::endl;
            SDL_DestroyRenderer(renderer_);
            SDL_DestroyWindow(window_);
            return false;
        }

        overlay_manager_->RegisterRedrawCallback([this]() {
            std::cout << "Overlay redraw requested" << std::endl;
        });

        overlay_manager_->RegisterResizeCallback([this](int width, int height) {
            std::cout << "Window resized to: " << width << "x" << height << std::endl;
        });

        overlay_manager_->Open();

        return true;
    }

    void Run() {
        SDL_Event event;
        while (is_running_) {
            while (SDL_PollEvent(&event)) {
                HandleEvent(event);
            }

            SDL_SetRenderDrawColor(renderer_, 32, 32, 32, 255);
            SDL_RenderClear(renderer_);

            RenderGameScene();

            overlay_manager_->Render();

            SDL_RenderPresent(renderer_);
        }
    }

    void Shutdown() {
        overlay_manager_->Shutdown();

        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
        }

        if (window_) {
            SDL_DestroyWindow(window_);
        }

        SDL_Quit();
    }

private:
    void HandleEvent(const SDL_Event& event) {
        overlay_manager_->HandleEvent(event);

        switch (event.type) {
            case SDL_QUIT:
                is_running_ = false;
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_o:
                        overlay_manager_->Open();
                        break;
                    case SDLK_c:
                        overlay_manager_->Close();
                        break;
                    case SDLK_e:
                        overlay_manager_->SetEnabled(!overlay_manager_->IsEnabled());
                        break;
                    case SDLK_q:
                        is_running_ = false;
                        break;
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    overlay_manager_->OnWindowResized(event.window.data1, event.window.data2);
                }
                break;
        }
    }

    void RenderGameScene() {
        SDL_Rect full_screen = {0, 0, 1024, 768};
        SDL_SetRenderDrawColor(renderer_, 64, 64, 128, 255);
        SDL_RenderFillRect(renderer_, &full_screen);
    }

    std::unique_ptr<OverlayManager> overlay_manager_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool is_running_;
};

int main(int argc, char* argv[]) {
    GUIExample example;

    if (!example.Initialize()) {
        return 1;
    }

    example.Run();
    example.Shutdown();

    return 0;
}
