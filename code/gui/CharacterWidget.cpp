#include "CharacterWidget.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "events.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace {
constexpr ImVec2 kTopGridSize(240.0f, 180.0f);

bool IsCharacterWindowFocused() {
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows |
                                  ImGuiFocusedFlags_NoPopupHierarchy);
}

void HandleCommandKeys(cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    if (!IsCharacterWindowFocused()) {
        return;
    }

    auto publish_command = [&](cataclysm::gui::CharacterCommand command) {
        event_bus_adapter.publish(cataclysm::gui::CharacterCommandEvent(command));
    };

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        publish_command(cataclysm::gui::CharacterCommand::QUIT);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        publish_command(cataclysm::gui::CharacterCommand::CONFIRM);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        publish_command(cataclysm::gui::CharacterCommand::RENAME);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F1, false) ||
        (ImGui::IsKeyPressed(ImGuiKey_Slash, false) && ImGui::GetIO().KeyShift)) {
        publish_command(cataclysm::gui::CharacterCommand::HELP);
    }
}

void HandleTabNavigation(const character_overlay_state& state,
                         cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    if (!IsCharacterWindowFocused() || state.tabs.empty()) {
        return;
    }

    const int tab_count = static_cast<int>(state.tabs.size());
    const int active_index = std::clamp(state.active_tab_index, 0, tab_count - 1);

    const bool shift_held = ImGui::GetIO().KeyShift;
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        int target_index = active_index;
        if (shift_held) {
            target_index = (active_index - 1 + tab_count) % tab_count;
        } else {
            target_index = (active_index + 1) % tab_count;
        }

        if (target_index != active_index) {
            event_bus_adapter.publish(
                cataclysm::gui::CharacterTabRequestedEvent(state.tabs[target_index].id));
        }
    }
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
    tab_rects_.clear();
    row_rects_.clear();
    command_button_rects_.clear();
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
    auto draw_grid = [&](const character_overlay_tab& tab, int active_row_index) {
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
                    event_bus_adapter_.publish(cataclysm::gui::CharacterRowActivatedEvent(tab.id, i));
                }
                RecordRect(row_rects_, tab.id + ":" + std::to_string(i));
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && !row.tooltip.empty()) {
                    ImGui::SetTooltip("%s", row.tooltip.c_str());
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(row.color), "%s", row.value.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
    };

    if (ImGui::BeginTable("TopGrids", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        if (state.tabs.size() > 0) {
            draw_grid(state.tabs[0], state.active_tab_index == 0 ? state.active_row_index : -1);
        }
        ImGui::TableNextColumn();
        if (state.tabs.size() > 1) {
            draw_grid(state.tabs[1], state.active_tab_index == 1 ? state.active_row_index : -1);
        }
        ImGui::TableNextColumn();
        if (state.tabs.size() > 2) {
            draw_grid(state.tabs[2], state.active_tab_index == 2 ? state.active_row_index : -1);
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
                bool tab_open = ImGui::BeginTabItem(tab.title.c_str(), nullptr, tab_flags);
                RecordRect(tab_rects_, tab.id);
                ImVec2 tab_min = tab_rects_.back().min;
                ImVec2 tab_max = tab_rects_.back().max;
                if (ImGuiTabBar* tab_bar = ImGui::GetCurrentTabBar()) {
                    const ImGuiID tab_item_id = ImGui::GetItemID();
                    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
                    float computed_offset = 0.0f;
                    for (int tab_index = 0; tab_index < tab_bar->Tabs.Size; ++tab_index) {
                        const ImGuiTabItem& tab_item = tab_bar->Tabs[tab_index];
                        if (tab_item.ID == tab_item_id) {
                            tab_min.x = tab_bar->BarRect.Min.x + computed_offset;
                            tab_max.x = tab_min.x + tab_item.Width;
                            tab_min.y = tab_bar->BarRect.Min.y;
                            tab_max.y = tab_bar->BarRect.Max.y;
                            tab_rects_.back().min = tab_min;
                            tab_rects_.back().max = tab_max;
                            break;
                        }
                        computed_offset += tab_bar->Tabs[tab_index].Width + spacing;
                    }
                }
                const bool tab_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                const bool tab_activated = ImGui::IsItemActivated();
                if (!is_active_tab && (tab_clicked || tab_activated)) {
                    const ImVec2 mouse_pos = ImGui::GetMousePos();
                    const bool within_tab = mouse_pos.x >= tab_min.x && mouse_pos.x <= tab_max.x &&
                                            mouse_pos.y >= tab_min.y && mouse_pos.y <= tab_max.y;
                    if (within_tab) {
                        event_bus_adapter_.publish(cataclysm::gui::CharacterTabRequestedEvent(tab.id));
                    }
                }
                if (tab_open) {
                    if (ImGui::BeginTable("CharacterTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        for (size_t j = 0; j < tab.rows.size(); ++j) {
                            const auto& row = tab.rows[j];
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            bool is_selected = row.highlighted || (is_active_tab && (static_cast<int>(j) == state.active_row_index));
                            const bool row_pressed = ImGui::Selectable(row.name.c_str(), is_selected,
                                                                         ImGuiSelectableFlags_SpanAllColumns);
                            RecordRect(row_rects_, tab.id + ":" + std::to_string(j));
                            const InteractiveRect& row_rect = row_rects_.back();
                            const bool row_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                            const bool row_activated = row_pressed || ImGui::IsItemActivated();
                            if (row_clicked || row_activated) {
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
        const bool button_pressed = ImGui::SmallButton(label);
        RecordRect(command_button_rects_, label);
        const InteractiveRect& button_rect = command_button_rects_.back();
        const bool button_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool button_activated = button_pressed || ImGui::IsItemActivated();
        if (button_clicked || button_activated) {
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

    HandleCommandKeys(event_bus_adapter_);
    HandleTabNavigation(state, event_bus_adapter_);

    ImGui::End();
}

void CharacterWidget::RecordRect(std::vector<InteractiveRect>& container, const std::string& id) {
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    container.push_back({id, min, max});
}

bool CharacterWidget::GetTabRect(const std::string& tab_id, ImVec2* min, ImVec2* max) const {
    return FindRect(tab_rects_, tab_id, min, max);
}

bool CharacterWidget::GetRowRect(const std::string& tab_id, size_t row_index, ImVec2* min, ImVec2* max) const {
    return FindRect(row_rects_, tab_id + ":" + std::to_string(row_index), min, max);
}

bool CharacterWidget::GetCommandButtonRect(const std::string& label, ImVec2* min, ImVec2* max) const {
    return FindRect(command_button_rects_, label, min, max);
}

bool CharacterWidget::FindRect(const std::vector<InteractiveRect>& container,
                               const std::string& id,
                               ImVec2* min,
                               ImVec2* max) const {
    if (min == nullptr || max == nullptr) {
        return false;
    }

    for (const auto& rect : container) {
        if (rect.id == id) {
            *min = rect.min;
            *max = rect.max;
            return true;
        }
    }

    return false;
}
