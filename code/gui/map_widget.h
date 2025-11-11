#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <SDL.h>

#include <optional>
#include <string>
#include <vector>

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
    [[nodiscard]] bool GetLastImageRect(ImVec2* min, ImVec2* max) const;

private:
    ImVec2 tile_size_;
    std::optional<TileSelection> selected_tile_;
    cataclysm::gui::EventBusAdapter &event_bus_adapter_;
    SDL_Texture* map_texture_ = nullptr;
    ImVec2 texture_size_{0,0};
    ImVec2 tile_dimensions_{0,0};
    bool has_image_rect_ = false;
    ImVec2 last_image_min_{0.0f, 0.0f};
    ImVec2 last_image_max_{0.0f, 0.0f};
};

#endif // MAP_WIDGET_H
