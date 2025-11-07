#include "overlay_manager.h"
#include "overlay_renderer.h"
#include "overlay_ui.h"
#include "event_bus_adapter.h"
#include "mock_events.h"
#include <algorithm>
#include <cstring>
#include <iostream>

struct OverlayManager::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    std::unique_ptr<OverlayRenderer> overlay_renderer;
    std::unique_ptr<OverlayUI> overlay_ui;
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

    bool overlay_has_focus = false;
    bool pass_through_enabled = true;

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

    pImpl_->event_bus_adapter->subscribe<cataclysm::gui::UIButtonClickedEvent>([](const cataclysm::gui::UIButtonClickedEvent& event) {
        std::cout << "Button clicked event received: " << event.button_id << std::endl;
    });

    if (!config.ini_filename.empty()) {
        pImpl_->overlay_renderer->SetIniFilename(config.ini_filename);
    }

    pImpl_->UpdateFocusState();
    return true;
}

void OverlayManager::Shutdown() {
    if (!pImpl_->is_initialized) {
        return;
    }

    Close();

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
    pImpl_->overlay_renderer->Render();
}

void OverlayManager::UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h) {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled || !pImpl_->is_open) {
        return;
    }
    if (pImpl_->overlay_ui) {
        pImpl_->overlay_ui->UpdateMapTexture(texture, width, height, tiles_w, tiles_h);
    }
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
            }
            pImpl_->UpdateFocusState();
            break;
    }

    if (pImpl_->overlay_has_focus && pImpl_->overlay_renderer) {
        bool consumed = pImpl_->overlay_renderer->HandleEvent(event);
        return consumed || !pImpl_->pass_through_enabled;
    }

    return false;
}

void OverlayManager::Open() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled || pImpl_->is_open) {
        return;
    }

    pImpl_->is_open = true;
    pImpl_->UpdateFocusState();

    if (pImpl_->redraw_callback) {
        pImpl_->redraw_callback();
    }
}

void OverlayManager::Close() {
    if (!pImpl_->is_open) {
        return;
    }

    pImpl_->is_open = false;
    pImpl_->UpdateFocusState();

    if (pImpl_->redraw_callback) {
        pImpl_->redraw_callback();
    }
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
