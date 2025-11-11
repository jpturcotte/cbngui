#include "InventoryWidget.h"
#include "events.h"
#include "imgui.h"

#include <algorithm>
#include <string>

namespace {

ImVec4 LightenColor(const ImVec4& color, float amount) {
    return ImVec4(std::min(color.x + amount, 1.0f), std::min(color.y + amount, 1.0f),
                  std::min(color.z + amount, 1.0f), color.w);
}

ImVec4 ActiveColumnBackgroundColor() {
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
    color.w = std::max(color.w, 0.25f);
    return color;
}

void DrawInventoryColumn(const inventory_column& column, bool is_active_column, int column_index,
                         cataclysm::gui::EventBusAdapter& event_bus_adapter) {
    ImGui::PushID(column_index);

    if (is_active_column) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ActiveColumnBackgroundColor());
    }

    constexpr ImGuiWindowFlags kChildFlags = ImGuiWindowFlags_AlwaysUseWindowPadding;
    const ImVec2 child_size(0.0f, 0.0f);
    ImGui::BeginChild("InventoryColumn", child_size, false, kChildFlags);

    const ImVec4 default_text_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    for (size_t entry_index = 0; entry_index < column.entries.size(); ++entry_index) {
        const auto& entry = column.entries[entry_index];
        if (entry.is_category) {
            ImGui::SeparatorText(entry.label.c_str());
            continue;
        }

        std::string label = entry.hotkey.empty() ? entry.label : entry.hotkey + " " + entry.label;
        const bool row_selected = entry.is_selected || entry.is_highlighted;

        ImVec4 text_color = default_text_color;
        bool use_custom_text_color = false;
        if (entry.is_favorite) {
            text_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            use_custom_text_color = true;
        }
        if (entry.is_disabled) {
            text_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            use_custom_text_color = true;
        }
        if (entry.is_selected && entry.is_highlighted) {
            text_color = LightenColor(text_color, 0.2f);
            use_custom_text_color = true;
        }

        ImGui::PushID(static_cast<int>(entry_index));
        if (use_custom_text_color) {
            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        }

        const float full_width = ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable(label.c_str(), row_selected, ImGuiSelectableFlags_None,
                              ImVec2(full_width, 0.0f))) {
            event_bus_adapter.publish(cataclysm::gui::InventoryItemClickedEvent(entry));
        }

        if (use_custom_text_color) {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();

    if (is_active_column) {
        ImGui::PopStyleColor();
    }

    ImGui::PopID();
}

}  // namespace

InventoryWidget::InventoryWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter) {}

InventoryWidget::~InventoryWidget() = default;

void InventoryWidget::Draw(const inventory_overlay_state& state) {
    ImGui::Begin("Inventory");

    // Header
    ImGui::TextUnformatted(state.title.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(state.hotkey_hint.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(state.weight_label.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(state.volume_label.c_str());

    // Table
    if (ImGui::BeginTable("InventoryTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn(state.columns[0].name.c_str());
        ImGui::TableSetupColumn(state.columns[1].name.c_str());
        ImGui::TableSetupColumn(state.columns[2].name.c_str());
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        for (int column_index = 0; column_index < 3; ++column_index) {
            ImGui::TableSetColumnIndex(column_index);
            const bool is_active_column = state.active_column == column_index;
            DrawInventoryColumn(state.columns[column_index], is_active_column, column_index, event_bus_adapter_);
        }

        ImGui::EndTable();
    }

    // Footer
    ImGui::TextUnformatted(state.filter_string.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(state.navigation_mode.c_str());

    ImGui::End();
}
