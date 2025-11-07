#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <optional>
#include <string>
#include <vector>

#include "event_bus_adapter.h"
#include "imgui.h"

struct Tile {
    char character;
    ImVec4 color;
};

struct TileSelection {
    int x = 0;
    int y = 0;
};

class MapWidget {
public:
    explicit MapWidget(cataclysm::gui::EventBusAdapter &event_bus_adapter);
    ~MapWidget();

    void Draw();

    [[nodiscard]] const ImVec2 &GetTileSize() const;
    [[nodiscard]] std::optional<TileSelection> GetSelectedTile() const;

private:
    std::vector<std::vector<Tile>> mock_map_;
    ImVec2 tile_size_;
    std::optional<TileSelection> selected_tile_;
    cataclysm::gui::EventBusAdapter &event_bus_adapter_;
};

#endif // MAP_WIDGET_H
