#include "gui_manager.h"
#include "gui_renderer.h"
#include <algorithm>
#include <cstring>
#include <iostream>

// This is the main implementation file for the GUIManager class.
// It uses the PIMPL idiom to hide implementation details from the header file.

/**
 * @brief PIMPL implementation structure for GUIManager
 * 
 * Contains all private implementation details to minimize header pollution
 */
struct GUIManager::Impl {
    // Core SDL handles
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    
    // GUI system components
    std::unique_ptr<GUIRenderer> gui_renderer;
    
    // Configuration and state
    Config config;
    bool is_initialized = false;
    bool is_open = false;
    bool is_focused = false;
    bool is_minimized = false;
    bool is_graphical_build = true;
    
    // Callback storage
    RedrawCallback redraw_callback;
    ResizeCallback resize_callback;
    
    // Error tracking
    std::string last_error;
    
    // Input handling state
    bool overlay_has_focus = false;
    bool pass_through_enabled = true;
    
    Impl() = default;
    ~Impl() = default;
    
    /**
     * @brief Check if system supports graphical GUI
     */
    bool IsGraphicalBuild() const {
        return is_graphical_build && window != nullptr && renderer != nullptr;
    }
    
    /**
     * @brief Update focus state based on window and overlay state
     */
    void UpdateFocusState() {
        overlay_has_focus = is_open && is_focused && !is_minimized && config.enabled;
    }
    
    /**
     * @brief Log error message
     */
    void LogError(const std::string& error) {
        last_error = error;
        std::cerr << "GUI Manager Error: " << error << std::endl;
    }
    
    /**
     * @brief Validate configuration
     */
    bool ValidateConfig(const Config& cfg) const {
        if (cfg.dpi_scale <= 0.0f || cfg.dpi_scale > 10.0f) {
            return false;
        }
        return true;
    }
};

GUIManager::GUIManager() : pImpl_(std::make_unique<Impl>()) {
}

GUIManager::~GUIManager() {
    if (pImpl_->is_initialized) {
        Shutdown();
    }
}

bool GUIManager::Initialize(SDL_Window* window, SDL_Renderer* renderer, const Config& config) {
    if (pImpl_->is_initialized) {
        pImpl_->LogError("GUI Manager already initialized");
        return false;
    }
    
    if (!window || !renderer) {
        pImpl_->LogError("Invalid SDL window or renderer handle");
        return false;
    }
    
    // Store SDL handles
    pImpl_->window = window;
    pImpl_->renderer = renderer;
    
    // Determine if this is a graphical build
    pImpl_->is_graphical_build = (window != nullptr && renderer != nullptr);
    
    // Validate configuration
    if (!ValidateConfig(config)) {
        pImpl_->LogError("Invalid configuration parameters");
        return false;
    }
    
    pImpl_->config = config;
    pImpl_->pass_through_enabled = config.pass_through_input;
    
    // Initialize internal systems
    if (!InitializeInternal(config)) {
        return false;
    }
    
    pImpl_->is_initialized = true;
    return true;
}

// This is the overloaded Initialize method that takes no config.
// It calls the main Initialize method with a default-constructed Config object.
bool GUIManager::Initialize(SDL_Window* window, SDL_Renderer* renderer)
{
    return Initialize(window, renderer, Config());
}

// This is the internal Initialize method that does the actual work of setting up the GUI.
bool GUIManager::InitializeInternal(const Config& config) {
    // Check if we can run graphical GUI.
    if (!IsGraphicalBuild()) {
        pImpl_->LogError("Graphical GUI not available in ASCII build");
        return false;
    }
    
    // Create and initialize GUI renderer
    pImpl_->gui_renderer = std::make_unique<GUIRenderer>();
    if (!pImpl_->gui_renderer) {
        pImpl_->LogError("Failed to create GUI renderer");
        return false;
    }
    
    // Initialize the renderer with SDL handles and DPI scaling
    if (!pImpl_->gui_renderer->Initialize(pImpl_->window, pImpl_->renderer, config.dpi_scale)) {
        pImpl_->LogError("Failed to initialize GUI renderer");
        return false;
    }
    
    // Apply additional configuration
    if (!config.ini_filename.empty()) {
        pImpl_->gui_renderer->SetIniFilename(config.ini_filename);
    }
    
    pImpl_->UpdateFocusState();
    return true;
}

// Shuts down the GUI manager and cleans up all resources.
void GUIManager::Shutdown() {
    if (!pImpl_->is_initialized) {
        return;
    }
    
    // Close any open GUI
    Close();
    
    // Clean up renderer
    if (pImpl_->gui_renderer) {
        pImpl_->gui_renderer->Shutdown();
        pImpl_->gui_renderer.reset();
    }
    
    // Reset state
    pImpl_->is_initialized = false;
    pImpl_->is_open = false;
    pImpl_->is_focused = false;
    pImpl_->is_minimized = false;
    pImpl_->window = nullptr;
    pImpl_->renderer = nullptr;
}

void GUIManager::Update() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled) {
        return;
    }
    
    // Update focus state
    pImpl_->UpdateFocusState();
    
    // Handle minimize pause
    if (pImpl_->config.minimize_pause && pImpl_->is_minimized) {
        return;  // Skip updates when minimized
    }
    
    // Update GUI renderer
    if (pImpl_->gui_renderer && pImpl_->is_open) {
        pImpl_->gui_renderer->Update();
    }
}

void GUIManager::Render() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled) {
        return;
    }
    
    // Handle minimize pause - don't render when minimized
    if (pImpl_->config.minimize_pause && pImpl_->is_minimized) {
        return;
    }
    
    // Render GUI overlay through the renderer
    if (pImpl_->gui_renderer && pImpl_->is_open) {
        pImpl_->gui_renderer->Render();
    }
}

bool GUIManager::HandleEvent(const SDL_Event& event) {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled) {
        return false;  // No GUI, no event consumption
    }
    
    // Update focus state based on events
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
    
    // Route event to GUI if overlay is active
    if (pImpl_->overlay_has_focus && pImpl_->gui_renderer) {
        bool consumed = pImpl_->gui_renderer->HandleEvent(event);
        
        // If GUI consumed event, always return true.
        // If pass-through is disabled, return true to block event from game.
        return consumed || !pImpl_->pass_through_enabled;
    }
    
    return false;  // Overlay not active, pass to game
}

void GUIManager::Open() {
    if (!pImpl_->is_initialized || !pImpl_->config.enabled) {
        return;
    }
    
    if (pImpl_->is_open) {
        return;  // Already open
    }
    
    pImpl_->is_open = true;
    pImpl_->UpdateFocusState();
    
    // Trigger redraw callback
    if (pImpl_->redraw_callback) {
        pImpl_->redraw_callback();
    }
}

void GUIManager::Close() {
    if (!pImpl_->is_open) {
        return;
    }
    
    pImpl_->is_open = false;
    pImpl_->UpdateFocusState();
    
    // Trigger redraw callback
    if (pImpl_->redraw_callback) {
        pImpl_->redraw_callback();
    }
}

bool GUIManager::IsOpen() const {
    return pImpl_->is_open;
}

bool GUIManager::IsEnabled() const {
    return pImpl_->config.enabled;
}

void GUIManager::SetEnabled(bool enabled) {
    pImpl_->config.enabled = enabled;
    pImpl_->UpdateFocusState();
    
    // Auto-close if disabling
    if (!enabled && pImpl_->is_open) {
        Close();
    }
}

bool GUIManager::IsMinimized() const {
    return pImpl_->is_minimized;
}

void GUIManager::SetFocused(bool focused) {
    pImpl_->is_focused = focused;
    pImpl_->UpdateFocusState();
}

void GUIManager::OnWindowResized(int width, int height) {
    if (!pImpl_->is_initialized || !pImpl_->gui_renderer) {
        return;
    }
    
    // Notify renderer of resize
    pImpl_->gui_renderer->OnWindowResized(width, height);
    
    // Trigger resize callback
    if (pImpl_->resize_callback) {
        pImpl_->resize_callback(width, height);
    }
}

void GUIManager::RegisterRedrawCallback(RedrawCallback callback) {
    pImpl_->redraw_callback = callback;
}

void GUIManager::RegisterResizeCallback(ResizeCallback callback) {
    pImpl_->resize_callback = callback;
}

const std::string& GUIManager::GetLastError() const {
    return pImpl_->last_error;
}

bool GUIManager::ValidateConfig(const Config& config) const {
    // Check if required SDL handles are available for graphical builds
    // In a full implementation, we'd also check for build type
    
    if (config.dpi_scale <= 0.0f || config.dpi_scale > 10.0f) {
        LogError("DPI scale must be between 0.0 and 10.0");
        return false;
    }
    
    return true;
}

void GUIManager::LogError(const std::string& error) const {
    pImpl_->LogError(error);
}

bool GUIManager::IsGraphicalBuild() const {
    return pImpl_->is_graphical_build;
}