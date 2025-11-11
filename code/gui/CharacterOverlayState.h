#ifndef CHARACTER_OVERLAY_STATE_H
#define CHARACTER_OVERLAY_STATE_H

#include <string>
#include <vector>
#include "imgui.h"

// Corresponds to cbn_character_overlay_column_entry in sdltiles.h
struct character_overlay_column_entry {
    std::string name;
    std::string value;
    std::string tooltip;
    ImU32 color;
    bool highlighted = false;
};

// Corresponds to cbn_character_overlay_tab in sdltiles.h
struct character_overlay_tab {
    std::string id;
    std::string title;
    std::vector<character_overlay_column_entry> rows;
};

// Corresponds to cbn_input_bindings in sdltiles.h
struct character_input_bindings {
    std::string help;
    std::string tab;
    std::string back_tab;
    std::string confirm;
    std::string quit;
    std::string rename;
};

// Corresponds to cbn_character_overlay_state in sdltiles.h
struct character_overlay_state {
    std::string header_left;
    std::string header_right;
    std::string info_panel_text;
    std::vector<character_overlay_tab> tabs;
    int active_tab_index = 0;
    int active_row_index = 0;
    std::vector<std::string> footer_lines;
    character_input_bindings bindings;
};

#endif // CHARACTER_OVERLAY_STATE_H
