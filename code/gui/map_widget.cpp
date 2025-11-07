#include "map_widget.h"
#include "imgui.h"

MapWidget::MapWidget() {
    mock_map_ = {
        "##########",
        "#........#",
        "#..@.....#",
        "#........#",
        "##########",
    };
}

MapWidget::~MapWidget() {}

void MapWidget::Draw() {
    ImGui::Begin("Map");
    for (const auto& row : mock_map_) {
        ImGui::Text("%s", row.c_str());
    }
    ImGui::End();
}
