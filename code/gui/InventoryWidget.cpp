#include "InventoryWidget.h"
#include "events.h"
#include "imgui.h"

#include <algorithm>
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

void DrawInventoryColumn(const inventory_column& column,
                         int column_index,
                         int active_column,
                         cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    ImGui::PushID(column_index);

    const bool is_active_column = column_index == active_column;
    StyleColorScope column_color_scope;
    if (is_active_column) {
        column_color_scope.Push(ImGuiCol_ChildBg, ActiveColumnBackgroundColor());
    }

    ImGui::TextUnformatted(column.name.c_str());
    ImGui::Separator();

    constexpr ImGuiWindowFlags kChildFlags = ImGuiWindowFlags_AlwaysUseWindowPadding |
                                             ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("InventoryColumnBody", ImVec2(0, 0), false, kChildFlags);

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

        if (ImGui::Selectable(label.c_str(), row_selected, ImGuiSelectableFlags_SpanAvailWidth)) {
            event_bus_adapter.publish(cataclysm::gui::InventoryItemClickedEvent(entry));
        }

        if (!entry.disabled_msg.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s", entry.disabled_msg.c_str());
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::PopID();
}

}  // namespace

InventoryWidget::InventoryWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter) {}

InventoryWidget::~InventoryWidget() = default;

namespace {
const ImVec4 kFavoriteColor = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
const ImVec4 kDisabledColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);

void DrawInventoryColumn(const inventory_column& column,
                         int column_index,
                         int active_column,
                         cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    ImGui::PushID(column_index);

    const bool is_active_column = column_index == active_column;
    if (is_active_column) {
        const ImVec4 highlight = ImGui::GetStyleColorVec4(ImGuiCol_Header);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(highlight.x * 0.25f, highlight.y * 0.25f, highlight.z * 0.25f, 0.35f));
    }

    ImGui::TextUnformatted(column.name.c_str());
    ImGui::Separator();

    ImGui::BeginChild("InventoryColumnBody", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (column.scroll_position > 0) {
        const float line_height = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetScrollY(column.scroll_position * line_height);
    }

    for (size_t row_index = 0; row_index < column.entries.size(); ++row_index) {
        const auto& entry = column.entries[row_index];
        ImGui::PushID(static_cast<int>(row_index));

        if (entry.is_category) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::SeparatorText(entry.label.c_str());
            ImGui::PopStyleColor();
            ImGui::PopID();
            continue;
        }

        const bool is_highlighted_row = entry.is_highlighted;
        if (is_highlighted_row) {
            const ImVec4 highlight = ImGui::GetStyleColorVec4(ImGuiCol_Header);
            ImGui::PushStyleColor(ImGuiCol_Header, highlight);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, highlight);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, highlight);
        }

        ImVec4 text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if (entry.is_favorite) {
            text_color = kFavoriteColor;
        }
        if (entry.is_disabled) {
            text_color = kDisabledColor;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);

        std::string label = entry.label;
        if (!entry.hotkey.empty()) {
            label = entry.hotkey + " " + label;
        }

        if (ImGui::Selectable(label.c_str(), entry.is_selected, ImGuiSelectableFlags_SpanAvailWidth)) {
            event_bus_adapter.publish(cataclysm::gui::InventoryItemClickedEvent(entry));
        }

        if (!entry.disabled_msg.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("%s", entry.disabled_msg.c_str());
        }

        ImGui::PopStyleColor();

        if (is_highlighted_row) {
            ImGui::PopStyleColor(3);
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    if (is_active_column) {
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}
} // namespace

void InventoryWidget::Draw(const inventory_overlay_state& state) {
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
    if (ImGui::BeginTable("InventoryColumns", 3, table_flags)) {
        for (int column_index = 0; column_index < 3; ++column_index) {
            ImGui::TableNextColumn();
            DrawInventoryColumn(state.columns[column_index], column_index, state.active_column, event_bus_adapter_);
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
}
