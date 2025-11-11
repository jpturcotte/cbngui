#ifndef INVENTORY_WIDGET_H
#define INVENTORY_WIDGET_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "InventoryOverlayState.h"
#include "event_bus_adapter.h"
#include "imgui.h"

class InventoryWidget {
public:
    InventoryWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter);
    ~InventoryWidget();

    InventoryWidget(const InventoryWidget&) = delete;
    InventoryWidget& operator=(const InventoryWidget&) = delete;
    InventoryWidget(InventoryWidget&&) = delete;
    InventoryWidget& operator=(InventoryWidget&&) = delete;

    void Draw(const inventory_overlay_state& state);

private:
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
};

#endif // INVENTORY_WIDGET_H
