#include "InventoryWidget.h"

#include <utility>

#include "imgui.h"

InventoryWidget::InventoryWidget() {
    mock_inventory_ = {
        {"Sword", 1, ImVec4(0.8f, 0.8f, 0.8f, 1.0f)},
        {"Shield", 1, ImVec4(0.6f, 0.4f, 0.2f, 1.0f)},
        {"Health Potion", 5, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)},
        {"Mana Potion", 3, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)},
    };
}

InventoryWidget::~InventoryWidget() = default;

void InventoryWidget::SetItemCallback(ItemCallback callback) {
    item_callback_ = std::move(callback);
}

void InventoryWidget::Draw() {
    ImGui::Begin("Inventory");

    for (const auto& item : mock_inventory_) {
        ImGui::PushID(&item);
        ImGui::PushStyleColor(ImGuiCol_Text, item.color);

        std::string label = item.name + " (" + std::to_string(item.quantity) + ")";
        if (ImGui::Selectable(label.c_str())) {
            if (item_callback_) {
                item_callback_(item, ItemAction::Click);
            }
        }

        if (ImGui::IsItemHovered()) {
            if (item_callback_) {
                item_callback_(item, ItemAction::Hover);
            }
        }

        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    ImGui::End();
}
