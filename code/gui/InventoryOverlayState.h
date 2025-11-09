#ifndef INVENTORY_OVERLAY_STATE_H
#define INVENTORY_OVERLAY_STATE_H

#include <array>
#include <string>
#include <vector>

struct inventory_entry {
    std::string label;
    std::string hotkey;
    bool is_category;
    bool is_selected;
    bool is_highlighted;
    bool is_favorite;
    bool is_disabled;
    std::string disabled_msg;
};

struct inventory_column {
    std::string name;
    int scroll_position;
    std::vector<inventory_entry> entries;
};

struct inventory_overlay_state {
    std::string title;
    std::string hotkey_hint;
    std::string weight_label;
    std::string volume_label;
    std::string filter_string;
    std::string navigation_mode;
    std::array<inventory_column, 3> columns;
    int active_column;
};

#endif // INVENTORY_OVERLAY_STATE_H
