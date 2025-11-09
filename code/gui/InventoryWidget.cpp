#include "InventoryWidget.h"
#include "events.h"
#include "imgui.h"

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

        size_t max_entries = 0;
        for (const auto& col : state.columns) {
            if (col.entries.size() > max_entries) {
                max_entries = col.entries.size();
            }
        }

        for (size_t i = 0; i < max_entries; ++i) {
            ImGui::TableNextRow();
            for (int j = 0; j < 3; ++j) {
                ImGui::TableSetColumnIndex(j);
                if (i < state.columns[j].entries.size()) {
                    const auto& entry = state.columns[j].entries[i];
                    if (entry.is_category) {
                        ImGui::SeparatorText(entry.label.c_str());
                    } else {
                        ImVec4 color(1.0f, 1.0f, 1.0f, 1.0f);
                        if (entry.is_favorite) {
                            color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                        }
                        if (entry.is_disabled) {
                            color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                        }

                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                        std::string label = entry.hotkey + " " + entry.label;
                        if (ImGui::Selectable(label.c_str(), entry.is_selected)) {
                            event_bus_adapter_.publish(cataclysm::gui::InventoryItemClickedEvent(entry));
                        }
                        ImGui::PopStyleColor();
                    }
                }
            }
        }
        ImGui::EndTable();
    }

    // Footer
    ImGui::TextUnformatted(state.filter_string.c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(state.navigation_mode.c_str());

    ImGui::End();
}
