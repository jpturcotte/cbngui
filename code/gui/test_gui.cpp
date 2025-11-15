#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>

#include "input_manager.h"
#include "CharacterOverlayState.h"
#include "CharacterWidget.h"
#include "InventoryOverlayState.h"
#include "InventoryWidget.h"
#include "event_bus_adapter.h"
#include "event_bus.h"
#include "events.h"
#include "imgui.h"
#include "overlay_manager.h"
#include "map_widget.h"
#include "overlay_ui.h"
#include "ui_manager.h"

namespace {

void RunInputManagerEventRoutingTests() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    BN::GUI::InputManager::InputSettings settings;
    settings.pass_through_enabled = true;
    settings.prevent_game_input_when_gui_focused = true;

    BN::GUI::InputManager manager(settings);
    assert(manager.Initialize());
    manager.SetGUIAreaBounds(0, 0, 256, 256);
    manager.SetFocusState(BN::GUI::InputManager::FocusState::GUI, "tests");

    bool key_press_called = false;
    bool key_release_called = false;
    bool mouse_press_called = false;

    manager.RegisterHandler(
        BN::GUI::InputManager::EventType::KEYBOARD_PRESS,
        [&](const BN::GUI::GUIEvent&) {
            key_press_called = true;
            return true;
        },
        BN::GUI::InputManager::Priority::NORMAL);

    manager.RegisterHandler(
        BN::GUI::InputManager::EventType::KEYBOARD_RELEASE,
        [&](const BN::GUI::GUIEvent&) {
            key_release_called = true;
            return true;
        },
        BN::GUI::InputManager::Priority::NORMAL);

    manager.RegisterHandler(
        BN::GUI::InputManager::EventType::MOUSE_BUTTON_PRESS,
        [&](const BN::GUI::GUIEvent&) {
            mouse_press_called = true;
            return true;
        },
        BN::GUI::InputManager::Priority::HIGH);

    auto stats = manager.GetStatistics();
    assert(stats.active_handlers.load() == 3);

    SDL_Event key_down{};
    key_down.type = SDL_KEYDOWN;
    key_down.key.type = SDL_KEYDOWN;
    key_down.key.state = SDL_PRESSED;
    key_down.key.repeat = 0;
    key_down.key.keysym.sym = SDLK_a;
    key_down.key.keysym.scancode = SDL_SCANCODE_A;
    key_down.key.keysym.mod = KMOD_NONE;

    assert(manager.ProcessEvent(key_down));
    assert(key_press_called);
    assert(!key_release_called);
    assert(!mouse_press_called);

    key_press_called = false;

    SDL_Event key_up{};
    key_up.type = SDL_KEYUP;
    key_up.key.type = SDL_KEYUP;
    key_up.key.state = SDL_RELEASED;
    key_up.key.keysym.sym = SDLK_a;
    key_up.key.keysym.scancode = SDL_SCANCODE_A;
    key_up.key.keysym.mod = KMOD_NONE;

    assert(manager.ProcessEvent(key_up));
    assert(!key_press_called);
    assert(key_release_called);

    key_release_called = false;

    SDL_Event mouse_button{};
    mouse_button.type = SDL_MOUSEBUTTONDOWN;
    mouse_button.button.type = SDL_MOUSEBUTTONDOWN;
    mouse_button.button.button = SDL_BUTTON_LEFT;
    mouse_button.button.state = SDL_PRESSED;
    mouse_button.button.x = 128;
    mouse_button.button.y = 128;

    assert(manager.ProcessEvent(mouse_button));
    assert(mouse_press_called);

    mouse_press_called = false;

    manager.SetFocusState(BN::GUI::InputManager::FocusState::GAME, "game");

    key_press_called = false;
    assert(!manager.ProcessEvent(key_down));
    assert(!key_press_called);

    auto updated_settings = manager.GetSettings();
    updated_settings.pass_through_enabled = false;
    manager.UpdateSettings(updated_settings);

    key_press_called = false;
    assert(manager.ProcessEvent(key_down));
    assert(key_press_called);

    mouse_button.button.x = 600;
    mouse_button.button.y = 600;
    assert(!manager.ProcessEvent(mouse_button));
    assert(!mouse_press_called);

    manager.Shutdown();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    // Verify shared-focus keyboard pass-through respects handler consumption
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    BN::GUI::InputManager::InputSettings shared_settings;
    shared_settings.pass_through_enabled = true;
    shared_settings.prevent_game_input_when_gui_focused = true;

    BN::GUI::InputManager shared_manager(shared_settings);
    assert(shared_manager.Initialize());
    shared_manager.SetFocusState(BN::GUI::InputManager::FocusState::SHARED, "shared-tests");

    bool keyboard_handler_invoked = false;
    shared_manager.RegisterHandler(
        BN::GUI::InputManager::EventType::KEYBOARD_PRESS,
        [&](const BN::GUI::GUIEvent& event) {
            keyboard_handler_invoked = true;
            return event.sdl_event.key.keysym.sym == SDLK_RETURN;
        },
        BN::GUI::InputManager::Priority::NORMAL);

    SDL_Event shared_key{};
    shared_key.type = SDL_KEYDOWN;
    shared_key.key.type = SDL_KEYDOWN;
    shared_key.key.state = SDL_PRESSED;
    shared_key.key.repeat = 0;
    shared_key.key.keysym.sym = SDLK_a;
    shared_key.key.keysym.scancode = SDL_SCANCODE_A;
    shared_key.key.keysym.mod = KMOD_NONE;

    keyboard_handler_invoked = false;
    assert(!shared_manager.ShouldConsumeEvent(shared_key));
    assert(!shared_manager.ProcessEvent(shared_key));
    assert(keyboard_handler_invoked);

    auto updated_shared_settings = shared_manager.GetSettings();
    updated_shared_settings.pass_through_enabled = false;
    shared_manager.UpdateSettings(updated_shared_settings);

    shared_key.key.keysym.sym = SDLK_RETURN;
    keyboard_handler_invoked = false;
    assert(shared_manager.ShouldConsumeEvent(shared_key));
    assert(shared_manager.ProcessEvent(shared_key));
    assert(keyboard_handler_invoked);

    shared_manager.Shutdown();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void RunOverlayManagerUiIntegrationTest() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    SDL_Window* window = SDL_CreateWindow(
        "overlay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_HIDDEN);
    assert(window != nullptr);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    assert(renderer != nullptr);

    OverlayManager overlay_manager;
    OverlayManager::Config config;
    config.enabled = true;
    config.pass_through_input = false;
    config.dpi_scale = 1.0f;

    assert(overlay_manager.Initialize(window, renderer, config));

    auto& ui_manager = cataclysm::gui::UiManager::instance();
    assert(ui_manager.registered_count() == 0);
    assert(!overlay_manager.IsOpen());

    overlay_manager.Open();
    assert(ui_manager.registered_count() == 1);
    assert(overlay_manager.IsOpen());

    overlay_manager.SetFocused(true);

    overlay_manager.ShowInventory();

    inventory_overlay_state modal_inventory_state{};
    modal_inventory_state.active_column = 0;
    modal_inventory_state.columns[0].name = "Worn";
    modal_inventory_state.columns[1].name = "Inventory";
    modal_inventory_state.columns[2].name = "Ground";
    overlay_manager.UpdateInventory(modal_inventory_state);

    SDL_Event handled_key{};
    handled_key.type = SDL_KEYDOWN;
    handled_key.key.type = SDL_KEYDOWN;
    handled_key.key.state = SDL_PRESSED;
    handled_key.key.repeat = 0;
    handled_key.key.keysym.sym = SDLK_RIGHT;
    handled_key.key.keysym.scancode = SDL_SCANCODE_RIGHT;
    handled_key.key.keysym.mod = KMOD_NONE;

    assert(overlay_manager.HandleEvent(handled_key));

    SDL_Event unhandled_event{};
    unhandled_event.type = SDL_USEREVENT;
    unhandled_event.user.type = SDL_USEREVENT;

    assert(!overlay_manager.HandleEvent(unhandled_event));

    overlay_manager.Close();
    assert(ui_manager.registered_count() == 0);
    assert(!overlay_manager.IsOpen());

    overlay_manager.HideInventory();
    overlay_manager.Shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

struct MockInventorySelector {
    int active_column = 0;
    std::string filter_text;
    bool examine_invoked = false;
    std::vector<std::string> activated_entries;

    void activate_stack(const inventory_entry &entry) {
        activated_entries.push_back(entry.hotkey);
    }

    void move_left() {
        if (active_column > 0) {
            --active_column;
        }
    }

    void move_right() {
        if (active_column < 2) {
            ++active_column;
        }
    }

    void append_filter_char(char ch) {
        filter_text.push_back(ch);
    }

    void backspace_filter() {
        if (!filter_text.empty()) {
            filter_text.pop_back();
        }
    }

    void examine_selected() {
        examine_invoked = true;
    }
};

class MockInputContext {
public:
    explicit MockInputContext(MockInventorySelector &selector) : selector_(selector) {}

    bool handle_event(const SDL_Event &event) {
        if (event.type != SDL_KEYDOWN) {
            return false;
        }

        const SDL_Keycode sym = event.key.keysym.sym;
        switch (sym) {
            case SDLK_LEFT:
                selector_.move_left();
                return true;
            case SDLK_RIGHT:
                selector_.move_right();
                return true;
            case SDLK_BACKSPACE:
                selector_.backspace_filter();
                return true;
            case SDLK_x:
                selector_.examine_selected();
                return true;
            default:
                if (sym >= ' ' && sym <= '~') {
                    selector_.append_filter_char(static_cast<char>(sym));
                    return true;
                }
                break;
        }

        return false;
    }

private:
    MockInventorySelector &selector_;
};

struct MockCharacterDisplay {
    std::vector<std::string> tab_order;
    int active_tab_index = -1;
    int active_row_index = -1;
    cataclysm::gui::CharacterCommand last_command = cataclysm::gui::CharacterCommand::HELP;
    int command_count = 0;

    void set_tabs(std::vector<std::string> tabs) {
        tab_order = std::move(tabs);
    }

    void request_tab(const std::string &tab_id) {
        const auto it = std::find(tab_order.begin(), tab_order.end(), tab_id);
        if (it != tab_order.end()) {
            active_tab_index = static_cast<int>(std::distance(tab_order.begin(), it));
        }
    }

    void activate_row(const std::string &tab_id, int row_index) {
        request_tab(tab_id);
        active_row_index = row_index;
    }

    void handle_command(cataclysm::gui::CharacterCommand command) {
        last_command = command;
        ++command_count;
    }
};

SDL_KeyboardEvent MakeKeyEvent(SDL_Keycode key, SDL_Scancode scancode) {
    SDL_KeyboardEvent event{};
    event.type = SDL_KEYDOWN;
    event.state = SDL_PRESSED;
    event.repeat = 0;
    event.keysym.sym = key;
    event.keysym.scancode = scancode;
    event.keysym.mod = KMOD_NONE;
    return event;
}

void RunOverlayInventoryInteractionBridgeTest() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    SDL_Window *window = SDL_CreateWindow(
        "overlay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_HIDDEN);
    assert(window != nullptr);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    assert(renderer != nullptr);

    OverlayManager overlay_manager;
    OverlayManager::Config config;
    config.enabled = true;
    config.pass_through_input = false;
    config.dpi_scale = 1.0f;

    assert(overlay_manager.Initialize(window, renderer, config));

    MockInventorySelector expected_selector;
    MockInventorySelector actual_selector;

    MockInputContext expected_context(expected_selector);
    MockInputContext actual_context(actual_selector);

    inventory_entry clicked_entry{};
    clicked_entry.label = "Bandage";
    clicked_entry.hotkey = "a";
    clicked_entry.is_category = false;
    clicked_entry.is_selected = false;
    clicked_entry.is_highlighted = false;
    clicked_entry.is_favorite = false;
    clicked_entry.is_disabled = false;
    clicked_entry.disabled_msg.clear();

    auto &event_bus = cataclysm::gui::EventBusManager::getGlobalEventBus();

    overlay_manager.SetInventoryClickHandler([
        &actual_selector
    ](const inventory_entry &entry) {
        actual_selector.activate_stack(entry);
    });

    overlay_manager.SetInventoryKeyHandler([
        &actual_context
    ](const SDL_KeyboardEvent &key_event) {
        SDL_Event wrapped{};
        wrapped.type = key_event.type;
        wrapped.key = key_event;
        actual_context.handle_event(wrapped);
    });

    cataclysm::gui::InventoryItemClickedEvent pre_click(clicked_entry);
    event_bus.publish(pre_click);
    assert(actual_selector.activated_entries.empty());

    SDL_KeyboardEvent sample_event = MakeKeyEvent(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    cataclysm::gui::InventoryKeyInputEvent pre_key(sample_event);
    event_bus.publish(pre_key);
    assert(actual_selector.active_column == 0);
    assert(actual_selector.filter_text.empty());
    assert(!actual_selector.examine_invoked);

    overlay_manager.Open();
    overlay_manager.ShowInventory();
    overlay_manager.SetFocused(true);

    expected_selector.activate_stack(clicked_entry);

    cataclysm::gui::InventoryItemClickedEvent click_event(clicked_entry);
    event_bus.publish(click_event);
    assert(actual_selector.activated_entries == expected_selector.activated_entries);

    const std::vector<SDL_KeyboardEvent> key_sequence = {
        MakeKeyEvent(SDLK_RIGHT, SDL_SCANCODE_RIGHT),
        MakeKeyEvent(SDLK_RIGHT, SDL_SCANCODE_RIGHT),
        MakeKeyEvent(SDLK_LEFT, SDL_SCANCODE_LEFT),
        MakeKeyEvent(SDLK_a, SDL_SCANCODE_A),
        MakeKeyEvent(SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE),
        MakeKeyEvent(SDLK_x, SDL_SCANCODE_X),
    };

    for (const auto &key_event : key_sequence) {
        SDL_Event wrapped{};
        wrapped.type = key_event.type;
        wrapped.key = key_event;
        expected_context.handle_event(wrapped);
    }

    for (const auto &key_event : key_sequence) {
        cataclysm::gui::InventoryKeyInputEvent gui_event(key_event);
        event_bus.publish(gui_event);
    }

    assert(actual_selector.active_column == expected_selector.active_column);
    assert(actual_selector.filter_text == expected_selector.filter_text);
    assert(actual_selector.examine_invoked == expected_selector.examine_invoked);

    overlay_manager.Close();

    cataclysm::gui::InventoryItemClickedEvent post_click(clicked_entry);
    event_bus.publish(post_click);
    assert(actual_selector.activated_entries == expected_selector.activated_entries);

    overlay_manager.HideInventory();
    overlay_manager.Shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void RunOverlayCharacterInteractionBridgeTest() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    SDL_Window *window = SDL_CreateWindow(
        "overlay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_HIDDEN);
    assert(window != nullptr);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    assert(renderer != nullptr);

    OverlayManager overlay_manager;
    OverlayManager::Config config;
    config.enabled = true;
    config.pass_through_input = false;
    config.dpi_scale = 1.0f;

    assert(overlay_manager.Initialize(window, renderer, config));

    MockCharacterDisplay expected_display;
    expected_display.set_tabs({ "stats", "skills", "traits", "effects" });
    MockCharacterDisplay actual_display = expected_display;

    overlay_manager.SetCharacterTabHandler([
        &actual_display
    ](const std::string &tab_id) {
        actual_display.request_tab(tab_id);
    });

    overlay_manager.SetCharacterRowHandler([
        &actual_display
    ](const std::string &tab_id, int row_index) {
        actual_display.activate_row(tab_id, row_index);
    });

    overlay_manager.SetCharacterCommandHandler([
        &actual_display
    ](cataclysm::gui::CharacterCommand command) {
        actual_display.handle_command(command);
    });

    auto &event_bus = cataclysm::gui::EventBusManager::getGlobalEventBus();

    cataclysm::gui::CharacterTabRequestedEvent pre_open_tab("skills");
    event_bus.publish(pre_open_tab);
    assert(actual_display.active_tab_index == -1);

    overlay_manager.Open();
    overlay_manager.ShowCharacter();
    overlay_manager.SetFocused(true);

    expected_display.request_tab("skills");
    cataclysm::gui::CharacterTabRequestedEvent tab_event("skills");
    event_bus.publish(tab_event);
    assert(actual_display.active_tab_index == expected_display.active_tab_index);

    expected_display.activate_row("traits", 2);
    cataclysm::gui::CharacterRowActivatedEvent row_event("traits", 2);
    event_bus.publish(row_event);
    assert(actual_display.active_tab_index == expected_display.active_tab_index);
    assert(actual_display.active_row_index == expected_display.active_row_index);

    expected_display.handle_command(cataclysm::gui::CharacterCommand::RENAME);
    cataclysm::gui::CharacterCommandEvent rename_event(cataclysm::gui::CharacterCommand::RENAME);
    event_bus.publish(rename_event);
    assert(actual_display.command_count == expected_display.command_count);
    assert(actual_display.last_command == expected_display.last_command);

    overlay_manager.HideCharacter();
    const int hidden_tab_index = actual_display.active_tab_index;
    const int hidden_row_index = actual_display.active_row_index;
    cataclysm::gui::CharacterTabRequestedEvent hidden_tab("stats");
    event_bus.publish(hidden_tab);
    assert(actual_display.active_tab_index == hidden_tab_index);
    assert(actual_display.active_row_index == hidden_row_index);

    overlay_manager.ShowCharacter();
    expected_display.request_tab("stats");
    event_bus.publish(hidden_tab);
    assert(actual_display.active_tab_index == expected_display.active_tab_index);

    expected_display.activate_row("effects", 1);
    cataclysm::gui::CharacterRowActivatedEvent effects_row("effects", 1);
    event_bus.publish(effects_row);
    assert(actual_display.active_tab_index == expected_display.active_tab_index);
    assert(actual_display.active_row_index == expected_display.active_row_index);

    overlay_manager.Close();
    const int closed_tab_index = actual_display.active_tab_index;
    const int closed_row_index = actual_display.active_row_index;
    cataclysm::gui::CharacterCommandEvent closed_command(cataclysm::gui::CharacterCommand::CONFIRM);
    event_bus.publish(closed_command);
    assert(actual_display.command_count == expected_display.command_count);
    assert(actual_display.last_command == expected_display.last_command);
    cataclysm::gui::CharacterTabRequestedEvent closed_tab("skills");
    event_bus.publish(closed_tab);
    assert(actual_display.active_tab_index == closed_tab_index);
    assert(actual_display.active_row_index == closed_row_index);

    overlay_manager.HideCharacter();
    overlay_manager.Shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void RunOverlayInventoryBridgeModalEventTest() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    SDL_Window *window = SDL_CreateWindow(
        "overlay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_HIDDEN);
    assert(window != nullptr);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    assert(renderer != nullptr);

    OverlayManager overlay_manager;
    OverlayManager::Config config;
    config.enabled = true;
    config.pass_through_input = false;
    config.dpi_scale = 1.0f;

    assert(overlay_manager.Initialize(window, renderer, config));

    bool inventory_key_forwarded = false;
    SDL_Keycode forwarded_keycode = SDLK_UNKNOWN;
    bool inventory_click_forwarded = false;
    std::string forwarded_hotkey;

    overlay_manager.SetInventoryKeyHandler([
        &inventory_key_forwarded,
        &forwarded_keycode
    ](const SDL_KeyboardEvent &key_event) {
        inventory_key_forwarded = true;
        forwarded_keycode = key_event.keysym.sym;
    });

    overlay_manager.SetInventoryClickHandler([
        &inventory_click_forwarded,
        &forwarded_hotkey
    ](const inventory_entry &entry) {
        inventory_click_forwarded = true;
        forwarded_hotkey = entry.hotkey;
    });

    auto &event_bus = cataclysm::gui::EventBusManager::getGlobalEventBus();

    inventory_entry sample_entry{};
    sample_entry.label = "Bandage";
    sample_entry.hotkey = "z";
    sample_entry.is_category = false;

    cataclysm::gui::InventoryItemClickedEvent pre_open_click(sample_entry);
    event_bus.publish(pre_open_click);
    assert(!inventory_click_forwarded);

    overlay_manager.Open();
    overlay_manager.ShowInventory();
    overlay_manager.SetFocused(true);

    inventory_overlay_state overlay_state{};
    overlay_state.title = "Inventory";
    overlay_state.active_column = 0;
    overlay_state.columns[0].name = "Worn";
    overlay_state.columns[1].name = "Inventory";
    overlay_state.columns[2].name = "Ground";
    overlay_manager.UpdateInventory(overlay_state);

    SDL_Event handled_key{};
    handled_key.type = SDL_KEYDOWN;
    handled_key.key.type = SDL_KEYDOWN;
    handled_key.key.state = SDL_PRESSED;
    handled_key.key.repeat = 0;
    handled_key.key.keysym.sym = SDLK_LEFT;
    handled_key.key.keysym.scancode = SDL_SCANCODE_LEFT;
    handled_key.key.keysym.mod = KMOD_NONE;

    inventory_key_forwarded = false;
    forwarded_keycode = SDLK_UNKNOWN;
    assert(overlay_manager.HandleEvent(handled_key));
    assert(inventory_key_forwarded);
    assert(forwarded_keycode == SDLK_LEFT);

    inventory_click_forwarded = false;
    forwarded_hotkey.clear();
    cataclysm::gui::InventoryItemClickedEvent click_event(sample_entry);
    event_bus.publish(click_event);
    assert(inventory_click_forwarded);
    assert(forwarded_hotkey == sample_entry.hotkey);

    overlay_manager.Close();
    overlay_manager.HideInventory();
    overlay_manager.Shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void RunOverlayCharacterBridgeModalEventTest() {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        assert(!"SDL_InitSubSystem(SDL_INIT_VIDEO) failed");
    }

    SDL_Window *window = SDL_CreateWindow(
        "overlay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_HIDDEN);
    assert(window != nullptr);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    assert(renderer != nullptr);

    OverlayManager overlay_manager;
    OverlayManager::Config config;
    config.enabled = true;
    config.pass_through_input = false;
    config.dpi_scale = 1.0f;

    assert(overlay_manager.Initialize(window, renderer, config));

    bool character_tab_forwarded = false;
    std::string forwarded_tab_id;
    bool character_row_forwarded = false;
    std::string forwarded_row_tab;
    int forwarded_row_index = -1;
    bool character_command_forwarded = false;
    cataclysm::gui::CharacterCommand forwarded_command = cataclysm::gui::CharacterCommand::HELP;

    overlay_manager.SetCharacterTabHandler([
        &character_tab_forwarded,
        &forwarded_tab_id
    ](const std::string &tab_id) {
        character_tab_forwarded = true;
        forwarded_tab_id = tab_id;
    });

    overlay_manager.SetCharacterRowHandler([
        &character_row_forwarded,
        &forwarded_row_tab,
        &forwarded_row_index
    ](const std::string &tab_id, int row_index) {
        character_row_forwarded = true;
        forwarded_row_tab = tab_id;
        forwarded_row_index = row_index;
    });

    overlay_manager.SetCharacterCommandHandler([
        &character_command_forwarded,
        &forwarded_command
    ](cataclysm::gui::CharacterCommand command) {
        character_command_forwarded = true;
        forwarded_command = command;
    });

    auto &event_bus = cataclysm::gui::EventBusManager::getGlobalEventBus();

    cataclysm::gui::CharacterTabRequestedEvent pre_open_tab("stats");
    event_bus.publish(pre_open_tab);
    assert(!character_tab_forwarded);

    overlay_manager.Open();
    overlay_manager.ShowCharacter();
    overlay_manager.SetFocused(true);

    character_overlay_state character_state{};
    character_state.header_left = "Character";
    character_state.header_right = "[?]";
    character_state.info_panel_text = "Info";
    character_state.tabs.push_back({ "stats", "Stats", {} });
    character_state.tabs.push_back({ "skills", "Skills", {} });
    character_state.tabs.push_back({ "traits", "Traits", {} });
    character_state.tabs.push_back({ "effects", "Effects", {} });
    character_state.tabs.back().rows.push_back({ "Effect A", "", "", IM_COL32(255, 255, 255, 255), false });
    character_state.tabs.back().rows.push_back({ "Effect B", "", "", IM_COL32(255, 255, 255, 255), false });
    character_state.active_tab_index = 1;
    character_state.active_row_index = 0;
    character_state.footer_lines = { "Footer" };
    character_state.bindings = { "?", "TAB", "SHIFT+TAB", "ENTER", "ESC", "r" };

    overlay_manager.UpdateCharacter(character_state);

    character_tab_forwarded = false;
    forwarded_tab_id.clear();
    cataclysm::gui::CharacterTabRequestedEvent tab_event("skills");
    event_bus.publish(tab_event);
    assert(character_tab_forwarded);
    assert(forwarded_tab_id == "skills");

    character_row_forwarded = false;
    forwarded_row_tab.clear();
    forwarded_row_index = -1;
    cataclysm::gui::CharacterRowActivatedEvent row_event("effects", 1);
    event_bus.publish(row_event);
    assert(character_row_forwarded);
    assert(forwarded_row_tab == "effects");
    assert(forwarded_row_index == 1);

    character_command_forwarded = false;
    forwarded_command = cataclysm::gui::CharacterCommand::HELP;
    cataclysm::gui::CharacterCommandEvent command_event(cataclysm::gui::CharacterCommand::CONFIRM);
    event_bus.publish(command_event);
    assert(character_command_forwarded);
    assert(forwarded_command == cataclysm::gui::CharacterCommand::CONFIRM);

    overlay_manager.Close();
    overlay_manager.HideCharacter();
    overlay_manager.Shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

struct EventRecorder {
    bool map_tile_hovered = false;
    bool map_tile_clicked = false;
    bool inventory_item_clicked = false;
    bool inventory_key_forwarded = false;
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
    SDL_Keycode last_forwarded_keycode = SDLK_UNKNOWN;
    SDL_Scancode last_forwarded_scancode = SDL_SCANCODE_UNKNOWN;
    SDL_Keymod last_forwarded_mod = KMOD_NONE;
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
        { "Emergency Whistle", "-", false, false, false, false, false, "" },
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
    io.AddMousePosEvent(mouse_pos.x, mouse_pos.y);
    io.AddMouseButtonEvent(0, mouse_down);
    io.AddMouseWheelEvent(0.0f, 0.0f);
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

    SDL_Event click_event{};
    click_event.type = SDL_MOUSEBUTTONDOWN;
    click_event.button.type = SDL_MOUSEBUTTONDOWN;
    click_event.button.button = SDL_BUTTON_LEFT;
    click_event.button.state = SDL_PRESSED;
    click_event.button.clicks = 1;
    click_event.button.x = static_cast<int>(item_target.x);
    click_event.button.y = static_cast<int>(item_target.y);

    const bool click_consumed = overlay_ui.GetInventoryWidget().HandleEvent(click_event);
    assert(click_consumed);
    assert(recorder.inventory_item_clicked);
    assert(recorder.last_inventory_entry.label == "Water");
    assert(recorder.last_inventory_entry.hotkey == "c");

    recorder.inventory_item_clicked = false;
    recorder.inventory_key_forwarded = false;
    recorder.last_forwarded_keycode = SDLK_UNKNOWN;
    recorder.last_forwarded_scancode = SDL_SCANCODE_UNKNOWN;
    recorder.last_forwarded_mod = KMOD_NONE;

    SDL_Event minus_key_event{};
    minus_key_event.type = SDL_KEYDOWN;
    minus_key_event.key.type = SDL_KEYDOWN;
    minus_key_event.key.state = SDL_PRESSED;
    minus_key_event.key.repeat = 0;
    minus_key_event.key.keysym.scancode = SDL_SCANCODE_MINUS;
    minus_key_event.key.keysym.sym = SDLK_MINUS;

    const bool minus_consumed = overlay_ui.GetInventoryWidget().HandleEvent(minus_key_event);
    assert(minus_consumed);
    assert(recorder.inventory_key_forwarded);
    assert(recorder.last_forwarded_keycode == SDLK_MINUS);
    assert(recorder.last_forwarded_scancode == SDL_SCANCODE_MINUS);
    assert(recorder.last_forwarded_mod == KMOD_NONE);
    assert(!recorder.inventory_item_clicked);

    recorder.inventory_key_forwarded = false;
    recorder.last_forwarded_keycode = SDLK_UNKNOWN;
    recorder.last_forwarded_scancode = SDL_SCANCODE_UNKNOWN;
    recorder.last_forwarded_mod = KMOD_NONE;

    SDL_Event wheel_event{};
    wheel_event.type = SDL_MOUSEWHEEL;
    wheel_event.wheel.type = SDL_MOUSEWHEEL;
    wheel_event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    wheel_event.wheel.y = 1;
    wheel_event.wheel.preciseY = 0.0f;

    const bool wheel_consumed = overlay_ui.GetInventoryWidget().HandleEvent(wheel_event);
    assert(wheel_consumed);
    assert(recorder.inventory_key_forwarded);
    assert(recorder.last_forwarded_keycode == SDLK_UP);
    assert(recorder.last_forwarded_scancode == SDL_SCANCODE_UP);
    assert(recorder.last_forwarded_mod == KMOD_NONE);

    recorder.inventory_key_forwarded = false;
    recorder.last_forwarded_keycode = SDLK_UNKNOWN;
    recorder.last_forwarded_scancode = SDL_SCANCODE_UNKNOWN;
    recorder.last_forwarded_mod = KMOD_NONE;

    SDL_Event precise_wheel_event{};
    precise_wheel_event.type = SDL_MOUSEWHEEL;
    precise_wheel_event.wheel.type = SDL_MOUSEWHEEL;
    precise_wheel_event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    precise_wheel_event.wheel.preciseY = 1.0f;

    const bool precise_wheel_consumed =
        overlay_ui.GetInventoryWidget().HandleEvent(precise_wheel_event);
    assert(precise_wheel_consumed);
    assert(recorder.inventory_key_forwarded);
    assert(recorder.last_forwarded_keycode == SDLK_UP);
    assert(recorder.last_forwarded_scancode == SDL_SCANCODE_UP);
    assert(recorder.last_forwarded_mod == KMOD_NONE);

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
    RunInputManagerEventRoutingTests();
    RunOverlayManagerUiIntegrationTest();
    RunOverlayInventoryInteractionBridgeTest();
    RunOverlayCharacterInteractionBridgeTest();
    RunOverlayInventoryBridgeModalEventTest();
    RunOverlayCharacterBridgeModalEventTest();

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

    auto inventory_key_sub = event_bus.subscribe<cataclysm::gui::InventoryKeyInputEvent>(
        [&recorder](const cataclysm::gui::InventoryKeyInputEvent &event) {
            recorder.inventory_key_forwarded = true;
            recorder.last_forwarded_keycode = event.getKeyEvent().keysym.sym;
            recorder.last_forwarded_scancode = event.getKeyEvent().keysym.scancode;
            recorder.last_forwarded_mod = static_cast<SDL_Keymod>(event.getKeyEvent().keysym.mod);
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
    inventory_key_sub->unsubscribe();
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

