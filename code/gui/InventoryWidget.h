#ifndef INVENTORY_WIDGET_H
#define INVENTORY_WIDGET_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "imgui.h"

struct InventoryItem {
    std::string name;
    int quantity;
    ImVec4 color;
};

enum class ItemAction {
    Hover,
    Click
};

class InventoryWidget {
public:
    InventoryWidget();
    ~InventoryWidget();

    void Draw();

    using ItemCallback = std::function<void(const InventoryItem& item, ItemAction action)>;
    void SetItemCallback(ItemCallback callback);

private:
    std::vector<InventoryItem> mock_inventory_;
    ItemCallback item_callback_;
};

#endif // INVENTORY_WIDGET_H
