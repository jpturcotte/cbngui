#ifndef OVERLAY_MANAGER_H
#define OVERLAY_MANAGER_H

#include <SDL.h>
#include <functional>
#include <memory>
#include <string>

namespace BN {
namespace GUI {
class InputManager;
} // namespace GUI
} // namespace BN

class OverlayManager {
public:
    struct Impl;

    struct Config {
        bool enabled = true;
        bool pass_through_input = true;
        float dpi_scale = 1.0f;
        bool minimize_pause = true;
        std::string ini_filename;
        BN::GUI::InputManager* input_manager = nullptr;

        Config() = default;
    };

    bool Initialize(SDL_Window* window, SDL_Renderer* renderer, const Config& config);
    bool Initialize(SDL_Window* window, SDL_Renderer* renderer);

    void Shutdown();

    void Render();

    void UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h);
    void UpdateInventory(const struct inventory_overlay_state& state);
    void ShowInventory();
    void HideInventory();
    bool IsInventoryVisible() const;

    void UpdateCharacter(const struct character_overlay_state& state);
    void ShowCharacter();
    void HideCharacter();
    bool IsCharacterVisible() const;

    bool HandleEvent(const SDL_Event& event);

    void Open();

    void Close();

    bool IsOpen() const;

    bool IsEnabled() const;

    void SetEnabled(bool enabled);

    bool IsPassThroughInputEnabled() const;

    void SetPassThroughInput(bool enabled);

    bool IsMinimized() const;

    void SetFocused(bool focused);

    void OnWindowResized(int width, int height);

    using RedrawCallback = std::function<void()>;
    using ResizeCallback = std::function<void(int, int)>;

    void RegisterRedrawCallback(RedrawCallback callback);

    void RegisterResizeCallback(ResizeCallback callback);

    const std::string& GetLastError() const;

    OverlayManager();
    ~OverlayManager();
    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;
    OverlayManager(OverlayManager&&) = delete;
    OverlayManager& operator=(OverlayManager&&) = delete;

private:
    std::unique_ptr<Impl> pImpl_;

    bool InitializeInternal(const Config& config);

    bool ValidateConfig(const Config& config) const;

    void LogError(const std::string& error) const;

    bool IsGraphicalBuild() const;

    bool IsRegisteredWithUiManager() const;
};

#endif // OVERLAY_MANAGER_H
