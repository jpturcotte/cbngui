#include "CharacterWidget.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "events.h"
#include "imgui.h"

namespace {
constexpr ImVec2 kTopGridSize(240.0f, 180.0f);

void DrawGrid(const character_overlay_tab& tab,
              int active_row_index,
              cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    ImGui::PushID(tab.id.c_str());
    if (ImGui::BeginTable("TopGrid", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, kTopGridSize)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < tab.rows.size(); ++i) {
            const auto& row = tab.rows[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool is_selected = row.highlighted || (static_cast<int>(i) == active_row_index);
            if (ImGui::Selectable(row.name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                event_bus_adapter.publish(cataclysm::gui::CharacterRowActivatedEvent(tab.id, i));
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !row.tooltip.empty()) {
                ImGui::SetTooltip("%s", row.tooltip.c_str());
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(row.color), "%s", row.value.c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
}

void DrawInfoPanel(std::string_view text) {
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string_view line = (end == std::string_view::npos)
            ? text.substr(start)
            : text.substr(start, end - start);
        ImGui::TextUnformatted(line.data(), line.data() + line.size());
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
}
} // namespace

CharacterWidget::CharacterWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter) {
}

CharacterWidget::~CharacterWidget() = default;

void CharacterWidget::Draw(const character_overlay_state& state) {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character");

    // Header
    ImGui::TextUnformatted(state.header_left.c_str());
    if (!state.header_right.empty()) {
        float header_width = ImGui::CalcTextSize(state.header_right.c_str()).x;
        float right_edge = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine(std::max(right_edge - header_width, ImGui::GetCursorPosX()));
        ImGui::TextUnformatted(state.header_right.c_str());
    }
    ImGui::Separator();

    // Top Grids (Stats, Encumbrance, Speed)
    if (ImGui::BeginTable("TopGrids", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        if (state.tabs.size() > 0) {
            DrawGrid(state.tabs[0], state.active_tab_index == 0 ? state.active_row_index : -1, event_bus_adapter_);
        }
        ImGui::TableNextColumn();
        if (state.tabs.size() > 1) {
            DrawGrid(state.tabs[1], state.active_tab_index == 1 ? state.active_row_index : -1, event_bus_adapter_);
        }
        ImGui::TableNextColumn();
        if (state.tabs.size() > 2) {
            DrawGrid(state.tabs[2], state.active_tab_index == 2 ? state.active_row_index : -1, event_bus_adapter_);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // Lower Section (Tabs and Info Panel)
    if (ImGui::BeginTable("LowerSection", 2)) {
        ImGui::TableSetupColumn("Tabs", ImGuiTableColumnFlags_WidthFixed, 400.0f);
        ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextColumn();
        // Tabbed Panel
        if (ImGui::BeginTabBar("CharacterTabs", ImGuiTabBarFlags_None)) {
            for (size_t i = 3; i < state.tabs.size(); ++i) {
                const auto& tab = state.tabs[i];
                bool is_active_tab = static_cast<int>(i) == state.active_tab_index;
                ImGui::PushID(tab.id.c_str());
                const ImGuiTabItemFlags tab_flags = is_active_tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(tab.title.c_str(), nullptr, tab_flags)) {
                    if (!is_active_tab && ImGui::IsItemActivated()) {
                        event_bus_adapter_.publish(cataclysm::gui::CharacterTabRequestedEvent(tab.id));
                    }
                    if (ImGui::BeginTable("CharacterTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        for (size_t j = 0; j < tab.rows.size(); ++j) {
                            const auto& row = tab.rows[j];
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            bool is_selected = row.highlighted || (is_active_tab && (static_cast<int>(j) == state.active_row_index));
                            if (ImGui::Selectable(row.name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                                event_bus_adapter_.publish(cataclysm::gui::CharacterRowActivatedEvent(tab.id, j));
                            }
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !row.tooltip.empty()) {
                                ImGui::SetTooltip("%s", row.tooltip.c_str());
                            }
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(row.color), "%s", row.value.c_str());
                        }
                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::PopID();
            }
            ImGui::EndTabBar();
        }

        ImGui::TableNextColumn();
        // Info Panel
        DrawInfoPanel(state.info_panel_text);

        ImGui::EndTable();
    }


    ImGui::Separator();

    // Footer
    for (const auto& line : state.footer_lines) {
        ImGui::TextUnformatted(line.c_str());
    }
    ImGui::Spacing();

    const float command_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    auto draw_command_button = [&](const char* label,
                                   const std::string& binding,
                                   cataclysm::gui::CharacterCommand command) {
        if (ImGui::SmallButton(label)) {
            event_bus_adapter_.publish(cataclysm::gui::CharacterCommandEvent(command));
        }
        if (!binding.empty()) {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextDisabled("%s", binding.c_str());
        }
    };

    draw_command_button("Help", state.bindings.help, cataclysm::gui::CharacterCommand::HELP);
    ImGui::SameLine(0.0f, command_spacing);
    draw_command_button("Confirm", state.bindings.confirm, cataclysm::gui::CharacterCommand::CONFIRM);
    ImGui::SameLine(0.0f, command_spacing);
    draw_command_button("Quit", state.bindings.quit, cataclysm::gui::CharacterCommand::QUIT);
    ImGui::SameLine(0.0f, command_spacing);
    draw_command_button("Rename", state.bindings.rename, cataclysm::gui::CharacterCommand::RENAME);
    ImGui::NewLine();

    ImGui::Text("Help: %s, Tab: %s, Back Tab: %s, Confirm: %s, Quit: %s, Rename: %s",
                state.bindings.help.c_str(),
                state.bindings.tab.c_str(),
                state.bindings.back_tab.c_str(),
                state.bindings.confirm.c_str(),
                state.bindings.quit.c_str(),
                state.bindings.rename.c_str());

    ImGui::End();
}
