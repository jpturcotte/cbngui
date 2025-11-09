#include "overlay_manager.h"
#include "event_bus.h"
#include "events.h"
#include "imgui.h"
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

SDL_Texture* create_checkerboard_texture(SDL_Renderer* renderer, int width, int height, int tile_size) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!texture) {
        std::cerr << "Failed to create checkerboard texture: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int y = 0; y < height; y += tile_size) {
        for (int x = 0; x < width; x += tile_size) {
            if ((x / tile_size + y / tile_size) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255);
            }
            SDL_Rect rect = {x, y, tile_size, tile_size};
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);
    return texture;
}

class GUIExample {
public:
    GUIExample() : overlay_manager_(std::make_unique<OverlayManager>()), is_running_(true) {}

    inventory_overlay_state mock_inventory_state_;
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

        map_texture_ = create_checkerboard_texture(renderer_, 480, 480, 16);
        if (!map_texture_) {
            return false;
        }

        auto& event_bus = cataclysm::gui::EventBusManager::getGlobalEventBus();
        subscriptions_.push_back(event_bus.subscribe<cataclysm::gui::MapTileHoveredEvent>([](const cataclysm::gui::MapTileHoveredEvent& event) {
            std::cout << "Map tile hovered at (" << event.getX() << ", " << event.getY() << ")" << std::endl;
        }));
        subscriptions_.push_back(event_bus.subscribe<cataclysm::gui::MapTileClickedEvent>([](const cataclysm::gui::MapTileClickedEvent& event) {
            std::cout << "Map tile clicked at (" << event.getX() << ", " << event.getY() << ")" << std::endl;
        }));

        subscriptions_.push_back(event_bus.subscribe<cataclysm::gui::InventoryItemClickedEvent>([](const cataclysm::gui::InventoryItemClickedEvent& event) {
            std::cout << "Inventory item clicked: " << event.getEntry().label << std::endl;
        }));

        mock_inventory_state_.title = "Inventory";
        mock_inventory_state_.hotkey_hint = "[i] to close";
        mock_inventory_state_.weight_label = "Weight: 10/100";
        mock_inventory_state_.volume_label = "Volume: 10/100";
        mock_inventory_state_.filter_string = "Filter: none";
        mock_inventory_state_.navigation_mode = "Item mode";
        mock_inventory_state_.active_column = 0;

        mock_inventory_state_.columns[0].name = "Worn";
        mock_inventory_state_.columns[0].scroll_position = 0;
        mock_inventory_state_.columns[0].entries = {
            {"Clothing", "", true, false, true, false, false, ""},
            {"Backpack", "a", false, true, true, true, false, ""},
            {"Jeans", "b", false, false, true, false, false, ""},
        };

        mock_inventory_state_.columns[1].name = "Inventory";
        mock_inventory_state_.columns[1].scroll_position = 0;
        mock_inventory_state_.columns[1].entries = {
            {"Food", "", true, false, false, false, false, ""},
            {"Water", "c", false, false, false, true, false, ""},
            {"Can of Beans", "d", false, false, false, false, false, ""},
            {"First Aid", "", true, false, false, false, false, ""},
            {"Bandage", "e", false, false, false, false, false, ""},
            {"Aspirin", "f", false, false, false, false, true, "Too weak"},
        };

        mock_inventory_state_.columns[2].name = "Ground";
        mock_inventory_state_.columns[2].scroll_position = 0;
        mock_inventory_state_.columns[2].entries = {
            {"Rocks", "g", false, false, false, false, false, ""},
        };

        return true;
    }

    void Run() {
        SDL_Event event;
        while (is_running_) {
            while (SDL_PollEvent(&event)) {
                HandleEvent(event);
            }

            overlay_manager_->UpdateMapTexture(map_texture_, 480, 480, 30, 30);
            overlay_manager_->UpdateInventory(mock_inventory_state_);

            SDL_SetRenderDrawColor(renderer_, 32, 32, 32, 255);
            SDL_RenderClear(renderer_);

            RenderGameScene();

            overlay_manager_->Render();

            SDL_RenderPresent(renderer_);
        }
    }

    void Shutdown() {
        for (auto& sub : subscriptions_) {
            sub->unsubscribe();
        }
        subscriptions_.clear();
        if (map_texture_) {
            SDL_DestroyTexture(map_texture_);
        }
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
                    case SDLK_i:
                        overlay_manager_->ShowInventory();
                        break;
                    case SDLK_j:
                        overlay_manager_->HideInventory();
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
    SDL_Texture* map_texture_ = nullptr;
    bool is_running_;
    std::vector<std::shared_ptr<cataclysm::gui::EventSubscription>> subscriptions_;
};

int main(int argc, char* argv[]) {
    cataclysm::gui::EventBusManager::initialize();

    int exit_code = 0;
    {
        GUIExample example;

        if (!example.Initialize()) {
            example.Shutdown();
            exit_code = 1;
        } else {
            example.Run();
            example.Shutdown();
        }
    }

    cataclysm::gui::EventBusManager::shutdown();
    return exit_code;
}
