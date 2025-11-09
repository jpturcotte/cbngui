#include "overlay_ui.h"

#include "InventoryWidget.h"
#include "event_bus_adapter.h"
#include "imgui.h"
#include "map_widget.h"

OverlayUI::OverlayUI(cataclysm::gui::EventBusAdapter &event_bus_adapter)
    : map_widget_(std::make_unique<MapWidget>(event_bus_adapter)),
      inventory_widget_(std::make_unique<InventoryWidget>(event_bus_adapter)),
      event_bus_adapter_(event_bus_adapter) {}

OverlayUI::~OverlayUI() = default;

void OverlayUI::Draw() {
    // ImGui::ShowDemoWindow();
    map_widget_->Draw();
}

void OverlayUI::DrawInventory(const inventory_overlay_state& state) {
    inventory_widget_->Draw(state);
}

void OverlayUI::UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h) {
    map_widget_->UpdateMapTexture(texture, width, height, tiles_w, tiles_h);
}
