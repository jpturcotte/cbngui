#include "overlay_ui.h"

#include "CharacterWidget.h"
#include "InventoryWidget.h"
#include "event_bus_adapter.h"
#include "imgui.h"
#include "map_widget.h"

#include <array>

OverlayUI::OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter,
                     BN::GUI::InputManager* input_manager)
    : map_widget_(std::make_unique<MapWidget>(event_bus_adapter)),
      inventory_widget_(std::make_unique<InventoryWidget>(event_bus_adapter)),
      character_widget_(std::make_unique<CharacterWidget>(event_bus_adapter)),
      event_bus_adapter_(event_bus_adapter),
      input_manager_(input_manager) {}

OverlayUI::~OverlayUI() {
    UnregisterInventoryHandlers();
    UnregisterCharacterHandlers();
}

void OverlayUI::Draw() {
    map_widget_->Draw();
}

void OverlayUI::DrawInventory(const inventory_overlay_state& state) {
    inventory_widget_->Draw(state);
}

void OverlayUI::DrawCharacter(const character_overlay_state& state) {
    character_widget_->Draw(state);
}

void OverlayUI::UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h) {
    map_widget_->UpdateMapTexture(texture, width, height, tiles_w, tiles_h);
}

MapWidget& OverlayUI::GetMapWidget() {
    return *map_widget_;
}

InventoryWidget& OverlayUI::GetInventoryWidget() {
    return *inventory_widget_;
}

CharacterWidget& OverlayUI::GetCharacterWidget() {
    return *character_widget_;
}

bool OverlayUI::ProcessEvent(const SDL_Event& event) {
    bool consumed = false;

    if (inventory_visible_) {
        bool handled = inventory_widget_->HandleEvent(event);
        inventory_consumption_.type = event.type;
        inventory_consumption_.timestamp = event.common.timestamp;
        inventory_consumption_.consumed = handled;
        consumed = consumed || handled;
    } else {
        inventory_consumption_ = ConsumptionRecord{};
    }

    if (character_visible_) {
        bool handled = character_widget_->HandleEvent(event);
        character_consumption_.type = event.type;
        character_consumption_.timestamp = event.common.timestamp;
        character_consumption_.consumed = handled;
        consumed = consumed || handled;
    } else {
        character_consumption_ = ConsumptionRecord{};
    }

    return consumed;
}

void OverlayUI::SetInventoryVisible(bool visible) {
    if (inventory_visible_ == visible) {
        return;
    }

    inventory_visible_ = visible;
    if (inventory_visible_) {
        RegisterInventoryHandlers();
    } else {
        UnregisterInventoryHandlers();
    }
}

bool OverlayUI::IsInventoryVisible() const {
    return inventory_visible_;
}

void OverlayUI::SetCharacterVisible(bool visible) {
    if (character_visible_ == visible) {
        return;
    }

    character_visible_ = visible;
    if (character_visible_) {
        RegisterCharacterHandlers();
    } else {
        UnregisterCharacterHandlers();
    }
}

bool OverlayUI::IsCharacterVisible() const {
    return character_visible_;
}

void OverlayUI::RegisterInventoryHandlers() {
    if (!input_manager_ || !inventory_handler_ids_.empty()) {
        return;
    }

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
        int id = input_manager_->RegisterHandler(
            type,
            [this](const BN::GUI::GUIEvent& event) {
                return this->HandleInventoryGuiEvent(event);
            },
            InputManager::Priority::NORMAL,
            "inventory_overlay");
        inventory_handler_ids_.push_back(id);
    }
}

void OverlayUI::UnregisterInventoryHandlers() {
    if (!input_manager_) {
        inventory_handler_ids_.clear();
        return;
    }

    for (int id : inventory_handler_ids_) {
        input_manager_->UnregisterHandler(id);
    }
    inventory_handler_ids_.clear();
}

void OverlayUI::RegisterCharacterHandlers() {
    if (!input_manager_ || !character_handler_ids_.empty()) {
        return;
    }

    using namespace BN::GUI;
    const std::array<InputManager::EventType, 3> kTypes = {
        InputManager::EventType::KEYBOARD_PRESS,
        InputManager::EventType::KEYBOARD_RELEASE,
        InputManager::EventType::TEXT_INPUT
    };

    for (const auto type : kTypes) {
        int id = input_manager_->RegisterHandler(
            type,
            [this](const BN::GUI::GUIEvent& event) {
                return this->HandleCharacterGuiEvent(event);
            },
            InputManager::Priority::NORMAL,
            "character_overlay");
        character_handler_ids_.push_back(id);
    }
}

void OverlayUI::UnregisterCharacterHandlers() {
    if (!input_manager_) {
        character_handler_ids_.clear();
        return;
    }

    for (int id : character_handler_ids_) {
        input_manager_->UnregisterHandler(id);
    }
    character_handler_ids_.clear();
}

bool OverlayUI::HandleInventoryGuiEvent(const BN::GUI::GUIEvent& event) {
    if (!inventory_visible_) {
        return false;
    }

    const SDL_Event& sdl = event.sdl_event;
    if (inventory_consumption_.consumed && inventory_consumption_.type == sdl.type &&
        inventory_consumption_.timestamp == sdl.common.timestamp) {
        return true;
    }

    bool handled = inventory_widget_->HandleEvent(sdl);
    inventory_consumption_.type = sdl.type;
    inventory_consumption_.timestamp = sdl.common.timestamp;
    inventory_consumption_.consumed = handled;
    return handled;
}

bool OverlayUI::HandleCharacterGuiEvent(const BN::GUI::GUIEvent& event) {
    if (!character_visible_) {
        return false;
    }

    const SDL_Event& sdl = event.sdl_event;
    if (character_consumption_.consumed && character_consumption_.type == sdl.type &&
        character_consumption_.timestamp == sdl.common.timestamp) {
        return true;
    }

    bool handled = character_widget_->HandleEvent(sdl);
    character_consumption_.type = sdl.type;
    character_consumption_.timestamp = sdl.common.timestamp;
    character_consumption_.consumed = handled;
    return handled;
}
