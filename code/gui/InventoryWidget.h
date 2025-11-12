#ifndef INVENTORY_WIDGET_H
#define INVENTORY_WIDGET_H

#include <functional>
#include <string>
#include <unordered_set>
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
        int column_index = 0;
        int row_index = 0;
        std::string entry_key;
    };
    std::vector<EntryBounds> last_entry_bounds_;
    std::unordered_set<std::string> handled_entries_;

    void DrawInventoryColumn(const inventory_column& column, int column_index, int active_column);
    static std::string BuildEntryKey(int column_index, int row_index, const inventory_entry& entry);
    const EntryBounds* FindEntryAtPosition(const ImVec2& position) const;
    bool DispatchEntryEvent(const EntryBounds& bounds);
    bool HandleMouseButtonEvent(const SDL_MouseButtonEvent& button_event);
    bool HandleKeyEvent(const SDL_KeyboardEvent& key_event);
};

#endif // INVENTORY_WIDGET_H
