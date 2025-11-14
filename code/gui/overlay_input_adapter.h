#ifndef OVERLAY_INPUT_ADAPTER_H
#define OVERLAY_INPUT_ADAPTER_H

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <SDL.h>

#include "input_manager.h"

class OverlayRenderer;
class OverlayUI;

namespace cataclysm::gui {
class EventBusAdapter;
}

class OverlayInputAdapter {
public:
    OverlayInputAdapter(OverlayRenderer& renderer,
                        OverlayUI& ui,
                        BN::GUI::InputManager& input_manager,
                        cataclysm::gui::EventBusAdapter& event_bus);
    ~OverlayInputAdapter();

    OverlayInputAdapter(const OverlayInputAdapter&) = delete;
    OverlayInputAdapter& operator=(const OverlayInputAdapter&) = delete;
    OverlayInputAdapter(OverlayInputAdapter&&) = delete;
    OverlayInputAdapter& operator=(OverlayInputAdapter&&) = delete;

    void Initialize(bool pass_through_enabled);
    void Shutdown();

    void SetOverlayActive(bool active);
    void SetFocusEligible(bool eligible);
    void SetPassThroughEnabled(bool enabled);

    void OnOverlayOpened();
    void OnOverlayClosed(bool was_cancelled = false);
    void OnOverlayMinimized(bool minimized);

    void OnInventoryVisibilityChanged(bool visible);
    void OnCharacterVisibilityChanged(bool visible);

    bool HandleEvent(const SDL_Event& event);

private:
    struct HandlerRegistration {
        BN::GUI::InputManager::EventType type;
        int id = 0;
    };

    OverlayRenderer& renderer_;
    OverlayUI& ui_;
    BN::GUI::InputManager& input_manager_;
    cataclysm::gui::EventBusAdapter& event_bus_;

    bool initialized_ = false;
    bool overlay_active_ = false;
    bool focus_eligible_ = false;
    bool pass_through_enabled_ = true;
    bool minimized_ = false;

    std::vector<HandlerRegistration> handler_ids_;

    std::optional<BN::GUI::InputManager::FocusState> previous_focus_state_;

    void RegisterHandlers();
    void UnregisterHandlers();

    bool HandleGuiEvent(const BN::GUI::GUIEvent& event);

    void UpdateFocusState();
    void RepublishModalState();
};

#endif // OVERLAY_INPUT_ADAPTER_H
