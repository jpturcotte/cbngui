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
#include "overlay_ui.h"

namespace {

struct EventRecorder {
    bool map_tile_hovered = false;
    bool map_tile_clicked = false;
    bool inventory_item_clicked = false;
    bool character_tab_requested = false;
    bool character_row_activated = false;
    bool character_command_received = false;
    bool overlay_open_received = false;
    bool overlay_close_received = false;
    bool filter_applied = false;
    bool item_selected = false;
    bool data_binding_updated = false;

    int hovered_x = -1;
    int hovered_y = -1;
    int clicked_x = -1;
    int clicked_y = -1;
    std::string last_tab_id;
    size_t last_row_index = 0;
    cataclysm::gui::CharacterCommand last_character_command = cataclysm::gui::CharacterCommand::HELP;
    inventory_entry last_inventory_entry;
    std::string last_overlay_id;
    bool last_overlay_modal = false;
    bool last_overlay_cancelled = false;
    std::string last_filter_text;
    std::string last_filter_target;
    bool last_filter_case_sensitive = false;
    std::string last_item_id;
    std::string last_item_source;
    bool last_item_double_click = false;
    int last_item_count = 0;
    std::string last_binding_id;
    std::string last_binding_source;
    bool last_binding_forced = false;
};

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

ImVec2 RectCenter(const ImVec2& min, const ImVec2& max) {
    return ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
}

void RenderOverlayFrame(OverlayUI& overlay_ui,
                        const inventory_overlay_state& inventory_state,
                        const character_overlay_state& character_state) {
    const ImVec2 map_pos(20.0f, 20.0f);
    const ImVec2 map_size(480.0f, 480.0f);
    const ImVec2 inventory_pos(520.0f, 20.0f);
    const ImVec2 inventory_size(420.0f, 540.0f);
    const ImVec2 character_pos(20.0f, 520.0f);
    const ImVec2 character_size(920.0f, 540.0f);

    ImGui::SetNextWindowPos(map_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(map_size, ImGuiCond_Always);
    overlay_ui.Draw();

    ImGui::SetNextWindowPos(inventory_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(inventory_size, ImGuiCond_Always);
    overlay_ui.DrawInventory(inventory_state);

    ImGui::SetNextWindowPos(character_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(character_size, ImGuiCond_Always);
    overlay_ui.DrawCharacter(character_state);
}

void RenderFrame(ImGuiIO& io,
                 OverlayUI& overlay_ui,
                 const inventory_overlay_state& inventory_state,
                 const character_overlay_state& character_state,
                 const ImVec2& mouse_pos,
                 bool mouse_down) {
    io.MousePos = mouse_pos;
    io.MouseDown[0] = mouse_down;
    io.MouseWheel = 0.0f;
    ImGui::NewFrame();
    RenderOverlayFrame(overlay_ui, inventory_state, character_state);
    ImGui::Render();
}

void RunVisualInteractionTest(ImGuiIO& io,
                              OverlayUI& overlay_ui,
                              const inventory_overlay_state& inventory_state,
                              const character_overlay_state& character_state,
                              EventRecorder& recorder) {
    RenderFrame(io, overlay_ui, inventory_state, character_state, ImVec2(-1000.0f, -1000.0f), false);

    ImVec2 map_min, map_max;
    assert(overlay_ui.GetMapWidget().GetLastImageRect(&map_min, &map_max));
    const ImVec2 map_target = RectCenter(map_min, map_max);

    ImVec2 item_min, item_max;
    assert(overlay_ui.GetInventoryWidget().GetEntryRect("c", "Water", &item_min, &item_max));
    const ImVec2 item_target = RectCenter(item_min, item_max);

    ImVec2 tab_min, tab_max;
    assert(overlay_ui.GetCharacterWidget().GetTabRect("traits", &tab_min, &tab_max));
    const ImVec2 tab_target = RectCenter(tab_min, tab_max);

    ImVec2 row_min, row_max;
    assert(overlay_ui.GetCharacterWidget().GetRowRect("skills", 1, &row_min, &row_max));
    const ImVec2 row_target = RectCenter(row_min, row_max);

    ImVec2 button_min, button_max;
    assert(overlay_ui.GetCharacterWidget().GetCommandButtonRect("Confirm", &button_min, &button_max));
    const ImVec2 button_target = RectCenter(button_min, button_max);

    RenderFrame(io, overlay_ui, inventory_state, character_state, map_target, false);
    RenderFrame(io, overlay_ui, inventory_state, character_state, map_target, true);
    RenderFrame(io, overlay_ui, inventory_state, character_state, map_target, false);

    assert(recorder.map_tile_hovered);
    assert(recorder.map_tile_clicked);
    assert(recorder.hovered_x >= 0 && recorder.hovered_y >= 0);
    assert(recorder.clicked_x >= 0 && recorder.clicked_y >= 0);

    RenderFrame(io, overlay_ui, inventory_state, character_state, item_target, true);
    RenderFrame(io, overlay_ui, inventory_state, character_state, item_target, false);

    assert(recorder.inventory_item_clicked);
    assert(recorder.last_inventory_entry.label == "Water");
    assert(recorder.last_inventory_entry.hotkey == "c");

    RenderFrame(io, overlay_ui, inventory_state, character_state, tab_target, true);
    RenderFrame(io, overlay_ui, inventory_state, character_state, tab_target, false);

    assert(recorder.character_tab_requested);
    assert(recorder.last_tab_id == "traits");

    RenderFrame(io, overlay_ui, inventory_state, character_state, row_target, true);
    RenderFrame(io, overlay_ui, inventory_state, character_state, row_target, false);

    assert(recorder.character_row_activated);
    assert(recorder.last_tab_id == "skills");
    assert(recorder.last_row_index == 1);

    RenderFrame(io, overlay_ui, inventory_state, character_state, button_target, true);
    RenderFrame(io, overlay_ui, inventory_state, character_state, button_target, false);

    assert(recorder.character_command_received);
    assert(recorder.last_character_command == cataclysm::gui::CharacterCommand::CONFIRM);
}

void RunOverlayLifecycleTest(cataclysm::gui::EventBusAdapter &adapter, EventRecorder &recorder) {
    auto stats_before = adapter.getStatistics();
    const int published_before = stats_before["events_published"];

    adapter.publishOverlayOpen("inventory_overlay", true);
    adapter.publishOverlayClose("inventory_overlay", false);
    adapter.publishFilterApplied("water", "inventory_panel", true);
    adapter.publishItemSelected("bandage", "inventory_panel", true, 3);
    adapter.publishDataBindingUpdate("player_health", "status_panel", true);

    assert(recorder.overlay_open_received);
    assert(recorder.last_overlay_id == "inventory_overlay");
    assert(recorder.last_overlay_modal);

    assert(recorder.overlay_close_received);
    assert(!recorder.last_overlay_cancelled);

    assert(recorder.filter_applied);
    assert(recorder.last_filter_text == "water");
    assert(recorder.last_filter_target == "inventory_panel");
    assert(recorder.last_filter_case_sensitive);

    assert(recorder.item_selected);
    assert(recorder.last_item_id == "bandage");
    assert(recorder.last_item_source == "inventory_panel");
    assert(recorder.last_item_double_click);
    assert(recorder.last_item_count == 3);

    assert(recorder.data_binding_updated);
    assert(recorder.last_binding_id == "player_health");
    assert(recorder.last_binding_source == "status_panel");
    assert(recorder.last_binding_forced);

    auto stats_after = adapter.getStatistics();
    const int published_after = stats_after["events_published"];
    assert(published_after >= published_before + 5);
}

}  // namespace

int main() {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1024.0f, 768.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    unsigned char* font_pixels = nullptr;
    int font_width = 0;
    int font_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

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

    auto command_sub = event_bus.subscribe<cataclysm::gui::CharacterCommandEvent>(
        [&recorder](const cataclysm::gui::CharacterCommandEvent &event) {
            recorder.character_command_received = true;
            recorder.last_character_command = event.getCommand();
        });

    auto overlay_open_sub = event_bus.subscribe<cataclysm::gui::UiOverlayOpenEvent>(
        [&recorder](const cataclysm::gui::UiOverlayOpenEvent &event) {
            recorder.overlay_open_received = true;
            recorder.last_overlay_id = event.getOverlayId();
            recorder.last_overlay_modal = event.isModal();
        });

    auto overlay_close_sub = event_bus.subscribe<cataclysm::gui::UiOverlayCloseEvent>(
        [&recorder](const cataclysm::gui::UiOverlayCloseEvent &event) {
            recorder.overlay_close_received = true;
            recorder.last_overlay_id = event.getOverlayId();
            recorder.last_overlay_cancelled = event.wasCancelled();
        });

    auto filter_sub = event_bus.subscribe<cataclysm::gui::UiFilterAppliedEvent>(
        [&recorder](const cataclysm::gui::UiFilterAppliedEvent &event) {
            recorder.filter_applied = true;
            recorder.last_filter_text = event.getFilterText();
            recorder.last_filter_target = event.getTargetComponent();
            recorder.last_filter_case_sensitive = event.isCaseSensitive();
        });

    auto item_selected_sub = event_bus.subscribe<cataclysm::gui::UiItemSelectedEvent>(
        [&recorder](const cataclysm::gui::UiItemSelectedEvent &event) {
            recorder.item_selected = true;
            recorder.last_item_id = event.getItemId();
            recorder.last_item_source = event.getSourceComponent();
            recorder.last_item_double_click = event.isDoubleClick();
            recorder.last_item_count = event.getItemCount();
        });

    auto data_binding_sub = event_bus.subscribe<cataclysm::gui::UiDataBindingUpdateEvent>(
        [&recorder](const cataclysm::gui::UiDataBindingUpdateEvent &event) {
            recorder.data_binding_updated = true;
            recorder.last_binding_id = event.getBindingId();
            recorder.last_binding_source = event.getDataSource();
            recorder.last_binding_forced = event.isForced();
        });

    OverlayUI overlay_ui(adapter);
    overlay_ui.UpdateMapTexture(reinterpret_cast<SDL_Texture*>(0x1), 480, 480, 24, 24);

    const auto inventory_state = BuildMockInventoryState();
    const auto character_state = BuildMockCharacterState();

    RunVisualInteractionTest(io, overlay_ui, inventory_state, character_state, recorder);
    RunOverlayLifecycleTest(adapter, recorder);

    adapter.shutdown();

    hover_sub->unsubscribe();
    click_sub->unsubscribe();
    inventory_sub->unsubscribe();
    tab_sub->unsubscribe();
    row_sub->unsubscribe();
    overlay_open_sub->unsubscribe();
    overlay_close_sub->unsubscribe();
    filter_sub->unsubscribe();
    item_selected_sub->unsubscribe();
    data_binding_sub->unsubscribe();
    command_sub->unsubscribe();

    ImGui::DestroyContext();

    return 0;
}

