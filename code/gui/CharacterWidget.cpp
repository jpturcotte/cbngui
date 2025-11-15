#include "CharacterWidget.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "events.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace {
constexpr ImVec2 kTopGridSize(240.0f, 180.0f);

int AdjustActiveRowIndex(const character_overlay_tab& tab, int active_row_index) {
    if (active_row_index < 0) {
        return active_row_index;
    }

    if (tab.id == "bionics") {
        const int adjusted_index = active_row_index + 1;
        if (adjusted_index >= static_cast<int>(tab.rows.size())) {
            return -1;
        }
        return adjusted_index;
    }

    return active_row_index;
}

int AdjustRowEventIndex(const character_overlay_tab& tab, int row_index) {
    if (tab.id == "bionics") {
        return row_index - 1;
    }

    return row_index;
}

bool IsCharacterWindowFocused() {
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows |
                                  ImGuiFocusedFlags_NoPopupHierarchy);
}

std::string TrimCopy(const std::string& value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return std::string();
    }
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

std::string ToUpperCopy(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return result;
}

std::vector<std::string> SplitBindingTokens(const std::string& binding) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : binding) {
        if (ch == '+') {
            tokens.push_back(TrimCopy(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    tokens.push_back(TrimCopy(current));
    return tokens;
}

SDL_Keycode LookupKeycode(const std::string& token) {
    if (token.empty()) {
        return SDLK_UNKNOWN;
    }

    SDL_Keycode keycode = SDL_GetKeyFromName(token.c_str());
    if (keycode != SDLK_UNKNOWN) {
        return keycode;
    }

    const std::string upper = ToUpperCopy(token);
    if (upper == "ESC" || upper == "ESCAPE") {
        return SDLK_ESCAPE;
    }
    if (upper == "ENTER" || upper == "RETURN") {
        return SDLK_RETURN;
    }
    if (upper == "SPACE" || upper == "SPACEBAR") {
        return SDLK_SPACE;
    }
    if (upper == "DEL" || upper == "DELETE") {
        return SDLK_DELETE;
    }
    if (upper == "PGUP" || upper == "PAGEUP") {
        return SDLK_PAGEUP;
    }
    if (upper == "PGDN" || upper == "PAGEDOWN") {
        return SDLK_PAGEDOWN;
    }

    if (token.size() == 1) {
        return static_cast<SDL_Keycode>(static_cast<unsigned char>(token.front()));
    }

    return SDLK_UNKNOWN;
}

struct ParsedBinding {
    bool require_shift = false;
    bool require_ctrl = false;
    bool require_alt = false;
    bool require_gui = false;
    SDL_Keycode keycode = SDLK_UNKNOWN;
};

ParsedBinding ParseBinding(const std::string& binding) {
    ParsedBinding parsed;
    if (binding.empty()) {
        return parsed;
    }

    const auto tokens = SplitBindingTokens(binding);
    for (const auto& token : tokens) {
        if (token.empty()) {
            continue;
        }
        const std::string upper = ToUpperCopy(token);
        if (upper == "SHIFT") {
            parsed.require_shift = true;
            continue;
        }
        if (upper == "CTRL" || upper == "CONTROL" || upper == "CTL") {
            parsed.require_ctrl = true;
            continue;
        }
        if (upper == "ALT") {
            parsed.require_alt = true;
            continue;
        }
        if (upper == "GUI" || upper == "META" || upper == "WIN" || upper == "SUPER") {
            parsed.require_gui = true;
            continue;
        }

        if (parsed.keycode == SDLK_UNKNOWN) {
            parsed.keycode = LookupKeycode(token);
        }
    }

    if (parsed.keycode == SDLK_UNKNOWN) {
        parsed.keycode = LookupKeycode(binding);
    }

    return parsed;
}

bool IsPrintableKey(SDL_Keycode keycode) {
    const int value = static_cast<int>(keycode);
    return value >= 32 && value <= 126;
}

bool MatchesBinding(const SDL_KeyboardEvent& key_event, const ParsedBinding& binding) {
    if (binding.keycode == SDLK_UNKNOWN) {
        return false;
    }

    if (key_event.keysym.sym != binding.keycode) {
        return false;
    }

    const SDL_Keymod mods = static_cast<SDL_Keymod>(key_event.keysym.mod);
    const bool shift_down = (mods & KMOD_SHIFT) != 0;
    const bool ctrl_down = (mods & KMOD_CTRL) != 0;
    const bool alt_down = (mods & KMOD_ALT) != 0;
    const bool gui_down = (mods & KMOD_GUI) != 0;

    if (binding.require_shift && !shift_down) {
        return false;
    }
    if (!binding.require_shift && shift_down && !IsPrintableKey(binding.keycode)) {
        return false;
    }
    if (binding.require_ctrl != ctrl_down) {
        return false;
    }
    if (binding.require_alt != alt_down) {
        return false;
    }
    if (binding.require_gui != gui_down) {
        return false;
    }

    return true;
}

bool MatchesBinding(const SDL_KeyboardEvent& key_event, const std::string& binding) {
    if (binding.empty()) {
        return false;
    }

    return MatchesBinding(key_event, ParseBinding(binding));
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
    const std::array<std::string_view, 3> kTopTabIds = {"stats", "encumbrance", "speed"};
    std::array<int, 3> top_tab_indices;
    top_tab_indices.fill(-1);

    for (size_t top_index = 0; top_index < kTopTabIds.size(); ++top_index) {
        const auto it = std::find_if(state.tabs.begin(), state.tabs.end(),
                                     [&](const character_overlay_tab& tab) {
                                         return tab.id == kTopTabIds[top_index];
                                     });
        if (it != state.tabs.end()) {
            top_tab_indices[top_index] = static_cast<int>(std::distance(state.tabs.begin(), it));
        }
    }

    auto draw_grid = [&](const character_overlay_tab& tab, int raw_active_row_index) {
        const int active_row_index = AdjustActiveRowIndex(tab, raw_active_row_index);
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

    auto is_top_tab_index = [&](int index) {
        return std::find(top_tab_indices.begin(), top_tab_indices.end(), index) != top_tab_indices.end();
    };

    auto draw_top_grid_if_present = [&](int tab_index) {
        if (tab_index < 0 || tab_index >= static_cast<int>(state.tabs.size())) {
            return;
        }
        const auto& tab = state.tabs[tab_index];
        const int active_row_index = state.active_tab_index == tab_index ? state.active_row_index : -1;
        draw_grid(tab, active_row_index);
    };

    if (ImGui::BeginTable("TopGrids", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        draw_top_grid_if_present(top_tab_indices[0]);
        ImGui::TableNextColumn();
        draw_top_grid_if_present(top_tab_indices[1]);
        ImGui::TableNextColumn();
        draw_top_grid_if_present(top_tab_indices[2]);
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
            for (size_t i = 0; i < state.tabs.size(); ++i) {
                if (is_top_tab_index(static_cast<int>(i))) {
                    continue;
                }
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
                const bool tab_mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
                const ImVec2 mouse_pos = ImGui::GetMousePos();
                const bool within_tab = mouse_pos.x >= tab_min.x && mouse_pos.x <= tab_max.x &&
                                        mouse_pos.y >= tab_min.y && mouse_pos.y <= tab_max.y;
                const bool tab_clicked_by_bounds = tab_mouse_released && within_tab;
                if (!is_active_tab && (tab_clicked || tab_activated || tab_clicked_by_bounds)) {
                    event_bus_adapter_.publish(cataclysm::gui::CharacterTabRequestedEvent(tab.id));
                }
                if (tab_open) {
                    const int active_row_index =
                        is_active_tab ? AdjustActiveRowIndex(tab, state.active_row_index) : -1;
                    if (ImGui::BeginTable("CharacterTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        for (size_t j = 0; j < tab.rows.size(); ++j) {
                            const auto& row = tab.rows[j];
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            bool is_selected = row.highlighted || (static_cast<int>(j) == active_row_index);
                            const bool row_pressed = ImGui::Selectable(row.name.c_str(), is_selected,
                                                                         ImGuiSelectableFlags_SpanAllColumns);
                            RecordRect(row_rects_, tab.id + ":" + std::to_string(j));
                            const InteractiveRect& row_rect = row_rects_.back();
                            const bool row_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
                            const bool row_activated = row_pressed || ImGui::IsItemActivated();
                            const bool row_mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
                            const ImVec2 row_mouse_pos = ImGui::GetMousePos();
                            const bool row_bounds_clicked =
                                row_mouse_released &&
                                row_mouse_pos.x >= row_rect.min.x && row_mouse_pos.x <= row_rect.max.x &&
                                row_mouse_pos.y >= row_rect.min.y && row_mouse_pos.y <= row_rect.max.y;
                            if (row_clicked || row_activated || row_bounds_clicked) {
                                const int event_row_index = AdjustRowEventIndex(tab, static_cast<int>(j));
                                if (event_row_index >= 0) {
                                    event_bus_adapter_.publish(
                                        cataclysm::gui::CharacterRowActivatedEvent(tab.id, event_row_index));
                                }
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
        const bool button_mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        const ImVec2 button_mouse_pos = ImGui::GetMousePos();
        const bool button_bounds_clicked =
            button_mouse_released &&
            button_mouse_pos.x >= button_rect.min.x && button_mouse_pos.x <= button_rect.max.x &&
            button_mouse_pos.y >= button_rect.min.y && button_mouse_pos.y <= button_rect.max.y;
        if (button_clicked || button_activated || button_bounds_clicked) {
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

bool CharacterWidget::HandleEvent(const SDL_Event& event, const character_overlay_state& state) {
    if (event.type != SDL_KEYDOWN) {
        return false;
    }

    if (!IsCharacterWindowFocused()) {
        return false;
    }

    const SDL_KeyboardEvent& key_event = event.key;
    if (key_event.repeat != 0) {
        return false;
    }

    auto publish_command = [&](cataclysm::gui::CharacterCommand command) {
        event_bus_adapter_.publish(cataclysm::gui::CharacterCommandEvent(command));
    };

    if (MatchesBinding(key_event, state.bindings.quit)) {
        publish_command(cataclysm::gui::CharacterCommand::QUIT);
        return true;
    }

    if (MatchesBinding(key_event, state.bindings.confirm)) {
        publish_command(cataclysm::gui::CharacterCommand::CONFIRM);
        return true;
    }

    if (MatchesBinding(key_event, state.bindings.rename)) {
        publish_command(cataclysm::gui::CharacterCommand::RENAME);
        return true;
    }

    if (MatchesBinding(key_event, state.bindings.help)) {
        publish_command(cataclysm::gui::CharacterCommand::HELP);
        return true;
    }

    if (state.tabs.empty()) {
        return false;
    }

    const int tab_count = static_cast<int>(state.tabs.size());
    const int active_index = std::clamp(state.active_tab_index, 0, tab_count - 1);

    if (MatchesBinding(key_event, state.bindings.back_tab)) {
        const int target_index = (active_index - 1 + tab_count) % tab_count;
        if (target_index != active_index) {
            event_bus_adapter_.publish(
                cataclysm::gui::CharacterTabRequestedEvent(state.tabs[target_index].id));
        }
        return true;
    }

    if (MatchesBinding(key_event, state.bindings.tab)) {
        const int target_index = (active_index + 1) % tab_count;
        if (target_index != active_index) {
            event_bus_adapter_.publish(
                cataclysm::gui::CharacterTabRequestedEvent(state.tabs[target_index].id));
        }
        return true;
    }

    return false;
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
