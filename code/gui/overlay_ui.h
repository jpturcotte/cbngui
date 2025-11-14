#ifndef OVERLAY_UI_H
#define OVERLAY_UI_H

#include <SDL.h>
#include <memory>
#include <vector>

#include "CharacterOverlayState.h"
#include "InventoryOverlayState.h"
#include "input_manager.h"

namespace cataclysm {
namespace gui {
class EventBusAdapter;
}
}

class MapWidget;
class InventoryWidget;
class CharacterWidget;

class OverlayUI {
public:
    OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter,
              BN::GUI::InputManager* input_manager);
    ~OverlayUI();

    void Draw();
    void DrawInventory(const inventory_overlay_state& state);
    void DrawCharacter(const character_overlay_state& state);

    void UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h);

    MapWidget& GetMapWidget();
    InventoryWidget& GetInventoryWidget();
    CharacterWidget& GetCharacterWidget();

    bool ProcessEvent(const SDL_Event& event);

    void SetInventoryVisible(bool visible);
    bool IsInventoryVisible() const;

    void SetCharacterVisible(bool visible);
    bool IsCharacterVisible() const;

private:
    void RegisterInventoryHandlers();
    void UnregisterInventoryHandlers();
    void RegisterCharacterHandlers();
    void UnregisterCharacterHandlers();

    bool HandleInventoryGuiEvent(const BN::GUI::GUIEvent& event);
    bool HandleCharacterGuiEvent(const BN::GUI::GUIEvent& event);

    std::unique_ptr<MapWidget> map_widget_;
    std::unique_ptr<InventoryWidget> inventory_widget_;
    std::unique_ptr<CharacterWidget> character_widget_;
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
    BN::GUI::InputManager* input_manager_ = nullptr;

    bool inventory_visible_ = false;
    bool character_visible_ = false;

    std::vector<int> inventory_handler_ids_;
    std::vector<int> character_handler_ids_;

    struct ConsumptionRecord {
        Uint32 type = 0;
        Uint32 timestamp = 0;
        bool consumed = false;
    };

    ConsumptionRecord inventory_consumption_;
    ConsumptionRecord character_consumption_;
};

#endif // OVERLAY_UI_H
