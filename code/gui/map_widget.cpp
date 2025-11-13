#include "map_widget.h"
#include "imgui.h"

#include <algorithm>

MapWidget::MapWidget(cataclysm::gui::EventBusAdapter &event_bus_adapter)
    : tile_size_(1.0f, 1.0f), event_bus_adapter_(event_bus_adapter) {
}

MapWidget::~MapWidget() = default;

void MapWidget::UpdateMapTexture(SDL_Texture* texture, int width, int height, int tiles_w, int tiles_h) {
    map_texture_ = texture;
    texture_size_ = ImVec2(static_cast<float>(width), static_cast<float>(height));
    tiles_w_ = std::max(0, tiles_w);
    tiles_h_ = std::max(0, tiles_h);
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

        if (tiles_w_ > 0 && tiles_h_ > 0 && draw_size.x > 0.0f && draw_size.y > 0.0f) {
            tile_size_.x = draw_size.x / static_cast<float>(tiles_w_);
            tile_size_.y = draw_size.y / static_cast<float>(tiles_h_);
        } else {
            tile_size_ = ImVec2(0.0f, 0.0f);
        }

        has_image_rect_ = true;
        last_image_min_ = ImGui::GetItemRectMin();
        last_image_max_ = ImGui::GetItemRectMax();

        if (ImGui::IsItemHovered()) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 image_pos = ImGui::GetItemRectMin();
            ImVec2 relative_pos = ImVec2(mouse_pos.x - image_pos.x, mouse_pos.y - image_pos.y);

            if (tiles_w_ > 0 && tiles_h_ > 0 && draw_size.x > 0.0f && draw_size.y > 0.0f) {
                float normalized_x = relative_pos.x / draw_size.x;
                float normalized_y = relative_pos.y / draw_size.y;

                normalized_x = std::clamp(normalized_x, 0.0f, 1.0f);
                normalized_y = std::clamp(normalized_y, 0.0f, 1.0f);

                int tile_x = static_cast<int>(normalized_x * static_cast<float>(tiles_w_));
                int tile_y = static_cast<int>(normalized_y * static_cast<float>(tiles_h_));

                const int max_tile_x = std::max(tiles_w_ - 1, 0);
                const int max_tile_y = std::max(tiles_h_ - 1, 0);

                tile_x = std::clamp(tile_x, 0, max_tile_x);
                tile_y = std::clamp(tile_y, 0, max_tile_y);

                event_bus_adapter_.publishMapTileHovered(tile_x, tile_y);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    selected_tile_ = TileSelection{tile_x, tile_y};
                    event_bus_adapter_.publishMapTileClicked(tile_x, tile_y);
                }
            }
        }
    } else {
        tile_size_ = ImVec2(0.0f, 0.0f);
        has_image_rect_ = false;
        ImGui::TextUnformatted("Waiting for map snapshot…");
    }
    ImGui::End();
}

bool MapWidget::GetLastImageRect(ImVec2* min, ImVec2* max) const {
    if (!has_image_rect_ || min == nullptr || max == nullptr) {
        return false;
    }
    *min = last_image_min_;
    *max = last_image_max_;
    return true;
}
