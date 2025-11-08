#include "map_widget.h"
#include "imgui.h"

MapWidget::MapWidget(cataclysm::gui::EventBusAdapter &event_bus_adapter)
    : tile_size_(1.0f, 1.0f), event_bus_adapter_(event_bus_adapter) {
}

MapWidget::~MapWidget() = default;

void MapWidget::UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h) {
    map_texture_ = texture;
    texture_size_ = ImVec2(static_cast<float>(width), static_cast<float>(height));
    tile_dimensions_ = ImVec2(static_cast<float>(tiles_w), static_cast<float>(tiles_h));
}

const ImVec2 &MapWidget::GetTileSize() const {
    return tile_size_;
}

std::optional<TileSelection> MapWidget::GetSelectedTile() const {
    return selected_tile_;
}

void MapWidget::Draw() {
    ImGui::Begin("Game Map");
    if (map_texture_) {
        const float aspect = texture_size_.x / texture_size_.y;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 draw_size = avail;
        if (draw_size.x <= 0 || draw_size.y <= 0) {
            draw_size = texture_size_;
        } else if (draw_size.x / draw_size.y > aspect) {
            draw_size.x = draw_size.y * aspect;
        } else {
            draw_size.y = draw_size.x / aspect;
        }
        ImGui::Image(reinterpret_cast<ImTextureID>(map_texture_), draw_size);

        if (ImGui::IsItemHovered()) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 image_pos = ImGui::GetItemRectMin();
            ImVec2 relative_pos = ImVec2(mouse_pos.x - image_pos.x, mouse_pos.y - image_pos.y);

            if (tile_dimensions_.x > 0 && tile_dimensions_.y > 0) {
                int tile_x = static_cast<int>((relative_pos.x / draw_size.x) * tile_dimensions_.x);
                int tile_y = static_cast<int>((relative_pos.y / draw_size.y) * tile_dimensions_.y);
                event_bus_adapter_.publishMapTileHovered(tile_x, tile_y);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    selected_tile_ = TileSelection{tile_x, tile_y};
                    event_bus_adapter_.publishMapTileClicked(tile_x, tile_y);
                }
            }
        }
    } else {
        ImGui::TextUnformatted("Waiting for map snapshot…");
    }
    ImGui::End();
}
