#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>

#include "CharacterOverlayState.h"
#include "CharacterWidget.h"
#include "InventoryOverlayState.h"
#include "InventoryWidget.h"
#include "event_bus_adapter.h"
#include "event_bus.h"
#include "events.h"
#include "imgui.h"
#include "map_widget.h"

namespace {

struct EventRecorder {
    bool map_tile_hovered = false;
    bool map_tile_clicked = false;
    bool inventory_item_clicked = false;
    bool character_tab_requested = false;
    bool character_row_activated = false;

    int hovered_x = -1;
    int hovered_y = -1;
    int clicked_x = -1;
    int clicked_y = -1;
    std::string last_tab_id;
    size_t last_row_index = 0;
    inventory_entry last_inventory_entry;
};

void DrawMapWidget(MapWidget &map_widget) {
    ImGui::NewFrame();
    map_widget.Draw();
    ImGui::Render();
}

void DrawInventoryWidget(InventoryWidget &inventory_widget, const inventory_overlay_state &state) {
    ImGui::NewFrame();
    inventory_widget.Draw(state);
    ImGui::Render();
}

void DrawCharacterWidget(CharacterWidget &character_widget, const character_overlay_state &state) {
    ImGui::NewFrame();
    character_widget.Draw(state);
    ImGui::Render();
}

inventory_overlay_state BuildMockInventoryState() {
    inventory_overlay_state state;
    state.title = "Inventory";
    state.hotkey_hint = "[i] to close";
    state.weight_label = "Weight: 10/100";
    state.volume_label = "Volume: 8/100";
    state.filter_string = "Filter: none";
    state.navigation_mode = "Item mode";
    state.active_column = 1;

    state.columns[0].name = "Worn";
    state.columns[0].scroll_position = 0;
    state.columns[0].entries = {
        { "Clothing", "", true, false, false, false, false, "" },
        { "Backpack", "a", false, true, true, false, false, "" },
        { "Jeans", "b", false, false, false, false, false, "" }
    };

    state.columns[1].name = "Inventory";
    state.columns[1].scroll_position = 1;
    state.columns[1].entries = {
        { "Food", "", true, false, false, false, false, "" },
        { "Water", "c", false, false, true, false, false, "" },
        { "Can of Beans", "d", false, false, false, false, false, "" },
        { "First Aid", "", true, false, false, false, false, "" },
        { "Bandage", "e", false, true, false, false, false, "" },
        { "Aspirin", "f", false, false, false, false, true, "Too weak" }
    };

    state.columns[2].name = "Ground";
    state.columns[2].scroll_position = 0;
    state.columns[2].entries = {
        { "Rocks", "g", false, false, false, false, false, "" }
    };

    return state;
}

character_overlay_state BuildMockCharacterState() {
    character_overlay_state state;
    state.header_left = "Player Name - Survivor";
    state.header_right = "[?] Help";
    state.info_panel_text = "This is the info panel.\nIt can span multiple lines.";
    state.active_tab_index = 3;
    state.active_row_index = 1;
    state.footer_lines = { "This is a footer line.", "And another one." };
    state.bindings = { "?", "TAB", "SHIFT+TAB", "ENTER", "ESC", "r" };

    state.tabs.push_back({
        "stats",
        "Stats",
        {
            { "Strength", "10", "Affects melee damage.", IM_COL32(255, 255, 255, 255), false },
            { "Dexterity", "8", "Affects dodge chance.", IM_COL32(255, 255, 255, 255), false },
            { "Intelligence", "9", "Affects skill gain.", IM_COL32(255, 255, 255, 255), false },
            { "Perception", "7", "Affects ranged accuracy.", IM_COL32(255, 255, 255, 255), false }
        }
    });

    state.tabs.push_back({
        "encumbrance",
        "Encumbrance",
        {
            { "Head", "0", "", IM_COL32(255, 255, 255, 255), false },
            { "Torso", "5", "", IM_COL32(255, 255, 0, 255), false },
            { "L Arm", "2", "", IM_COL32(0, 255, 255, 255), false },
            { "R Arm", "2", "", IM_COL32(0, 255, 255, 255), false }
        }
    });

    state.tabs.push_back({
        "speed",
        "Speed",
        {
            { "Base", "100", "", IM_COL32(255, 255, 255, 255), false },
            { "Pain", "-10", "", IM_COL32(255, 0, 0, 255), false },
            { "Total", "90", "", IM_COL32(255, 255, 255, 255), false }
        }
    });

    state.tabs.push_back({
        "skills",
        "Skills",
        {
            { "Melee", "3", "Skill in hand-to-hand combat.", IM_COL32(255, 255, 255, 255), false },
            { "Marksmanship", "2", "Skill with ranged weapons.", IM_COL32(255, 255, 255, 255), true },
            { "Computers", "1", "Skill with computers.", IM_COL32(255, 255, 255, 255), false }
        }
    });

    state.tabs.push_back({
        "traits",
        "Traits",
        {
            { "Tough", "", "You are tougher than normal.", IM_COL32(0, 255, 0, 255), false },
            { "Fast Learner", "", "You learn skills faster.", IM_COL32(0, 255, 0, 255), false }
        }
    });

    return state;
}

void RunMapWidgetTest(cataclysm::gui::EventBusAdapter &adapter, EventRecorder &recorder) {
    MapWidget map_widget(adapter);
    map_widget.UpdateMapTexture(reinterpret_cast<SDL_Texture*>(0x1), 480, 480, 24, 24);

    DrawMapWidget(map_widget);

    assert(!map_widget.GetSelectedTile().has_value());

    adapter.publishMapTileHovered(12, 6);
    adapter.publishMapTileClicked(3, 4);

    assert(recorder.map_tile_hovered);
    assert(recorder.map_tile_clicked);
    assert(recorder.hovered_x == 12 && recorder.hovered_y == 6);
    assert(recorder.clicked_x == 3 && recorder.clicked_y == 4);
}

void RunInventoryWidgetTest(cataclysm::gui::EventBusAdapter &adapter, EventRecorder &recorder) {
    InventoryWidget inventory_widget(adapter);
    auto state = BuildMockInventoryState();

    DrawInventoryWidget(inventory_widget, state);

    adapter.publish(cataclysm::gui::InventoryItemClickedEvent(state.columns[1].entries[1]));

    assert(recorder.inventory_item_clicked);
    assert(recorder.last_inventory_entry.label == "Water");
    assert(recorder.last_inventory_entry.hotkey == "c");
}

void RunCharacterWidgetTest(cataclysm::gui::EventBusAdapter &adapter, EventRecorder &recorder) {
    CharacterWidget character_widget(adapter);
    auto state = BuildMockCharacterState();

    DrawCharacterWidget(character_widget, state);

    adapter.publish(cataclysm::gui::CharacterTabRequestedEvent("skills"));
    adapter.publish(cataclysm::gui::CharacterRowActivatedEvent("skills", 2));

    assert(recorder.character_tab_requested);
    assert(recorder.last_tab_id == "skills");
    assert(recorder.character_row_activated);
    assert(recorder.last_row_index == 2);
}

}  // namespace

int main() {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1024.0f, 768.0f);
    io.DeltaTime = 1.0f / 60.0f;

    cataclysm::gui::EventBus event_bus;
    cataclysm::gui::EventBusAdapter adapter(event_bus);
    adapter.initialize();

    EventRecorder recorder;

    auto hover_sub = event_bus.subscribe<cataclysm::gui::MapTileHoveredEvent>(
        [&recorder](const cataclysm::gui::MapTileHoveredEvent &event) {
            recorder.map_tile_hovered = true;
            recorder.hovered_x = event.getX();
            recorder.hovered_y = event.getY();
        });

    auto click_sub = event_bus.subscribe<cataclysm::gui::MapTileClickedEvent>(
        [&recorder](const cataclysm::gui::MapTileClickedEvent &event) {
            recorder.map_tile_clicked = true;
            recorder.clicked_x = event.getX();
            recorder.clicked_y = event.getY();
        });

    auto inventory_sub = event_bus.subscribe<cataclysm::gui::InventoryItemClickedEvent>(
        [&recorder](const cataclysm::gui::InventoryItemClickedEvent &event) {
            recorder.inventory_item_clicked = true;
            recorder.last_inventory_entry = event.getEntry();
        });

    auto tab_sub = event_bus.subscribe<cataclysm::gui::CharacterTabRequestedEvent>(
        [&recorder](const cataclysm::gui::CharacterTabRequestedEvent &event) {
            recorder.character_tab_requested = true;
            recorder.last_tab_id = event.getTabId();
        });

    auto row_sub = event_bus.subscribe<cataclysm::gui::CharacterRowActivatedEvent>(
        [&recorder](const cataclysm::gui::CharacterRowActivatedEvent &event) {
            recorder.character_row_activated = true;
            recorder.last_tab_id = event.getTabId();
            recorder.last_row_index = event.getRowIndex();
        });

    RunMapWidgetTest(adapter, recorder);
    RunInventoryWidgetTest(adapter, recorder);
    RunCharacterWidgetTest(adapter, recorder);

    adapter.shutdown();

    hover_sub->unsubscribe();
    click_sub->unsubscribe();
    inventory_sub->unsubscribe();
    tab_sub->unsubscribe();
    row_sub->unsubscribe();

    ImGui::DestroyContext();

    return 0;
}

