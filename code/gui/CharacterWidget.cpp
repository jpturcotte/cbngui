#include "CharacterWidget.h"
#include "imgui.h"
#include <vector>
#include "events.h"

namespace {
// Helper to draw one of the three top grids.
void DrawGrid(const char* title, const std::vector<character_overlay_column_entry>& rows,
              int active_row_index, const std::string& tab_id,
              cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    if (ImGui::BeginTable(title, 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(220, 150))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool is_selected = row.highlighted || (static_cast<int>(i) == active_row_index);
            if (ImGui::Selectable(row.name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                event_bus_adapter.publish(cataclysm::gui::CharacterRowActivatedEvent(tab_id, i));
            }
            if (ImGui::IsItemHovered() && !row.tooltip.empty()) {
                ImGui::SetTooltip("%s", row.tooltip.c_str());
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(row.color), "%s", row.value.c_str());
        }
        ImGui::EndTable();
    }
}
} // namespace

void CharacterWidget::HandleTabNavigation(const character_overlay_state& state) {
    constexpr size_t kFirstTabBarIndex = 3;
    if (!ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        return;
    }

    if (state.tabs.size() <= kFirstTabBarIndex) {
        return;
    }

    const int tab_count = static_cast<int>(state.tabs.size());
    int current_index = state.active_tab_index;
    if (current_index < static_cast<int>(kFirstTabBarIndex) || current_index >= tab_count) {
        current_index = static_cast<int>(kFirstTabBarIndex);
    }

    const bool navigating_backward = ImGui::IsKeyDown(ImGuiKey_ModShift);
    int new_index = current_index;

    do {
        new_index = navigating_backward ? new_index - 1 : new_index + 1;
        if (new_index < 0) {
            new_index = tab_count - 1;
        } else if (new_index >= tab_count) {
            new_index = 0;
        }
    } while (new_index < static_cast<int>(kFirstTabBarIndex));

    event_bus_adapter_.publish(cataclysm::gui::CharacterTabRequestedEvent(state.tabs[new_index].id));
}

void CharacterWidget::HandleKeyPresses(const character_overlay_state& state) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        event_bus_adapter_.publish(cataclysm::gui::CharacterCommandEvent(cataclysm::gui::CharacterCommand::QUIT));
    } else if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        event_bus_adapter_.publish(cataclysm::gui::CharacterCommandEvent(cataclysm::gui::CharacterCommand::CONFIRM));
    } else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
        event_bus_adapter_.publish(cataclysm::gui::CharacterCommandEvent(cataclysm::gui::CharacterCommand::RENAME));
    } else if (ImGui::IsKeyPressed(ImGuiKey_Slash) && ImGui::IsKeyDown(ImGuiKey_ModShift)) {
        event_bus_adapter_.publish(cataclysm::gui::CharacterCommandEvent(cataclysm::gui::CharacterCommand::HELP));
    }

    HandleTabNavigation(state);
}

CharacterWidget::CharacterWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter) {
}

CharacterWidget::~CharacterWidget() = default;

void CharacterWidget::Draw(const character_overlay_state& state) {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character");

    HandleKeyPresses(state);

    // Header
    ImGui::TextUnformatted(state.header_left.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::TextUnformatted(state.header_right.c_str());
    ImGui::Separator();

    // Top Grids (Stats, Encumbrance, Speed)
    if (ImGui::BeginTable("TopGrids", 3)) {
        ImGui::TableNextColumn();
        if (state.tabs.size() > 0) {
            DrawGrid("Stats", state.tabs[0].rows, state.active_tab_index == 0 ? state.active_row_index : -1, state.tabs[0].id, event_bus_adapter_);
        }
        ImGui::TableNextColumn();
        if (state.tabs.size() > 1) {
            DrawGrid("Encumbrance", state.tabs[1].rows, state.active_tab_index == 1 ? state.active_row_index : -1, state.tabs[1].id, event_bus_adapter_);
        }
        ImGui::TableNextColumn();
        if (state.tabs.size() > 2) {
            DrawGrid("Speed", state.tabs[2].rows, state.active_tab_index == 2 ? state.active_row_index : -1, state.tabs[2].id, event_bus_adapter_);
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
                if (ImGui::BeginTabItem(tab.title.c_str(), nullptr, is_active_tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                    if (ImGui::IsItemClicked()) {
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
                            if (ImGui::IsItemHovered() && !row.tooltip.empty()) {
                                ImGui::SetTooltip("%s", row.tooltip.c_str());
                            }
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

        ImGui::TableNextColumn();
        // Info Panel
        ImGui::TextWrapped("%s", state.info_panel_text.c_str());

        ImGui::EndTable();
    }


    ImGui::Separator();

    // Footer
    for (const auto& line : state.footer_lines) {
        ImGui::TextUnformatted(line.c_str());
    }
    ImGui::Text("Help: %s, Tab: %s, Back Tab: %s, Confirm: %s, Quit: %s, Rename: %s",
                state.bindings.help.c_str(),
                state.bindings.tab.c_str(),
                state.bindings.back_tab.c_str(),
                state.bindings.confirm.c_str(),
                state.bindings.quit.c_str(),
                state.bindings.rename.c_str());

    ImGui::End();
}
