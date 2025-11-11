#include "InventoryWidget.h"
#include "events.h"
#include "imgui.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace {

class StyleColorScope {
public:
    StyleColorScope() = default;
    StyleColorScope(const StyleColorScope&) = delete;
    StyleColorScope& operator=(const StyleColorScope&) = delete;

    ~StyleColorScope() {
        if (count_ > 0) {
            ImGui::PopStyleColor(count_);
        }
    }

    void Push(ImGuiCol idx, const ImVec4& color) {
        ImGui::PushStyleColor(idx, color);
        ++count_;
    }

private:
    int count_ = 0;
};

constexpr float kHighlightLightenAmount = 0.1f;
constexpr float kTextLightenAmount = 0.2f;

ImVec4 LightenColor(const ImVec4& color, float amount) {
    return ImVec4(std::min(color.x + amount, 1.0f),
                  std::min(color.y + amount, 1.0f),
                  std::min(color.z + amount, 1.0f),
                  color.w);
}

ImVec4 ActiveColumnBackgroundColor() {
    return LightenColor(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg), kHighlightLightenAmount);
}

const ImVec4 kFavoriteColor = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
const ImVec4 kDisabledColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);

}  // namespace

std::string InventoryWidget::BuildEntryKey(int column_index,
                                           int row_index,
                                           const inventory_entry& entry) {
    return std::to_string(column_index) + ":" + std::to_string(row_index) + ":" +
           entry.hotkey + ":" + entry.label;
}

std::string InventoryWidget::NormalizeHotkeyString(const std::string& hotkey) {
    std::string normalized;
    normalized.reserve(hotkey.size());
    for (char ch : hotkey) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (ch == '-' || ch == '_' || ch == '+' || ch == '.') {
            normalized.push_back(ch);
        }
    }
    return normalized;
}

std::optional<std::string> InventoryWidget::NormalizeKeycode(SDL_Keycode keycode) {
    if (keycode == SDLK_UNKNOWN) {
        return std::nullopt;
    }

    if (keycode >= 32 && keycode <= 126) {
        char key_char = static_cast<char>(keycode);
        key_char = static_cast<char>(std::tolower(static_cast<unsigned char>(key_char)));
        return std::string(1, key_char);
    }

    const char* key_name = SDL_GetKeyName(keycode);
    if (key_name == nullptr || key_name[0] == '\0') {
        return std::nullopt;
    }

    std::string normalized_name;
    for (const char* it = key_name; *it != '\0'; ++it) {
        const unsigned char ch = static_cast<unsigned char>(*it);
        if (std::isalnum(ch)) {
            normalized_name.push_back(static_cast<char>(std::tolower(ch)));
        } else if (*it == '-' || *it == '_' || *it == '+') {
            normalized_name.push_back(*it);
        }
    }

    if (normalized_name == "minus" || normalized_name == "kpminus") {
        return std::string("-");
    }

    if (normalized_name.empty()) {
        return std::nullopt;
    }

    return normalized_name;
}

const InventoryWidget::EntryBounds* InventoryWidget::FindEntryAtPosition(const ImVec2& position) const {
    for (const auto& bounds : last_entry_bounds_) {
        if (position.x >= bounds.min.x && position.x <= bounds.max.x &&
            position.y >= bounds.min.y && position.y <= bounds.max.y) {
            return &bounds;
        }
    }
    return nullptr;
}

bool InventoryWidget::DispatchEntryEvent(const EntryBounds& bounds) {
    if (bounds.entry.is_category || bounds.entry.is_disabled) {
        return false;
    }

    const auto insertion = handled_entries_.insert(bounds.entry_key);
    if (!insertion.second) {
        return true;
    }

    event_bus_adapter_.publish(cataclysm::gui::InventoryItemClickedEvent(bounds.entry));
    return true;
}

bool InventoryWidget::HandleMouseButtonEvent(const SDL_MouseButtonEvent& button_event) {
    if (button_event.button != SDL_BUTTON_LEFT) {
        return false;
    }

    const ImVec2 click_pos(static_cast<float>(button_event.x), static_cast<float>(button_event.y));
    const EntryBounds* bounds = FindEntryAtPosition(click_pos);
    if (bounds == nullptr) {
        return false;
    }

    return DispatchEntryEvent(*bounds);
}

bool InventoryWidget::HandleKeyEvent(const SDL_KeyboardEvent& key_event) {
    if (key_event.repeat != 0) {
        return false;
    }

    const std::optional<std::string> hotkey = NormalizeKeycode(key_event.keysym.sym);
    if (!hotkey.has_value()) {
        return false;
    }

    for (const auto& bounds : last_entry_bounds_) {
        if (bounds.normalized_hotkey.empty()) {
            continue;
        }
        if (bounds.normalized_hotkey == *hotkey) {
            return DispatchEntryEvent(bounds);
        }
    }

    return false;
}

void InventoryWidget::DrawInventoryColumn(const inventory_column& column,
                                          int column_index,
                                          int active_column) {
    ImGui::PushID(column_index);

    const bool is_active_column = column_index == active_column;
    StyleColorScope column_color_scope;
    if (is_active_column) {
        column_color_scope.Push(ImGuiCol_ChildBg, ActiveColumnBackgroundColor());
    }

    ImGui::TextUnformatted(column.name.c_str());
    ImGui::Separator();

    constexpr ImGuiChildFlags kChildFlags = ImGuiChildFlags_AlwaysUseWindowPadding;
    constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("InventoryColumnBody", ImVec2(0, 0), kChildFlags, kWindowFlags);

    if (column.scroll_position > 0) {
        const float line_height = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetScrollY(column.scroll_position * line_height);
    }

    const ImVec4 default_text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    for (size_t row_index = 0; row_index < column.entries.size(); ++row_index) {
        const auto& entry = column.entries[row_index];
        ImGui::PushID(static_cast<int>(row_index));

        if (entry.is_category) {
            StyleColorScope category_color_scope;
            category_color_scope.Push(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::SeparatorText(entry.label.c_str());
            ImGui::PopID();
            continue;
        }

        const bool is_selected = entry.is_selected;
        const bool is_highlighted = entry.is_highlighted;
        const bool row_selected = is_selected || is_highlighted;
        const bool overlap_selected_and_highlighted = is_selected && is_highlighted;
        const bool is_interactable = !entry.is_disabled;

        ImVec4 text_color = default_text_color;
        if (entry.is_favorite) {
            text_color = kFavoriteColor;
        }
        if (entry.is_disabled) {
            text_color = kDisabledColor;
        }
        if (overlap_selected_and_highlighted) {
            text_color = LightenColor(text_color, kTextLightenAmount);
        }

        StyleColorScope row_color_scope;
        if (entry.is_favorite || entry.is_disabled || overlap_selected_and_highlighted) {
            row_color_scope.Push(ImGuiCol_Text, text_color);
        }

        if (is_highlighted) {
            ImVec4 header_color = ImGui::GetStyleColorVec4(ImGuiCol_Header);
            if (overlap_selected_and_highlighted) {
                header_color = LightenColor(header_color, kHighlightLightenAmount);
            }
            row_color_scope.Push(ImGuiCol_Header, header_color);
            row_color_scope.Push(ImGuiCol_HeaderHovered, header_color);
            row_color_scope.Push(ImGuiCol_HeaderActive, header_color);
        }

        std::string label = entry.label;
        if (!entry.hotkey.empty()) {
            label = entry.hotkey + " " + label;
        }

        const bool selectable_pressed = ImGui::Selectable(label.c_str(), row_selected,
                                                          ImGuiSelectableFlags_None);
        if (selectable_pressed && is_interactable) {
            handled_entries_.insert(BuildEntryKey(column_index, static_cast<int>(row_index), entry));
        }

        EntryBounds bounds;
        bounds.entry = entry;
        bounds.min = ImGui::GetItemRectMin();
        bounds.max = ImGui::GetItemRectMax();
        bounds.column_index = column_index;
        bounds.row_index = static_cast<int>(row_index);
        bounds.entry_key = BuildEntryKey(column_index, bounds.row_index, entry);
        bounds.normalized_hotkey = NormalizeHotkeyString(entry.hotkey);
        last_entry_bounds_.push_back(bounds);

        if (!entry.disabled_msg.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s", entry.disabled_msg.c_str());
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::PopID();
}

InventoryWidget::InventoryWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter) {}

InventoryWidget::~InventoryWidget() = default;

void InventoryWidget::Draw(const inventory_overlay_state& state) {
    last_entry_bounds_.clear();
    ImGui::Begin("Inventory");

    // Header
    ImGui::TextUnformatted(state.title.c_str());
    if (!state.hotkey_hint.empty()) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("%s", state.hotkey_hint.c_str());
    }
    if (!state.weight_label.empty()) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(state.weight_label.c_str());
    }
    if (!state.volume_label.empty()) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(state.volume_label.c_str());
    }

    ImGui::Spacing();

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp |
                                            ImGuiTableFlags_BordersInnerV |
                                            ImGuiTableFlags_BordersOuterV;
    const float table_width = ImGui::GetContentRegionAvail().x;
    const float column_width = table_width / 3.0f;
    if (ImGui::BeginTable("InventoryColumns", 3, table_flags)) {
        ImGui::TableSetupColumn("Worn", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableSetupColumn("Inventory", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableSetupColumn("Ground", ImGuiTableColumnFlags_WidthFixed, column_width);
        for (int column_index = 0; column_index < 3; ++column_index) {
            ImGui::TableNextColumn();
            DrawInventoryColumn(state.columns[column_index], column_index, state.active_column);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();

    if (!state.filter_string.empty()) {
        ImGui::TextUnformatted(state.filter_string.c_str());
        if (!state.navigation_mode.empty()) {
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        }
    }
    if (!state.navigation_mode.empty()) {
        ImGui::TextUnformatted(state.navigation_mode.c_str());
    }

    ImGui::End();

    handled_entries_.clear();
}

bool InventoryWidget::HandleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN: {
            return HandleMouseButtonEvent(event.button);
        }
        case SDL_KEYDOWN: {
            return HandleKeyEvent(event.key);
        }
        default:
            return false;
    }
}

bool InventoryWidget::GetEntryRect(const std::string& hotkey,
                                   const std::string& label,
                                   ImVec2* min,
                                   ImVec2* max) const {
    if (min == nullptr || max == nullptr) {
        return false;
    }
    for (const auto& bounds : last_entry_bounds_) {
        if (bounds.entry.hotkey == hotkey && bounds.entry.label == label) {
            *min = bounds.min;
            *max = bounds.max;
            return true;
        }
    }
    return false;
}
