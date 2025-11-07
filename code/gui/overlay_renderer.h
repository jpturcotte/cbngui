#ifndef OVERLAY_RENDERER_H
#define OVERLAY_RENDERER_H

#include <SDL.h>
#include <string>
#include <memory>
#include <functional>

struct ImGuiContext;
struct ImGuiIO;

class OverlayRenderer {
public:
    bool Initialize(SDL_Window* window, SDL_Renderer* renderer, float dpi_scale = 1.0f);

    void Shutdown();

    void NewFrame();

    void Render();

    bool HandleEvent(const SDL_Event& event);

    void OnWindowResized(int width, int height);

    void SetIniFilename(const std::string& filename);

    void SetLogFilename(const std::string& filename);

    void SetDockingEnabled(bool enabled);

    void SetViewportsEnabled(bool enabled);

    ImGuiContext* GetContext() const;

    ImGuiIO* GetIO() const;

    bool IsInitialized() const;

    bool HasContext() const;

    void RebuildFontAtlas();

    OverlayRenderer();
    ~OverlayRenderer();
    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;
    OverlayRenderer(OverlayRenderer&&) = delete;
    OverlayRenderer& operator=(OverlayRenderer&&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;

    bool CheckSDLVersion() const;

    void SetupImGuiConfig();

    void ApplyDPISettings(float dpi_scale);

    void LogError(const std::string& error);
};

#endif // OVERLAY_RENDERER_H
