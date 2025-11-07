#include "overlay_renderer.h"
#include <iostream>
#include <cstring>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

struct OverlayRenderer::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    
    ImGuiContext* context = nullptr;
    ImGuiIO* io = nullptr;
    
    float dpi_scale = 1.0f;
    bool is_initialized = false;
    bool has_context = false;
    
    std::string ini_filename;
    std::string log_filename;
    bool docking_enabled = false;
    bool viewports_enabled = false;
    
    std::string last_error;
    
    Impl() = default;
    ~Impl() = default;
    
    void LogError(const std::string& error) {
        last_error = error;
        std::cerr << "Overlay Renderer Error: " << error << std::endl;
    }
    
    bool CheckSDLVersion() const {
        SDL_version compiled;
        SDL_version linked;
        
        SDL_VERSION(&compiled);
        SDL_GetVersion(&linked);
        
        if (linked.major < compiled.major ||
            (linked.major == compiled.major && linked.minor < compiled.minor)) {
            return false;
        }
        
        return true;
    }
    
    void SetupImGuiConfig() {
        if (!io) return;
        
        if (ini_filename.empty()) {
            io->IniFilename = nullptr;
        } else {
            io->IniFilename = ini_filename.c_str();
        }
        
        if (log_filename.empty()) {
            io->LogFilename = nullptr;
        } else {
            io->LogFilename = log_filename.c_str();
        }
        
        io->BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io->BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
    }
    
    void ApplyDPISettings(float scale) {
        if (!io) return;
        
        dpi_scale = scale;
        io->FontGlobalScale = dpi_scale;
        
        if (renderer && scale > 0.0f) {
            SDL_RenderSetScale(renderer, scale, scale);
        }
    }
};

OverlayRenderer::OverlayRenderer() : pImpl_(std::make_unique<Impl>()) {}

OverlayRenderer::~OverlayRenderer() {
    if (pImpl_->is_initialized) {
        Shutdown();
    }
}

bool OverlayRenderer::Initialize(SDL_Window* window, SDL_Renderer* renderer, float dpi_scale) {
    if (pImpl_->is_initialized) {
        pImpl_->LogError("Overlay Renderer already initialized");
        return false;
    }
    
    if (!window || !renderer) {
        pImpl_->LogError("Invalid SDL window or renderer handle");
        return false;
    }
    
    if (!pImpl_->CheckSDLVersion()) {
        pImpl_->LogError("Incompatible SDL version");
        return false;
    }
    
    pImpl_->window = window;
    pImpl_->renderer = renderer;
    
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
    
    pImpl_->SetupImGuiConfig();
    
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer)) {
        pImpl_->LogError("Failed to initialize ImGui SDL2 backend");
        return false;
    }
    
    if (!ImGui_ImplSDLRenderer2_Init(renderer)) {
        pImpl_->LogError("Failed to initialize ImGui SDL Renderer backend");
        ImGui_ImplSDL2_Shutdown();
        return false;
    }
    
    pImpl_->ApplyDPISettings(dpi_scale);
    
    pImpl_->is_initialized = true;
    return true;
}

void OverlayRenderer::Shutdown() {
    if (!pImpl_->is_initialized) {
        return;
    }
    
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    
    if (pImpl_->has_context && pImpl_->context) {
        ImGui::DestroyContext(pImpl_->context);
        pImpl_->context = nullptr;
    }
    
    pImpl_->has_context = false;
    pImpl_->io = nullptr;
    pImpl_->is_initialized = false;
}

void OverlayRenderer::NewFrame() {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return;
    }
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void OverlayRenderer::Render() {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return;
    }
    
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data) {
        ImGui_ImplSDLRenderer2_RenderDrawData(draw_data, pImpl_->renderer);
    }
}

bool OverlayRenderer::HandleEvent(const SDL_Event& event) {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return false;
    }
    
    ImGui_ImplSDL2_ProcessEvent(&event);

    return pImpl_->io && (pImpl_->io->WantCaptureMouse || pImpl_->io->WantCaptureKeyboard);
}

void OverlayRenderer::OnWindowResized(int width, int height) {
    if (!pImpl_->is_initialized || !pImpl_->io) {
        return;
    }
    
    pImpl_->io->DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
}

void OverlayRenderer::SetIniFilename(const std::string& filename) {
    pImpl_->ini_filename = filename;
    
    if (pImpl_->io) {
        if (pImpl_->ini_filename.empty()) {
            pImpl_->io->IniFilename = nullptr;
        } else {
            pImpl_->io->IniFilename = pImpl_->ini_filename.c_str();
        }
    }
}

void OverlayRenderer::SetLogFilename(const std::string& filename) {
    pImpl_->log_filename = filename;
    
    if (pImpl_->io) {
        if (pImpl_->log_filename.empty()) {
            pImpl_->io->LogFilename = nullptr;
        } else {
            pImpl_->io->LogFilename = pImpl_->log_filename.c_str();
        }
    }
}

void OverlayRenderer::SetDockingEnabled(bool enabled) {
    pImpl_->docking_enabled = enabled;
    
    if (pImpl_->io) {
        if (enabled) {
            pImpl_->io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        } else {
            pImpl_->io->ConfigFlags &= ~ImGuiConfigFlags_DockingEnable;
        }
    }
}

void OverlayRenderer::SetViewportsEnabled(bool enabled) {
    pImpl_->viewports_enabled = enabled;
    
    if (pImpl_->io) {
        if (enabled) {
            pImpl_->io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        } else {
            pImpl_->io->ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        }
    }
}

ImGuiContext* OverlayRenderer::GetContext() const {
    return pImpl_->context;
}

ImGuiIO* OverlayRenderer::GetIO() const {
    return pImpl_->io;
}

bool OverlayRenderer::IsInitialized() const {
    return pImpl_->is_initialized;
}

bool OverlayRenderer::HasContext() const {
    return pImpl_->has_context;
}

void OverlayRenderer::RebuildFontAtlas() {
    if (!pImpl_->is_initialized || !pImpl_->has_context) {
        return;
    }
    
    if (pImpl_->io && pImpl_->io->Fonts) {
        pImpl_->io->Fonts->Clear();
        pImpl_->io->Fonts->AddFontDefault();
        pImpl_->io->Fonts->Build();
    }
}
