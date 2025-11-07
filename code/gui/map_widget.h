#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "imgui.h"

struct Tile {
    char character;
    ImVec4 color;
};

enum class TileAction {
    Hover,
    Click
};

struct TileSelection {
    int x = 0;
    int y = 0;
};

class MapWidget {
public:
    MapWidget();
    ~MapWidget();

    void Draw();

    using TileCallback = std::function<void(int x, int y, TileAction action)>;
    void SetTileCallback(TileCallback callback);

    [[nodiscard]] const ImVec2& GetTileSize() const;
    [[nodiscard]] std::optional<TileSelection> GetSelectedTile() const;

private:
    std::vector<std::vector<Tile>> mock_map_;
    ImVec2 tile_size_;
    TileCallback tile_callback_;
    std::optional<TileSelection> selected_tile_;
};

#endif // MAP_WIDGET_H
