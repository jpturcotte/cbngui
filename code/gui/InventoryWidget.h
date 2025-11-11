#ifndef INVENTORY_WIDGET_H
#define INVENTORY_WIDGET_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <SDL.h>

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

    bool HandleEvent(const SDL_Event& event);

    [[nodiscard]] bool GetEntryRect(const std::string& hotkey,
                                    const std::string& label,
                                    ImVec2* min,
                                    ImVec2* max) const;

private:
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
    struct EntryBounds {
        inventory_entry entry;
        ImVec2 min{0.0f, 0.0f};
        ImVec2 max{0.0f, 0.0f};
    };
    std::vector<EntryBounds> last_entry_bounds_;

    void DrawInventoryColumn(const inventory_column& column, int column_index, int active_column);
};

#endif // INVENTORY_WIDGET_H
