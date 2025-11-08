#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <optional>
#include <string>
#include <vector>

#include <SDL.h>
#include "event_bus_adapter.h"
#include "imgui.h"

struct TileSelection {
    int x = 0;
    int y = 0;
};

class MapWidget {
public:
    explicit MapWidget(cataclysm::gui::EventBusAdapter &event_bus_adapter);
    ~MapWidget();

    void Draw();
    void UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h);

    [[nodiscard]] const ImVec2 &GetTileSize() const;
    [[nodiscard]] std::optional<TileSelection> GetSelectedTile() const;

private:
    ImVec2 tile_size_;
    std::optional<TileSelection> selected_tile_;
    cataclysm::gui::EventBusAdapter &event_bus_adapter_;
    SDL_Texture* map_texture_ = nullptr;
    ImVec2 texture_size_{0,0};
    ImVec2 tile_dimensions_{0,0};
};

#endif // MAP_WIDGET_H
