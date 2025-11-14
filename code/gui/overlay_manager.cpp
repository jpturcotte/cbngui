#include "overlay_manager.h"
#include "overlay_renderer.h"
#include "overlay_ui.h"
#include "overlay_input_adapter.h"
#include "input_manager.h"
#include "event_bus_adapter.h"
#include "mock_events.h"
#include "InventoryWidget.h"
#include "ui_adaptor.h"
#include "ui_manager.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <optional>

#include "InventoryOverlayState.h"
#include "CharacterOverlayState.h"

struct OverlayManager::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    std::unique_ptr<OverlayRenderer> overlay_renderer;
    std::unique_ptr<OverlayUI> overlay_ui;
    std::unique_ptr<OverlayInputAdapter> input_adapter;
    std::unique_ptr<cataclysm::gui::EventBusAdapter> event_bus_adapter;

    Config config;
    bool is_initialized = false;
    bool is_open = false;
    bool is_focused = false;
    bool is_minimized = false;
    bool is_graphical_build = true;

    RedrawCallback redraw_callback;
    ResizeCallback resize_callback;

    std::string last_error;

    BN::GUI::InputManager* input_manager = nullptr;
    std::unique_ptr<BN::GUI::InputManager> owned_input_manager;
    bool pass_through_enabled = true;
    bool inventory_widget_visible_ = false;
    std::optional<inventory_overlay_state> inventory_state_;
    bool character_widget_visible_ = false;
    std::optional<character_overlay_state> character_state_;
    std::unique_ptr<cataclysm::gui::UiAdaptor> ui_adaptor;
    bool registered_with_ui_manager = false;

    Impl() = default;
    ~Impl() = default;

    bool IsGraphicalBuild() const {
        return is_graphical_build && window != nullptr && renderer != nullptr;
    }

    void UpdateFocusState() {
        const bool overlay_active = is_open && config.enabled;
        if (input_adapter) {
            input_adapter->SetOverlayActive(overlay_active);
            input_adapter->SetFocusEligible(is_focused && !is_minimized && config.enabled);
        }
    }

    void LogError(const std::string& error) {
        last_error = error;
        std::cerr << "Overlay Manager Error: " << error << std::endl;
    }

    bool ValidateConfig(const Config& cfg) const {
        if (cfg.dpi_scale <= 0.0f || cfg.dpi_scale > 10.0f) {
            return false;
        }
        if (!cfg.input_manager) {
            return false;
        }
        return true;
    }

    void NotifyRedraw() {
        cataclysm::gui::UiManager::instance().request_redraw();
        if (redraw_callback) {
            redraw_callback();
        }
    }
};

OverlayManager::OverlayManager() : pImpl_(std::make_unique<Impl>()) {}

OverlayManager::~OverlayManager() {
    if (pImpl_->is_initialized) {
        Shutdown();
    }
}

bool OverlayManager::Initialize(SDL_Window* window, SDL_Renderer* renderer, const Config& config) {
    if (pImpl_->is_initialized) {
        pImpl_->LogError("Overlay Manager already initialized");
        return false;
    }

    if (!window || !renderer) {
        pImpl_->LogError("Invalid SDL window or renderer handle");
        return false;
    }

    pImpl_->window = window;
    pImpl_->renderer = renderer;

    pImpl_->is_graphical_build = (window != nullptr && renderer != nullptr);

    if (!ValidateConfig(config)) {
        pImpl_->LogError("Invalid configuration parameters");
        return false;
    }

    pImpl_->config = config;
    pImpl_->input_manager = config.input_manager;
    pImpl_->pass_through_enabled = config.pass_through_input;

    if (!InitializeInternal(config)) {
        return false;
    }

    pImpl_->is_initialized = true;
    return true;
}

bool OverlayManager::Initialize(SDL_Window* window, SDL_Renderer* renderer) {
    auto default_input_manager = std::make_unique<BN::GUI::InputManager>();
    if (!default_input_manager->Initialize()) {
        pImpl_->LogError("Failed to initialize default InputManager");
        return false;
    }

    Config config;
    config.input_manager = default_input_manager.get();

    const bool initialized = Initialize(window, renderer, config);
    if (!initialized) {
        default_input_manager->Shutdown();
        return false;
    }

    pImpl_->owned_input_manager = std::move(default_input_manager);
    return true;
}

bool OverlayManager::InitializeInternal(const Config& config) {
    if (!IsGraphicalBuild()) {
        pImpl_->LogError("Graphical overlay not available in ASCII build");
        return false;
    }

    pImpl_->overlay_renderer = std::make_unique<OverlayRenderer>();
    if (!pImpl_->overlay_renderer) {
        pImpl_->LogError("Failed to create OverlayRenderer");
        return false;
    }

    if (!pImpl_->overlay_renderer->Initialize(pImpl_->window, pImpl_->renderer, config.dpi_scale)) {
        pImpl_->LogError("Failed to initialize OverlayRenderer");
        return false;
    }

    if (!pImpl_->input_manager) {
        pImpl_->LogError("InputManager is required for overlay input routing");
        return false;
    }

    pImpl_->event_bus_adapter = std::make_unique<cataclysm::gui::EventBusAdapter>(cataclysm::gui::EventBusManager::getGlobalEventBus());
    pImpl_->overlay_ui = std::make_unique<OverlayUI>(*pImpl_->event_bus_adapter, pImpl_->input_manager);
    pImpl_->event_bus_adapter->initialize();

    pImpl_->event_bus_adapter->subscribe<cataclysm::gui::UIButtonClickedEvent>([](const cataclysm::gui::UIButtonClickedEvent& event) {
        std::cout << "Button clicked event received: " << event.button_id << std::endl;
    });

    if (!config.ini_filename.empty()) {
        pImpl_->overlay_renderer->SetIniFilename(config.ini_filename);
    }

    pImpl_->input_adapter = std::make_unique<OverlayInputAdapter>(*pImpl_->overlay_renderer, *pImpl_->overlay_ui, *pImpl_->input_manager, *pImpl_->event_bus_adapter);
    pImpl_->input_adapter->Initialize(config.pass_through_input);
    pImpl_->UpdateFocusState();

    pImpl_->ui_adaptor = std::make_unique<cataclysm::gui::UiAdaptor>();
    pImpl_->ui_adaptor->set_redraw_callback([this]() {
        this->Render();
    });
    pImpl_->ui_adaptor->set_screen_resize_callback([this](int width, int height) {
        this->OnWindowResized(width, height);
    });
    return true;
}

void OverlayManager::Shutdown() {
    if (!pImpl_->is_initialized) {
        return;
    }

    Close();

    if (pImpl_->ui_adaptor && pImpl_->registered_with_ui_manager) {
        cataclysm::gui::UiManager::instance().unregister_adaptor(*pImpl_->ui_adaptor);
        pImpl_->registered_with_ui_manager = false;
    }

    pImpl_->ui_adaptor.reset();

    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->Shutdown();
        pImpl_->input_adapter.reset();
    }

    if (pImpl_->event_bus_adapter) {
        pImpl_->event_bus_adapter->shutdown();
        pImpl_->event_bus_adapter.reset();
    }

    if (pImpl_->overlay_renderer) {
        pImpl_->overlay_renderer->Shutdown();
        pImpl_->overlay_renderer.reset();
    }

    if (pImpl_->owned_input_manager) {
        pImpl_->owned_input_manager->Shutdown();
        pImpl_->owned_input_manager.reset();
    }

    pImpl_->input_manager = nullptr;

    pImpl_->is_initialized = false;
}

void OverlayManager::Render() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled || !pImpl_->is_open) {
        return;
    }

    if (pImpl_->config.minimize_pause && pImpl_->is_minimized) {
        return;
    }

    pImpl_->overlay_renderer->NewFrame();
    pImpl_->overlay_ui->Draw();
    if (pImpl_->inventory_widget_visible_ && pImpl_->inventory_state_) {
        pImpl_->overlay_ui->DrawInventory(*pImpl_->inventory_state_);
    }
    if (pImpl_->character_widget_visible_ && pImpl_->character_state_) {
        pImpl_->overlay_ui->DrawCharacter(*pImpl_->character_state_);
    }
    pImpl_->overlay_renderer->Render();
}

void OverlayManager::UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h) {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled) {
        return;
    }
    if (pImpl_->overlay_ui) {
        pImpl_->overlay_ui->UpdateMapTexture(texture, width, height, tiles_w, tiles_h);
    }
}

void OverlayManager::UpdateInventory(const inventory_overlay_state& state) {
    pImpl_->inventory_state_ = state;
    pImpl_->NotifyRedraw();
}

void OverlayManager::ShowInventory() {
    pImpl_->inventory_widget_visible_ = true;
    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->OnInventoryVisibilityChanged(true);
    }
    pImpl_->NotifyRedraw();
}

void OverlayManager::HideInventory() {
    pImpl_->inventory_widget_visible_ = false;
    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->OnInventoryVisibilityChanged(false);
    }
    pImpl_->NotifyRedraw();
}

bool OverlayManager::IsInventoryVisible() const {
    return pImpl_->inventory_widget_visible_;
}

void OverlayManager::UpdateCharacter(const character_overlay_state& state) {
    pImpl_->character_state_ = state;
    pImpl_->NotifyRedraw();
}

void OverlayManager::ShowCharacter() {
    pImpl_->character_widget_visible_ = true;
    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->OnCharacterVisibilityChanged(true);
    }
    pImpl_->NotifyRedraw();
}

void OverlayManager::HideCharacter() {
    pImpl_->character_widget_visible_ = false;
    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->OnCharacterVisibilityChanged(false);
    }
    pImpl_->NotifyRedraw();
}

bool OverlayManager::IsCharacterVisible() const {
    return pImpl_->character_widget_visible_;
}

bool OverlayManager::HandleEvent(const SDL_Event& event) {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled) {
        return false;
    }

    switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                    pImpl_->is_focused = true;
                    break;
                case SDL_WINDOWEVENT_FOCUS_LOST:
                    pImpl_->is_focused = false;
                    break;
                case SDL_WINDOWEVENT_MINIMIZED:
                    pImpl_->is_minimized = true;
                    if (pImpl_->input_adapter) {
                        pImpl_->input_adapter->OnOverlayMinimized(true);
                    }
                    break;
                case SDL_WINDOWEVENT_RESTORED:
                    pImpl_->is_minimized = false;
                    if (pImpl_->input_adapter) {
                        pImpl_->input_adapter->OnOverlayMinimized(false);
                    }
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    OnWindowResized(event.window.data1, event.window.data2);
                    break;
            }
            pImpl_->UpdateFocusState();
            break;
    }

    if (pImpl_->input_adapter) {
        return pImpl_->input_adapter->HandleEvent(event);
    }

    return false;
}

void OverlayManager::Open() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled || pImpl_->is_open) {
        return;
    }

    pImpl_->is_open = true;
    pImpl_->UpdateFocusState();

    if (pImpl_->ui_adaptor && !pImpl_->registered_with_ui_manager) {
        cataclysm::gui::UiManager::instance().register_adaptor(*pImpl_->ui_adaptor);
        pImpl_->registered_with_ui_manager = true;
    }

    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->OnOverlayOpened();
        pImpl_->input_adapter->SetPassThroughEnabled(pImpl_->pass_through_enabled);
    }

    pImpl_->NotifyRedraw();
}

void OverlayManager::Close() {
    if (!pImpl_->is_open) {
        return;
    }

    pImpl_->is_open = false;

    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->OnOverlayClosed(false);
    }

    pImpl_->UpdateFocusState();

    if (pImpl_->ui_adaptor && pImpl_->registered_with_ui_manager) {
        cataclysm::gui::UiManager::instance().unregister_adaptor(*pImpl_->ui_adaptor);
        pImpl_->registered_with_ui_manager = false;
    }

    pImpl_->NotifyRedraw();
}

bool OverlayManager::IsOpen() const {
    return pImpl_->is_open;
}

bool OverlayManager::IsEnabled() const {
    return pImpl_->config.enabled;
}

void OverlayManager::SetEnabled(bool enabled) {
    if (pImpl_->config.enabled == enabled) {
        return;
    }

    pImpl_->config.enabled = enabled;

    if (!enabled) {
        if (pImpl_->is_open) {
            Close();
        } else {
            pImpl_->UpdateFocusState();
        }
        return;
    }

    pImpl_->UpdateFocusState();

    if (enabled) {
        pImpl_->NotifyRedraw();
    }
}

bool OverlayManager::IsPassThroughInputEnabled() const {
    return pImpl_->pass_through_enabled;
}

void OverlayManager::SetPassThroughInput(bool enabled) {
    if (pImpl_->pass_through_enabled == enabled) {
        return;
    }

    pImpl_->pass_through_enabled = enabled;
    pImpl_->config.pass_through_input = enabled;

    if (pImpl_->input_adapter) {
        pImpl_->input_adapter->SetPassThroughEnabled(enabled);
    }
}

bool OverlayManager::IsMinimized() const {
    return pImpl_->is_minimized;
}

void OverlayManager::SetFocused(bool focused) {
    pImpl_->is_focused = focused;
    pImpl_->UpdateFocusState();
}

void OverlayManager::OnWindowResized(int width, int height) {
    if (!pImpl_->is_initialized || !pImpl_->overlay_renderer) {
        return;
    }

    pImpl_->overlay_renderer->OnWindowResized(width, height);

    if (pImpl_->resize_callback) {
        pImpl_->resize_callback(width, height);
    }

    pImpl_->NotifyRedraw();
}

void OverlayManager::RegisterRedrawCallback(RedrawCallback callback) {
    pImpl_->redraw_callback = callback;
}

void OverlayManager::RegisterResizeCallback(ResizeCallback callback) {
    pImpl_->resize_callback = callback;
}

const std::string& OverlayManager::GetLastError() const {
    return pImpl_->last_error;
}

bool OverlayManager::ValidateConfig(const Config& config) const {
    if (config.dpi_scale <= 0.0f || config.dpi_scale > 10.0f) {
        LogError("DPI scale must be between 0.0 and 10.0");
        return false;
    }
    if (!config.input_manager) {
        LogError("InputManager pointer must be provided");
        return false;
    }
    return true;
}

void OverlayManager::LogError(const std::string& error) const {
    pImpl_->LogError(error);
}

bool OverlayManager::IsGraphicalBuild() const {
    return pImpl_->is_graphical_build;
}

bool OverlayManager::IsRegisteredWithUiManager() const {
    return pImpl_->registered_with_ui_manager;
}
