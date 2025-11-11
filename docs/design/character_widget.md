# CBN GUI Character Widget Design Document

## Objective
Create an ImGui-based character overlay widget in **cbngui** that mirrors the Cataclysm:BN in-game character screen (`@`) and synchronizes with the engine via the existing overlay infrastructure (shared state structs, SDL tiles bridge, overlay manager). The design prioritizes feature parity, configurability awareness, and reuse of the inventory overlay’s proven data flow.

## Background
- The inventory overlay demonstrates the end-to-end pipeline for presenting an engine-driven UI in the GUI client:
  1. Engine builds a plain-old-data (`cbn_inventory_overlay_state`).
  2. `sdltiles.cpp` converts the struct to a GUI-friendly mirror and invokes overlay manager helpers.
  3. `OverlayManager` caches the payload, controls visibility, and asks `OverlayUI` to render it.
  4. `OverlayUI` owns `InventoryWidget`, which renders the ImGui window and emits interaction events.
- The classic character screen lives in `character_display::disp_info` and spans multiple logical tabs (stats, encumbrance, skills, traits, bionics, effects, speed). Matching the ASCII UI keeps the GUI and SDL builds aligned for testing and documentation.

## High-Level Requirements
1. **Feature parity:** Present all sections currently available in the ASCII character screen, including selection-dependent detail panes.
2. **Live synchronization:** Update when stats change (e.g., encumbrance, mutations) and close when the user exits the character screen.
3. **Input awareness:** Respect the player’s keybinding configuration by relaying the active bindings in the payload.
4. **Event routing:** Allow GUI-originated actions (e.g., tab switches, stat upgrades) to signal back to the engine without duplicating logic.
5. **Extensibility:** Follow the inventory overlay pattern to minimize custom plumbing and ease maintenance.

## Proposed Architecture
```
character_display::disp_info
        │
        ▼
cbn_character_overlay_state (engine, sdltiles.h)
        │ conversion helpers (sdltiles.cpp)
        ▼
character_overlay_state (cbngui, CharacterOverlayState.h)
        │
OverlayManager::UpdateCharacter(...) ──▶ OverlayUI::DrawCharacter(...)
        │                                  │
        │                                  ▼
        └── Show/Hide control        CharacterWidget::Draw(ImGui, EventBus)
```

## Data Model
Create twin structs that serialize all panels. Each section can be represented as vectors of simple records to keep ABI stability.

### Engine-side (`sdltiles.h`)
```cpp
struct cbn_character_overlay_column_entry {
    std::string name;
    std::string value;
    std::string tooltip; // optional detail text
    nc_color color;
    bool highlighted = false;
};

struct cbn_character_overlay_tab {
    std::string id;      // "stats", "encumbrance", ...
    std::string title;   // localized tab name
    std::vector<cbn_character_overlay_column_entry> rows;
};

struct cbn_character_overlay_state {
    std::string header_left;   // name / profession
    std::string header_right;  // help key prompt
    std::string info_panel_text; // six-line descriptive block
    std::vector<cbn_character_overlay_tab> tabs;
    int active_tab_index = 0;
    int active_row_index = 0; // interpreted within tab
    std::vector<std::string> footer_lines; // speed summary, encumbrance hints, etc.
    cbn_input_bindings bindings; // reused struct that lists keys for HELP, TAB, BACKTAB, CONFIRM, QUIT, RENAME, etc.
};
```

### GUI-side (`cbngui/code/gui/CharacterOverlayState.h`)
Mirror the engine struct using `ImColor` or `uint32_t` for colors and `std::vector<std::string>` for multi-line tooltips. Maintain identical member ordering for safe `memcpy`/`reinterpret_cast` conversions.

## Implementation Steps

### 1. Define Overlay State Payloads
1. Add the C struct definitions to `sdltiles.h`, co-located with existing inventory overlay structs.
2. Introduce the matching C++ definitions in a new `cbngui/code/gui/CharacterOverlayState.h` (and `.cpp` if helper functions are needed).
3. Include convenience helpers for default construction and color translation (e.g., `ColorToImU32`).

### 2. Build the Character Widget
1. Create `cbngui/code/gui/widgets/CharacterWidget.h/.cpp` beside `InventoryWidget`.
2. The widget constructor accepts `EventBusAdapter &` to emit:
   - `CharacterTabRequestedEvent { std::string tab_id }`
   - `CharacterRowActivatedEvent { std::string tab_id, int row_index }`
   - `CharacterCommandEvent { CharacterCommand command }` (for rename, confirm, quit, help).
3. `Draw(const character_overlay_state &state)` renders:
   - A fixed-size ImGui window following the ASCII layout (three 26×9 grids on the top row, detail pane beneath, vertical list panels).
   - Left column: stats grid. Center: encumbrance grid. Right: speed panel. Lower half: tabbed sections (skills, traits, bionics, effects) with shared info pane.
   - Info panel text is split on newline characters (`\n`) to mimic the six-line catacurses panel.
4. Highlight the active row and tab using the `highlighted` flag and `active_tab_index`. Use ImGui tables with row hover states and color-coded text.
5. Render binding hints by reading `state.bindings`, matching the overlay header’s prompt and optional footer.
6. Capture mouse interactions: clicking rows triggers event bus messages, clicking tab headers sends `CharacterTabRequestedEvent`.

### 3. Integrate with OverlayUI & OverlayManager
1. **OverlayUI** (`cbngui/code/gui/OverlayUI.h/.cpp`):
   - Add a `std::unique_ptr<CharacterWidget> character_widget;` member.
   - Instantiate it in the constructor with the shared `EventBusAdapter`.
   - Implement `void DrawCharacter(const character_overlay_state &state);` that calls `character_widget->Draw(state)` if visible.
2. **OverlayManager** (`cbngui/code/gui/OverlayManager.h/.cpp`):
   - Add storage for `std::optional<character_overlay_state> character_state;` and a `bool is_character_visible` flag.
   - Provide `ShowCharacter`, `HideCharacter`, `UpdateCharacter`, and `IsCharacterVisible` methods mirroring the inventory overlay’s API.
   - Extend `Render()` to call `overlay_ui.DrawCharacter(*character_state);` when visible.
3. Export the new manager methods via `extern "C"` functions in `cbngui/code/gui/OverlayExports.cpp` so `sdltiles` can toggle visibility.

### 4. Bridge Engine and GUI (`sdltiles.cpp`)
1. Add helper `static character_overlay_state to_gui_state(const cbn_character_overlay_state &src);` similar to `inventory_overlay_state_from_cbn`.
2. Implement `cbn_gui_show_character`, `cbn_gui_update_character`, and `cbn_gui_hide_character` that delegate to the overlay manager if GUI overlays are enabled.
3. Update the exported symbols so ASCII builds stub them out when the GUI is disabled.

### 5. Populate the State in Gameplay
1. In `character_display::disp_info` (and any helper classes), construct `cbn_character_overlay_state state;` every refresh tick.
2. Populate tabs:
   - `stats`: four attribute entries plus derived values via `Character::hp_max`, `get_weight_capacity_string`, etc.
   - `encumbrance`: use `get_encumbrance()` results, merging symmetrical limbs (reuse `bodypart_id::list_all` loops).
   - `speed`: replicate `speed_effects` map to build rows showing each contributing modifier.
   - `traits/bionics/effects/skills`: iterate existing containers; reuse display formatting functions (`Character::mutation_display_color`, `bionic_display_name`, `effect_name_and_text`).
3. Fill `info_panel_text` using the same functions that populate the ASCII detail window for the active selection.
4. Serialize keybindings by calling `input_context::get_desc("HELP_KEYBINDINGS")`, etc., and storing the string results in `bindings`.
5. When the character screen opens, call `cbn_gui_show_character(state);`. During navigation updates, call `cbn_gui_update_character(state);`. On exit (when the UI loop breaks), call `cbn_gui_hide_character();`.

### 6. Event Round-Trip (Optional MVP extension)
- Subscribe to `CharacterWidget` events in the shared event bus so that GUI clicks invoke the same actions as keyboard commands:
  - Tab clicks -> call `ui_manager.action_context().set_tab(tab_id);`
  - Row activations -> mimic confirm action (e.g., toggle skill training).
  - Command events -> dispatch to the existing keybinding logic (`player_character::disp_info_action`).

## Key Considerations
- **Localization:** Tab titles and tooltip texts should stay localized; reuse `translated` strings from the engine rather than duplicating in the GUI.
- **Color fidelity:** Translate `nc_color` to RGBA using the same helper as the inventory overlay to match ASCII color schemes.
- **Performance:** The state object should remain lightweight; avoid embedding heavyweight structures. If necessary, compress repeated strings (e.g., share trait descriptions by index) but prioritize clarity first.
- **Testing:** Add smoke tests or debug logging to ensure show/update/hide calls pair correctly, preventing orphaned overlays.

## Build & Integration Checklist
1. **Data Contracts**
   - [ ] `sdltiles.h` defines `cbn_character_overlay_state` and nested structs.
   - [ ] `cbngui/code/gui/CharacterOverlayState.h` mirrors the layout.
2. **Widget**
   - [ ] `CharacterWidget` implemented with rendering logic and event emission.
   - [ ] Unit-testable helper functions (formatting, layout calculations) extracted where practical.
3. **Overlay Plumbing**
   - [ ] `OverlayUI` instantiates `CharacterWidget` and exposes `DrawCharacter`.
   - [ ] `OverlayManager` manages show/hide/update with cached state.
   - [ ] C exports added for engine calls.
4. **Engine Bridge**
   - [ ] Conversion helpers in `sdltiles.cpp` and exported entry points.
   - [ ] ASCII fallback stubs updated.
5. **Gameplay Hook**
   - [ ] `character_display::disp_info` constructs and updates the overlay state.
   - [ ] Cleanup guaranteed on exit or exception paths.
6. **Event Round-Trip (if implemented now)**
   - [ ] GUI events mapped to engine actions via event bus handlers.

## Future Enhancements
- Synchronize in real-time while the character screen is open (e.g., receiving health/mutation updates from the event bus).
- Add optional expanded panels for mods that extend the character sheet (bionics categories, achievements).
- Support mouse-over tooltips and context popups that display extended descriptions beyond the six-line info pane.
- Explore a responsive layout for smaller screens or multi-column traits list.

## Appendix: References
- Inventory overlay files: `cbngui/code/gui/InventoryOverlayState.h`, `cbngui/code/gui/widgets/InventoryWidget.*`, `cbngui/code/gui/OverlayManager.*`.
- Engine bridge: `src/sdltiles.cpp`, `src/sdltiles.h`.
- Character screen logic: `src/character_display.cpp`, `src/character.cpp`.
- Keybinding context: `src/input.cpp` (for `input_context`).