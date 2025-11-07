#include "map_widget.h"
#include "imgui.h"

MapWidget::MapWidget() {
    const ImVec4 wall_color   = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    const ImVec4 floor_color  = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
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
                case '#': color = wall_color; break;
                case '.': color = floor_color; break;
                case '@': color = player_color; break;
            }
            row.push_back({c, color});
        }
        mock_map_.push_back(row);
    }
}

MapWidget::~MapWidget() {}

void MapWidget::Draw() {
    ImGui::Begin("Map");
    for (const auto& row : mock_map_) {
        for (const auto& tile : row) {
            ImGui::PushStyleColor(ImGuiCol_Text, tile.color);
            ImGui::Text("%c", tile.character);
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 0.0f);
        }
        ImGui::NewLine();
    }
    ImGui::End();
}
