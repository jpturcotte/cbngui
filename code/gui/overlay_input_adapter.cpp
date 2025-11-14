#include "overlay_input_adapter.h"

#include "event_bus_adapter.h"
#include "overlay_renderer.h"
#include "overlay_ui.h"

#include <array>

namespace {
constexpr const char* kOverlayLifecycleId = "overlay_ui";
}

OverlayInputAdapter::OverlayInputAdapter(OverlayRenderer& renderer,
                                         OverlayUI& ui,
                                         BN::GUI::InputManager& input_manager,
                                         cataclysm::gui::EventBusAdapter& event_bus)
    : renderer_(renderer), ui_(ui), input_manager_(input_manager), event_bus_(event_bus) {}

OverlayInputAdapter::~OverlayInputAdapter() { Shutdown(); }

void OverlayInputAdapter::Initialize(bool pass_through_enabled) {
    if (initialized_) {
        return;
    }

    pass_through_enabled_ = pass_through_enabled;
    RegisterHandlers();
    initialized_ = true;
}

void OverlayInputAdapter::Shutdown() {
    UnregisterHandlers();
    initialized_ = false;
    overlay_active_ = false;
    focus_eligible_ = false;
    minimized_ = false;
    previous_focus_state_.reset();
}

void OverlayInputAdapter::SetOverlayActive(bool active) {
    if (overlay_active_ == active) {
        return;
    }
    overlay_active_ = active;
    UpdateFocusState();
}

void OverlayInputAdapter::SetFocusEligible(bool eligible) {
    if (focus_eligible_ == eligible) {
        return;
    }
    focus_eligible_ = eligible;
    UpdateFocusState();
}

void OverlayInputAdapter::SetPassThroughEnabled(bool enabled) {
    if (pass_through_enabled_ == enabled) {
        return;
    }
    pass_through_enabled_ = enabled;
    UpdateFocusState();
    if (overlay_active_) {
        RepublishModalState();
    }
}

void OverlayInputAdapter::OnOverlayOpened() {
    SetOverlayActive(true);
    RepublishModalState();
}

void OverlayInputAdapter::OnOverlayClosed(bool was_cancelled) {
    SetOverlayActive(false);
    RepublishModalState();
    event_bus_.publishOverlayClose(kOverlayLifecycleId, was_cancelled);
}

void OverlayInputAdapter::OnOverlayMinimized(bool minimized) {
    if (minimized_ == minimized) {
        return;
    }
    minimized_ = minimized;
    UpdateFocusState();
}

void OverlayInputAdapter::OnInventoryVisibilityChanged(bool visible) {
    ui_.SetInventoryVisible(visible);
}

void OverlayInputAdapter::OnCharacterVisibilityChanged(bool visible) {
    ui_.SetCharacterVisible(visible);
}

bool OverlayInputAdapter::HandleEvent(const SDL_Event& event) {
    if (!initialized_ || !overlay_active_ || !focus_eligible_ || minimized_) {
        return false;
    }

    const bool renderer_consumed = renderer_.HandleEvent(event);
    const bool widget_consumed = ui_.ProcessEvent(event);

    return renderer_consumed || widget_consumed;
}

void OverlayInputAdapter::RegisterHandlers() {
    using namespace BN::GUI;
    const std::array<InputManager::EventType, 6> kTypes = {
        InputManager::EventType::KEYBOARD_PRESS,
        InputManager::EventType::KEYBOARD_RELEASE,
        InputManager::EventType::TEXT_INPUT,
        InputManager::EventType::MOUSE_BUTTON_PRESS,
        InputManager::EventType::MOUSE_BUTTON_RELEASE,
        InputManager::EventType::MOUSE_WHEEL
    };

    for (const auto type : kTypes) {
        int id = input_manager_.RegisterHandler(
            type,
            [this](const BN::GUI::GUIEvent& event) {
                return this->HandleGuiEvent(event);
            },
            BN::GUI::InputManager::Priority::HIGH,
            "overlay_ui");
        handler_ids_.push_back({type, id});
    }

    int motion_id = input_manager_.RegisterHandler(
        BN::GUI::InputManager::EventType::MOUSE_MOVE,
        [this](const BN::GUI::GUIEvent& event) {
            return this->HandleGuiEvent(event);
        },
        BN::GUI::InputManager::Priority::HIGH,
        "overlay_ui");
    handler_ids_.push_back({BN::GUI::InputManager::EventType::MOUSE_MOVE, motion_id});
}

void OverlayInputAdapter::UnregisterHandlers() {
    for (const auto& reg : handler_ids_) {
        if (reg.id != 0) {
            input_manager_.UnregisterHandler(reg.id);
        }
    }
    handler_ids_.clear();
}

bool OverlayInputAdapter::HandleGuiEvent(const BN::GUI::GUIEvent& event) {
    return HandleEvent(event.sdl_event);
}

void OverlayInputAdapter::UpdateFocusState() {
    if (!initialized_) {
        return;
    }

    if (!overlay_active_ || !focus_eligible_ || minimized_) {
        if (previous_focus_state_) {
            input_manager_.SetFocusState(*previous_focus_state_, "overlay-inactive");
            previous_focus_state_.reset();
        }
        return;
    }

    BN::GUI::InputManager::FocusState desired =
        pass_through_enabled_ ? BN::GUI::InputManager::FocusState::SHARED
                              : BN::GUI::InputManager::FocusState::GUI;

    if (!previous_focus_state_) {
        previous_focus_state_ = input_manager_.GetFocusState();
    }

    input_manager_.SetFocusState(desired, pass_through_enabled_ ? "overlay-shared" : "overlay-modal");
}

void OverlayInputAdapter::RepublishModalState() {
    const bool is_modal = !pass_through_enabled_ && overlay_active_;
    if (overlay_active_) {
        event_bus_.publishOverlayOpen(kOverlayLifecycleId, is_modal);
    }
}
