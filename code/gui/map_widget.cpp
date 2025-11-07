#include "map_widget.h"

#include <utility>

#include "imgui.h"

MapWidget::MapWidget() : tile_size_(0.0f, 0.0f) {
    const ImVec4 wall_color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    const ImVec4 floor_color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    const ImVec4 player_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 default_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    std::vector<std::string> ascii_map = {
        "##########",
        "#........#",
        "#..@.....#",
        "#........#",
        "##########",
    };

    for (const std::string& row_str : ascii_map) {
        std::vector<Tile> row;
        row.reserve(row_str.length());
        for (char c : row_str) {
            ImVec4 color = default_color;
            switch (c) {
                case '#':
                    color = wall_color;
                    break;
                case '.':
                    color = floor_color;
                    break;
                case '@':
                    color = player_color;
                    break;
                default:
                    break;
            }
            row.push_back({c, color});
        }
        mock_map_.push_back(row);
    }
}

MapWidget::~MapWidget() = default;

void MapWidget::SetTileCallback(TileCallback callback) {
    tile_callback_ = std::move(callback);
}

const ImVec2& MapWidget::GetTileSize() const {
    return tile_size_;
}

std::optional<TileSelection> MapWidget::GetSelectedTile() const {
    return selected_tile_;
}

void MapWidget::Draw() {
    ImGui::Begin("Map");

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 text_size = ImGui::CalcTextSize("@");
    tile_size_.x = text_size.x + style.FramePadding.x * 2.0f;
    tile_size_.y = text_size.y + style.FramePadding.y * 2.0f;

    for (int y = 0; y < static_cast<int>(mock_map_.size()); ++y) {
        const auto& row = mock_map_[y];
        for (int x = 0; x < static_cast<int>(row.size()); ++x) {
            const Tile& tile = row[x];
            const bool is_selected = selected_tile_.has_value() && selected_tile_->x == x && selected_tile_->y == y;

            ImGui::PushID(y * static_cast<int>(row.size()) + x);
            ImGui::PushStyleColor(ImGuiCol_Text, tile.color);

            std::string label(1, tile.character);
            const bool was_clicked = ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_None, tile_size_);

            if (ImGui::IsItemHovered()) {
                if (tile_callback_) {
                    tile_callback_(x, y, TileAction::Hover);
                }
            }

            if (was_clicked || ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                selected_tile_ = TileSelection{x, y};
                if (tile_callback_) {
                    tile_callback_(x, y, TileAction::Click);
                }
            }

            ImGui::PopStyleColor();

            if (x + 1 < static_cast<int>(row.size())) {
                ImGui::SameLine(0.0f, style.ItemSpacing.x);
            }

            ImGui::PopID();
        }
        ImGui::NewLine();
    }

    ImGui::End();
}
