#include "overlay_manager.h"
#include "overlay_renderer.h"
#include "overlay_ui.h"
#include "event_bus_adapter.h"
#include "overlay_interaction_bridge.h"
#include "mock_events.h"
#include "InventoryWidget.h"
#include "ui_adaptor.h"
#include "ui_manager.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <functional>
#include <optional>
#include <utility>
#include <cassert>

#include "InventoryOverlayState.h"
#include "CharacterOverlayState.h"
#include "debug.h"

struct OverlayManager::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    std::unique_ptr<OverlayRenderer> overlay_renderer;
    std::unique_ptr<OverlayUI> overlay_ui;
    std::unique_ptr<cataclysm::gui::EventBusAdapter> event_bus_adapter;
    std::unique_ptr<cataclysm::gui::OverlayInteractionBridge> interaction_bridge;

    Config config;
    bool is_initialized = false;
    bool is_open = false;
    bool is_focused = false;
    bool is_minimized = false;
    bool is_graphical_build = true;

    RedrawCallback redraw_callback;
    ResizeCallback resize_callback;

    std::string last_error;

    bool overlay_has_focus = false;
    bool pass_through_enabled = true;
    bool inventory_widget_visible_ = false;
    std::optional<inventory_overlay_state> inventory_state_;
    bool character_widget_visible_ = false;
    std::optional<character_overlay_state> character_state_;
    std::unique_ptr<cataclysm::gui::UiAdaptor> ui_adaptor;
    bool registered_with_ui_manager = false;

    std::function<void(const inventory_entry&)> inventory_click_handler = [](const inventory_entry&) {};
    std::function<void(const SDL_KeyboardEvent&)> inventory_key_handler = [](const SDL_KeyboardEvent&) {};
    std::function<void(const std::string&)> character_tab_handler = [](const std::string&) {};
    std::function<void(const std::string&, int)> character_row_handler = [](const std::string&, int) {};
    std::function<void(cataclysm::gui::CharacterCommand)> character_command_handler =
        [](cataclysm::gui::CharacterCommand) {};

    Impl() = default;
    ~Impl() = default;

    bool IsGraphicalBuild() const {
        return is_graphical_build && window != nullptr && renderer != nullptr;
    }

    void UpdateFocusState() {
        overlay_has_focus = is_open && is_focused && !is_minimized && config.enabled;
    }

    void LogError(const std::string& error) {
        last_error = error;
        std::cerr << "Overlay Manager Error: " << error << std::endl;
    }

    bool ValidateConfig(const Config& cfg) const {
        if (cfg.dpi_scale <= 0.0f || cfg.dpi_scale > 10.0f) {
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

    void StartInventoryForwarding() {
        if (!interaction_bridge) {
            return;
        }
        interaction_bridge->set_inventory_click_handler(inventory_click_handler);
        interaction_bridge->set_inventory_key_handler(inventory_key_handler);
        interaction_bridge->enable_inventory_forwarding();
    }

    void StopInventoryForwarding() {
        if (!interaction_bridge) {
            return;
        }
        interaction_bridge->disable_inventory_forwarding();
        interaction_bridge->set_inventory_click_handler(nullptr);
        interaction_bridge->set_inventory_key_handler(nullptr);
    }

    void StartCharacterForwarding() {
        if (!interaction_bridge) {
            return;
        }
        interaction_bridge->set_character_tab_handler(character_tab_handler);
        interaction_bridge->set_character_row_handler(character_row_handler);
        interaction_bridge->set_character_command_handler(character_command_handler);
        interaction_bridge->enable_character_forwarding();
    }

    void StopCharacterForwarding() {
        if (!interaction_bridge) {
            return;
        }
        interaction_bridge->disable_character_forwarding();
        interaction_bridge->set_character_tab_handler(nullptr);
        interaction_bridge->set_character_row_handler(nullptr);
        interaction_bridge->set_character_command_handler(nullptr);
    }

    void SetInventoryClickHandler(std::function<void(const inventory_entry&)> handler) {
        if (handler) {
            inventory_click_handler = std::move(handler);
        } else {
            inventory_click_handler = [](const inventory_entry&) {};
        }

        if (is_open && inventory_widget_visible_) {
            StartInventoryForwarding();
        }
    }

    void SetInventoryKeyHandler(std::function<void(const SDL_KeyboardEvent&)> handler) {
        if (handler) {
            inventory_key_handler = std::move(handler);
        } else {
            inventory_key_handler = [](const SDL_KeyboardEvent&) {};
        }

        if (is_open && inventory_widget_visible_) {
            StartInventoryForwarding();
        }
    }

    void SetCharacterTabHandler(std::function<void(const std::string&)> handler) {
        if (handler) {
            character_tab_handler = std::move(handler);
        } else {
            character_tab_handler = [](const std::string&) {};
        }

        if (is_open && character_widget_visible_) {
            StartCharacterForwarding();
        }
    }

    void SetCharacterRowHandler(std::function<void(const std::string&, int)> handler) {
        if (handler) {
            character_row_handler = std::move(handler);
        } else {
            character_row_handler = [](const std::string&, int) {};
        }

        if (is_open && character_widget_visible_) {
            StartCharacterForwarding();
        }
    }

    void SetCharacterCommandHandler(std::function<void(cataclysm::gui::CharacterCommand)> handler) {
        if (handler) {
            character_command_handler = std::move(handler);
        } else {
            character_command_handler = [](cataclysm::gui::CharacterCommand) {};
        }

        if (is_open && character_widget_visible_) {
            StartCharacterForwarding();
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
    pImpl_->pass_through_enabled = config.pass_through_input;

    if (!InitializeInternal(config)) {
        return false;
    }

    pImpl_->is_initialized = true;
    return true;
}

bool OverlayManager::Initialize(SDL_Window* window, SDL_Renderer* renderer) {
    return Initialize(window, renderer, Config());
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

    pImpl_->event_bus_adapter = std::make_unique<cataclysm::gui::EventBusAdapter>(cataclysm::gui::EventBusManager::getGlobalEventBus());
    pImpl_->overlay_ui = std::make_unique<OverlayUI>(*pImpl_->event_bus_adapter);
    pImpl_->event_bus_adapter->initialize();
    pImpl_->interaction_bridge = std::make_unique<cataclysm::gui::OverlayInteractionBridge>(*pImpl_->event_bus_adapter);

    pImpl_->event_bus_adapter->subscribe<cataclysm::gui::UIButtonClickedEvent>([](const cataclysm::gui::UIButtonClickedEvent& event) {
        std::cout << "Button clicked event received: " << event.button_id << std::endl;
    });

    if (!config.ini_filename.empty()) {
        pImpl_->overlay_renderer->SetIniFilename(config.ini_filename);
    }

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

    if (pImpl_->interaction_bridge) {
        pImpl_->interaction_bridge.reset();
    }

    if (pImpl_->event_bus_adapter) {
        pImpl_->event_bus_adapter->shutdown();
        pImpl_->event_bus_adapter.reset();
    }

    if (pImpl_->overlay_renderer) {
        pImpl_->overlay_renderer->Shutdown();
        pImpl_->overlay_renderer.reset();
    }

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
    if (pImpl_->inventory_widget_visible_) {
        return;
    }

    pImpl_->inventory_widget_visible_ = true;
    if (pImpl_->is_open) {
        pImpl_->StartInventoryForwarding();
    }
    pImpl_->NotifyRedraw();
}

void OverlayManager::HideInventory() {
    if (!pImpl_->inventory_widget_visible_) {
        return;
    }

    pImpl_->inventory_widget_visible_ = false;
    pImpl_->StopInventoryForwarding();
    if (pImpl_->interaction_bridge) {
        const bool active = pImpl_->interaction_bridge->is_inventory_forwarding_active();
        debuglog(DebugLevel::Debug, "Inventory bridge forwarding active after hide? ", active);
        assert(!active && "Inventory forwarding should be disabled when the inventory widget is hidden.");
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
    if (pImpl_->character_widget_visible_) {
        return;
    }

    pImpl_->character_widget_visible_ = true;
    if (pImpl_->is_open) {
        pImpl_->StartCharacterForwarding();
    }
    pImpl_->NotifyRedraw();
}

void OverlayManager::HideCharacter() {
    if (!pImpl_->character_widget_visible_) {
        return;
    }

    pImpl_->character_widget_visible_ = false;
    pImpl_->StopCharacterForwarding();
    if (pImpl_->interaction_bridge) {
        const bool active = pImpl_->interaction_bridge->is_character_forwarding_active();
        debuglog(DebugLevel::Debug, "Character bridge forwarding active after hide? ", active);
        assert(!active && "Character forwarding should be disabled when the character widget is hidden.");
    }
    pImpl_->NotifyRedraw();
}

bool OverlayManager::IsCharacterVisible() const {
    return pImpl_->character_widget_visible_;
}

void OverlayManager::StartInventoryForwarding() {
    if (!pImpl_) {
        return;
    }
    pImpl_->StartInventoryForwarding();
}

void OverlayManager::StopInventoryForwarding() {
    if (!pImpl_) {
        return;
    }
    pImpl_->StopInventoryForwarding();
}

void OverlayManager::StartCharacterForwarding() {
    if (!pImpl_) {
        return;
    }
    pImpl_->StartCharacterForwarding();
}

void OverlayManager::StopCharacterForwarding() {
    if (!pImpl_) {
        return;
    }
    pImpl_->StopCharacterForwarding();
}

void OverlayManager::SetInventoryClickHandler(std::function<void(const inventory_entry&)> handler) {
    if (!pImpl_) {
        return;
    }

    pImpl_->SetInventoryClickHandler(std::move(handler));
}

void OverlayManager::SetInventoryKeyHandler(std::function<void(const SDL_KeyboardEvent&)> handler) {
    if (!pImpl_) {
        return;
    }

    pImpl_->SetInventoryKeyHandler(std::move(handler));
}

void OverlayManager::SetCharacterTabHandler(std::function<void(const std::string&)> handler) {
    if (!pImpl_) {
        return;
    }

    pImpl_->SetCharacterTabHandler(std::move(handler));
}

void OverlayManager::SetCharacterRowHandler(std::function<void(const std::string&, int)> handler) {
    if (!pImpl_) {
        return;
    }

    pImpl_->SetCharacterRowHandler(std::move(handler));
}

void OverlayManager::SetCharacterCommandHandler(
    std::function<void(cataclysm::gui::CharacterCommand)> handler) {
    if (!pImpl_) {
        return;
    }

    pImpl_->SetCharacterCommandHandler(std::move(handler));
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
                    break;
                case SDL_WINDOWEVENT_RESTORED:
                    pImpl_->is_minimized = false;
                    break;
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    OnWindowResized(event.window.data1, event.window.data2);
                    break;
            }
            pImpl_->UpdateFocusState();
            break;
    }

    if (pImpl_->overlay_has_focus && pImpl_->overlay_renderer) {
        const bool renderer_consumed = pImpl_->overlay_renderer->HandleEvent(event);

        bool widget_consumed = false;
        if (pImpl_->overlay_ui) {
            if (pImpl_->inventory_widget_visible_ && pImpl_->inventory_state_) {
                widget_consumed = pImpl_->overlay_ui->GetInventoryWidget().HandleEvent(event) || widget_consumed;
            }
            if (pImpl_->character_widget_visible_ && pImpl_->character_state_) {
                widget_consumed = pImpl_->overlay_ui->GetCharacterWidget().HandleEvent(
                                      event, *pImpl_->character_state_) ||
                                  widget_consumed;
            }
        }

        const bool overlay_consumed = renderer_consumed || widget_consumed;
        if (!pImpl_->pass_through_enabled) {
            // Modal overlays consume all input while focused, even if no widget explicitly handled it.
            return true;
        }

        return overlay_consumed;
    }

    return false;
}

namespace {
constexpr const char* kOverlayLifecycleId = "overlay_ui";
}

void OverlayManager::Open() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled || pImpl_->is_open) {
        return;
    }

    pImpl_->is_open = true;
    pImpl_->UpdateFocusState();

    if (pImpl_->inventory_widget_visible_) {
        pImpl_->StartInventoryForwarding();
    }
    if (pImpl_->character_widget_visible_) {
        pImpl_->StartCharacterForwarding();
    }

    if (pImpl_->ui_adaptor && !pImpl_->registered_with_ui_manager) {
        cataclysm::gui::UiManager::instance().register_adaptor(*pImpl_->ui_adaptor);
        pImpl_->registered_with_ui_manager = true;
    }

    if (pImpl_->event_bus_adapter) {
        const bool is_modal = !pImpl_->pass_through_enabled;
        pImpl_->event_bus_adapter->publishOverlayOpen(kOverlayLifecycleId, is_modal);
    }

    pImpl_->NotifyRedraw();
}

void OverlayManager::Close() {
    if (!pImpl_->is_open) {
        return;
    }

    pImpl_->is_open = false;
    pImpl_->UpdateFocusState();

    pImpl_->StopInventoryForwarding();
    pImpl_->StopCharacterForwarding();

    if (pImpl_->event_bus_adapter) {
        pImpl_->event_bus_adapter->publishOverlayClose(kOverlayLifecycleId, false);
    }

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
    pImpl_->config.enabled = enabled;
    pImpl_->UpdateFocusState();

    if (!enabled && pImpl_->is_open) {
        Close();
        return;
    }

    if (enabled) {
        pImpl_->NotifyRedraw();
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
