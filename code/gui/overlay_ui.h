#ifndef OVERLAY_UI_H
#define OVERLAY_UI_H

#include <SDL.h>
#include <memory>

#include "InventoryOverlayState.h"
#include "CharacterOverlayState.h"

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
    OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter);
    ~OverlayUI();

    void Draw();
    void DrawInventory(const inventory_overlay_state& state);
    void DrawCharacter(const character_overlay_state& state);

    void UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h);

    MapWidget& GetMapWidget();
    InventoryWidget& GetInventoryWidget();
    CharacterWidget& GetCharacterWidget();

private:
    std::unique_ptr<MapWidget> map_widget_;
    std::unique_ptr<InventoryWidget> inventory_widget_;
    std::unique_ptr<CharacterWidget> character_widget_;
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
};

#endif // OVERLAY_UI_H
