#ifndef OVERLAY_UI_H
#define OVERLAY_UI_H

#include <memory>

namespace cataclysm {
namespace gui {
class EventBusAdapter;
}
}

class MapWidget;
class InventoryWidget;

class OverlayUI {
public:
    OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter);
    ~OverlayUI();

    void Draw();

private:
    std::unique_ptr<MapWidget> map_widget_;
    std::unique_ptr<InventoryWidget> inventory_widget_;
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
};

#endif // OVERLAY_UI_H
