#include "gui_renderer.h"
#include <iostream>
#include <cstring>

// This is the main implementation file for the GUIRenderer class.
// It uses the PIMPL idiom to hide implementation details from the header file.
// It also includes the necessary headers for Dear ImGui and the SDL2 backend.

// Use proper ImGui headers
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

/**
 * @brief PIMPL implementation structure for GUIRenderer
 */
struct GUIRenderer::Impl {
    // Core SDL handles
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    
    // Dear ImGui components
    ImGuiContext* context = nullptr;
    ImGuiIO* io = nullptr;
    
    // Configuration
    float dpi_scale = 1.0f;
    bool is_initialized = false;
    bool has_context = false;
    
    // ImGui settings
    std::string ini_filename;
    std::string log_filename;
    bool docking_enabled = false;
    bool viewports_enabled = false;
    
    // Error tracking
    std::string last_error;
    
    Impl() = default;
    ~Impl() = default;
    
    /**
     * @brief Log error message
     */
    void LogError(const std::string& error) {
        last_error = error;
        std::cerr << "GUI Renderer Error: " << error << std::endl;
    }
    
    /**
     * @brief Check SDL version compatibility
     */
    bool CheckSDLVersion() const {
        SDL_version compiled;
        SDL_version linked;
        
        SDL_VERSION(&compiled);
        SDL_GetVersion(&linked);
        
        // Basic version check (in real implementation, more detailed checks)
        if (linked.major < compiled.major ||
            (linked.major == compiled.major && linked.minor < compiled.minor)) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Setup ImGui configuration
     */
    void SetupImGuiConfig() {
        if (!io) return;
        
        // Disable ImGui ini file by default for cleaner behavior
        if (ini_filename.empty()) {
            io->IniFilename = nullptr;
        } else {
            io->IniFilename = ini_filename.c_str();
        }
        
        // Disable log file by default
        if (log_filename.empty()) {
            io->LogFilename = nullptr;
        } else {
            io->LogFilename = log_filename.c_str();
        }
        
        // Configure IO settings
        io->BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io->BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    }
    
    /**
     * @brief Apply DPI scaling settings
     */
    void ApplyDPISettings(float scale) {
        if (!io) return;
        
        dpi_scale = scale;
        
        // Apply global scale to ImGui
        io->FontGlobalScale = dpi_scale;
        
        // Apply scale to SDL renderer
        if (renderer && scale > 0.0f) {
            SDL_RenderSetScale(renderer, scale, scale);
        }
    }
};

GUIRenderer::GUIRenderer() : pImpl_(std::make_unique<Impl>()) {
}

GUIRenderer::~GUIRenderer() {
    if (pImpl_->is_initialized) {
        Shutdown();
    }
}

bool GUIRenderer::Initialize(SDL_Window* window, SDL_Renderer* renderer, float dpi_scale) {
    if (pImpl_->is_initialized) {
        pImpl_->LogError("GUI Renderer already initialized");
        return false;
    }
    
    if (!window || !renderer) {
        pImpl_->LogError("Invalid SDL window or renderer handle");
        return false;
    }
    
    // Check SDL version compatibility
    if (!pImpl_->CheckSDLVersion()) {
        pImpl_->LogError("Incompatible SDL version");
        return false;
    }
    
    // Store handles
    pImpl_->window = window;
    pImpl_->renderer = renderer;
    
    // Create ImGui context
    IMGUI_CHECKVERSION();
    pImpl_->context = ImGui::CreateContext();
    
    if (!pImpl_->context) {
        pImpl_->LogError("Failed to create ImGui context");
        return false;
    }
    
    pImpl_->has_context = true;
    pImpl_->io = &ImGui::GetIO();
    
    if (!pImpl_->io) {
        pImpl_->LogError("Failed to get ImGui IO");
        return false;
    }
    
    // Setup ImGui configuration
    pImpl_->SetupImGuiConfig();
    
    // Initialize SDL2 backend for ImGui
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
        pImpl_->LogError("Failed to initialize ImGui SDL2 backend");
        return false;
    }
    
    // Initialize SDL Renderer backend for ImGui
    if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
        pImpl_->LogError("Failed to initialize ImGui SDL Renderer backend");
        ImGui_ImplSDL2_Shutdown();
        return false;
    }
    
    // Apply DPI scaling
    pImpl_->ApplyDPISettings(dpi_scale);
    
    pImpl_->is_initialized = true;
    return true;
}

// Shuts down the GUI renderer and cleans up all resources.
void GUIRenderer::Shutdown() {
    if (!pImpl_->is_initialized) {
        return;
    }
    
    // Shutdown backends
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    
    // Destroy ImGui context
    if (pImpl_->has_context && pImpl_->context) {
        ImGui::DestroyContext(pImpl_->context);
        pImpl_->context = nullptr;
    }
    
    pImpl_->has_context = false;
    pImpl_->io = nullptr;
    pImpl_->is_initialized = false;
    pImpl_->window = nullptr;
    pImpl_->renderer = nullptr;
}

void GUIRenderer::Update() {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return;
    }
    
    // Start new ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void GUIRenderer::Render() {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return;
    }
    
    // Render ImGui draw data
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data) {
        ImGui_ImplSDLRenderer2_RenderDrawData(draw_data, pImpl_->renderer);
    }
}

bool GUIRenderer::HandleEvent(const SDL_Event& event) {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return false;
    }
    
    // Process event through ImGui
    ImGui_ImplSDL2_ProcessEvent(&event);

    // Check if ImGui wants to capture input
    return pImpl_->io && (pImpl_->io->WantCaptureMouse || pImpl_->io->WantCaptureKeyboard);
}

void GUIRenderer::OnWindowResized(int width, int height) {
    if (!pImpl_->is_initialized || !pImpl_->io) {
        return;
    }
    
    // Update display size in ImGui IO
    pImpl_->io->DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
}

void GUIRenderer::SetIniFilename(const std::string& filename) {
    pImpl_->ini_filename = filename;
    
    if (pImpl_->io) {
        if (pImpl_->ini_filename.empty()) {
            pImpl_->io->IniFilename = nullptr;
        } else {
            pImpl_->io->IniFilename = pImpl_->ini_filename.c_str();
        }
    }
}

void GUIRenderer::SetLogFilename(const std::string& filename) {
    pImpl_->log_filename = filename;
    
    if (pImpl_->io) {
        if (pImpl_->log_filename.empty()) {
            pImpl_->io->LogFilename = nullptr;
        } else {
            pImpl_->io->LogFilename = pImpl_->log_filename.c_str();
        }
    }
}

void GUIRenderer::SetDockingEnabled(bool enabled) {
    pImpl_->docking_enabled = enabled;
    
    if (pImpl_->io) {
        if (enabled) {
            pImpl_->io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        } else {
            pImpl_->io->ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
        }
    }
}

void GUIRenderer::SetViewportsEnabled(bool enabled) {
    pImpl_->viewports_enabled = enabled;
    
    if (pImpl_->io) {
        if (enabled) {
            pImpl_->io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        } else {
            pImpl_->io->ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        }
    }
}

ImGuiContext* GUIRenderer::GetContext() const {
    return pImpl_->context;
}

ImGuiIO* GUIRenderer::GetIO() const {
    return pImpl_->io;
}

bool GUIRenderer::IsInitialized() const {
    return pImpl_->is_initialized;
}

bool GUIRenderer::HasContext() const {
    return pImpl_->has_context;
}

void GUIRenderer::RebuildFontAtlas() {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return;
    }
    
    // Rebuild font atlas
    if (pImpl_->io && pImpl_->io->Fonts) {
        pImpl_->io->Fonts->Clear();
        pImpl_->io->Fonts->AddFontDefault();
        pImpl_->io->Fonts->Build();
    }
}