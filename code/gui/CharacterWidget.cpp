#include "CharacterWidget.h"
#include "imgui.h"

CharacterWidget::CharacterWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter) {
}

CharacterWidget::~CharacterWidget() = default;

void CharacterWidget::Draw(const character_overlay_state& state) {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character");

    // Header
    ImGui::Text("%s", state.header_left.c_str());
    ImGui::SameLine();
    ImGui::Text("%s", state.header_right.c_str());

    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("CharacterTabs")) {
        for (size_t i = 0; i < state.tabs.size(); ++i) {
            const auto& tab = state.tabs[i];
            if (ImGui::BeginTabItem(tab.title.c_str())) {
                if (ImGui::BeginTable("CharacterTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    for (const auto& row : tab.rows) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", row.name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(row.color), "%s", row.value.c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // Info Panel
    ImGui::Text("%s", state.info_panel_text.c_str());

    ImGui::Separator();

    // Footer
    for (const auto& line : state.footer_lines) {
        ImGui::Text("%s", line.c_str());
    }

    ImGui::End();
}
