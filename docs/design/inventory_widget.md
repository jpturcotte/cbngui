# CBN GUI Inventory Widget Design Document

## Purpose
The Cataclysm: Bright Nights (CBN) ImGui front end currently ships with a terrain map overlay that mirrors the curses-based map view. This document specifies the complementary inventory overlay so that GUI users obtain the same information and control afforded by the terminal interface. It captures the necessary research, summarizes the behavior of the inventory selector, and lays out the design and implementation steps required to export, render, and interact with inventory data inside the SDL/ImGui overlay system.

## Background
- **Classic inventory selector** — The curses inventory screen is built around `inventory_pick_selector` (and subclasses) in `src/inventory_ui.cpp`. The selector rebuilds its UI every frame: it restacks items, groups them by category, and draws a header, three-column body, and footer. Each rebuild occurs inside the loop kicked off by `game_menus::inv::common( player &u )` when the `inventory` action (default key: `i`) fires.
- **Overlay manager** — The ImGui layer is controlled by `cbngui::overlay_manager` (in `cbngui/src/overlay_manager.cpp`). When enabled via `CBN_GUI_ENABLED`, the manager intercepts SDL events, advances registered widgets in `overlay_manager::update`, and renders them during the frame. The map widget follows this path: the curses terrain renderer populates an SDL texture, hands the texture to `overlay_manager`, and the ImGui widget (`cbngui/src/widgets/map_widget.cpp`) draws it while also relaying mouse/keyboard events back into the game.

## Goals
1. Provide a faithful ImGui representation of the inventory selector, including titles, weight/volume stats, columnar layout, categories, item stacks, hotkeys, and filter state.
2. Keep the widget in lockstep with the curses UI so that any action (filtering, toggling favorites, assigning letters, moving between columns, opening the examine menu) updates both views simultaneously.
3. Allow the ImGui widget to consume pointer and keyboard input without breaking the existing inventory control flow.
4. Make the integration minimally invasive: reuse selector data instead of recomputing it and rely on the existing overlay lifecycle used by the map widget.

## Non-goals
- Rewriting the inventory selector or changing its business logic.
- Adding new gameplay features or inventory actions; the scope is limited to visualization and event routing.

## High-level Architecture
```
+---------------------------+
| inventory_pick_selector   |
|  - restack/rebuild data   |
|  - draw to curses WINDOW  |
|  - now: export DTO        |
+-------------+-------------+
              |
              v
+---------------------------+        +---------------------+
| cbngui::overlay_manager  | -----> | ImGui inventory     |
|  - register widget       |        | widget (new)        |
|  - propagate SDL events  |        |  - render state     |
|  - manage lifecycle      |        |  - send commands    |
+---------------------------+        +---------------------+
```

### Data transfer object (DTO)
- Introduce a lightweight struct (e.g., `inventory_overlay_state`) holding:
  - `std::string title` and `std::string hotkey_hint` from `inventory_pick_selector::prepare_layout`.
  - Weight/volume strings returned by `get_weight_and_volume_stats`.
  - Column metadata (column label, scroll position, entries).
  - For each category header and item stack: display text, hotkey letter (if any), favorite flag, disabled-state message, and selection flags.
  - Footer content: active filter string and navigation mode label.
- Populate the DTO inside `inventory_pick_selector::refresh_window()` after the selector has rebuilt its cached columns but before it draws to curses. This placement ensures the DTO always reflects the latest selector state.

### Overlay update path
1. `inventory_selector::execute()` already runs `ui_manager::redraw()` each loop iteration. `ui_manager::redraw()` calls back into `inventory_pick_selector::refresh_window()`.
2. After the curses drawing functions finish (or immediately after column data is prepared), call a new helper (`cbngui::inventory_overlay::update( dto )`) guarded by `#ifdef CBN_GUI_ENABLED`.
3. The helper forwards the DTO to `overlay_manager::update_inventory( dto )`. The manager caches the most recent state so the ImGui widget can render it during the next `overlay_manager::draw()` call.

### Widget rendering
- Add a new widget class (e.g., `cbngui::inventory_widget`) in `cbngui/src/widgets/inventory_widget.cpp` with a matching header in `include/cbngui/widgets/inventory_widget.h`.
- The widget exposes:
  - `void inventory_widget::set_state( inventory_overlay_state state )` — copies the latest DTO into the widget.
  - `void inventory_widget::draw()` — builds the ImGui layout:
    - Header: title, hotkey hint, weight/volume labels.
    - A three-column table mirroring the curses layout. Each category header and stack row is rendered with ImGui selectable widgets; colors and icons should reproduce favorite/highlight cues.
    - Footer: filter string and navigation mode label.
  - `bool inventory_widget::handle_event( const SDL_Event &evt )` — interprets mouse clicks, scrolls, and key presses. Map them to the inventory input context keys by feeding them back to the game (see “Input routing”).

### Input routing
- The overlay manager already maintains an event pass-through chain. Follow the map widget pattern:
  1. When the inventory widget receives an event, translate relevant inputs to the inventory context (e.g., clicking a row triggers the equivalent of selecting that row and pressing Enter; mouse wheel scroll maps to up/down).
  2. Use `input_context::register_action` hotkeys to ensure the ImGui widget respects rebinding.
  3. Any event the widget does not consume should be returned so the standard inventory selector handles it.

## Detailed Implementation Plan

### 1. Data definitions
- Create `include/cbngui/inventory_overlay_state.h` describing the DTO:
  ```cpp
  struct inventory_entry {
      std::string label;        // "2x clean water (2)"
      std::string hotkey;       // "a"
      bool is_category;         // true for category headers
      bool is_selected;
      bool is_highlighted;      // active column highlight
      bool is_favorite;
      bool is_disabled;
      std::string disabled_msg; // optional
  };

  struct inventory_column {
      std::string name;         // e.g., "Inventory"
      int scroll_position;      // row index for top item
      std::vector<inventory_entry> entries;
  };

  struct inventory_overlay_state {
      std::string title;
      std::string hotkey_hint;
      std::string weight_label;
      std::string volume_label;
      std::string filter_string;
      std::string navigation_mode; // "Category mode" / "Item mode"
      std::array<inventory_column, 3> columns;
      int active_column;          // 0..2
  };
  ```
- If the selector can show fewer than three columns (e.g., no ground items), allow empty columns.

### 2. Extract data from the selector
- In `inventory_pick_selector::refresh_window()` (or a helper it calls), after the columns are prepared:
  1. Create an `inventory_overlay_state dto;`.
  2. Copy header strings from existing member variables (`title`, `hint`, `weight_carried`, etc.).
  3. For each `inventory_column`, iterate over `current_column->entries` (or equivalent) to fill DTO entries. Mark categories versus item stacks based on entry type.
  4. Set `is_selected` for the row that matches `active_stack_position`, `is_highlighted` for entries whose column equals `active_column`, etc.
  5. Wrap all new code in `#ifdef CBN_GUI_ENABLED` to avoid impacting non-GUI builds.
- Call `cbngui::inventory_overlay::update( dto )` with the fully populated DTO. Provide a `cbngui::inventory_overlay::update( const inventory_overlay_state & )` declaration in `include/cbngui/inventory_overlay.h`.

### 3. Overlay manager integration
- Extend `cbngui::overlay_manager`:
  - Add a member `std::optional<inventory_overlay_state> inventory_state;`.
  - Add `void overlay_manager::update_inventory( inventory_overlay_state state );` that stores the state and ensures the inventory widget is instantiated.
  - When the inventory selector opens, ask the manager to show the widget:
    ```cpp
    if( cbn::overlay_manager *mgr = cbn::overlay_manager::try_get() ) {
        mgr->show_inventory_widget();
    }
    ```
  - On exit, hide the widget and clear the cached state.
- Register the widget in the manager’s widget list similarly to the map widget (e.g., `widgets.emplace_back( std::make_unique<inventory_widget>() );`).

### 4. ImGui widget rendering
- Implement `inventory_widget::draw()`:
  - Use `ImGui::Begin( "Inventory" )` (or a style-matching wrapper) to open a window anchored near the inventory area.
  - Draw the header line using `ImGui::TextUnformatted` for the title and stats, optionally grouping them with `ImGui::SameLine()`.
  - Use `ImGui::BeginTable` with three columns. For each `inventory_column`, push the column and iterate entries:
    - Category headers can use `ImGui::SeparatorText` or colored text.
    - Item rows should be `ImGui::Selectable` with `ImGuiSelectableFlags_SpanAllColumns` to render hotkeys and additional info.
    - Apply colors for favorites (yellow), disabled entries (red), and the currently selected row (highlight background).
  - Footer: print filter status and navigation mode.
- Implement `handle_event` to map ImGui interactions back to the game:
  - Store row bounds when rendering so mouse clicks can map to row indexes.
  - On click, call a helper that injects the equivalent action (e.g., set selector’s highlight, send Enter). Use existing `send_game_input` facilities from other widgets.
  - Forward keyboard input by publishing an `InventoryKeyInputEvent` through the event bus so the gameplay `input_context` resolves the active binding.

### 5. Lifecycle hooks
- In the inventory selector’s constructor/destructor or in `execute()`:
  - On entry (just before the `do { ... } while` loop), call `overlay_manager::instance().open_inventory_widget()` if the GUI is active.
  - On exit (after the loop, before returning), call `overlay_manager::close_inventory_widget()`.
- Ensure hiding the widget cancels any outstanding pointer capture.

### 6. Build system updates
- Add new source and header files to the `cbngui` CMake target (`cbngui/CMakeLists.txt`). Mirror how `map_widget.cpp` is included.
- If the DTO header needs to be visible to both core and GUI targets, place it in `include/cbngui/` and update include directories accordingly.

### 7. Documentation and samples
- Update the GUI developer README (if present) to mention the inventory widget toggle and how to invoke the inventory screen via `i`.
- Optionally record a short video or screenshot demonstrating the synchronized view.

## Hook-up Instructions
1. **Ensure GUI build flags are enabled:** Configure CMake with `-DCMAKE_BUILD_TYPE=Release -DCBN_GUI_ENABLED=ON`. The standard SDL2/ImGui dependencies for the map overlay apply; no new libraries are required.
2. **Implement DTO extraction:** Follow “Detailed Implementation Plan – Step 2” to export selector state each redraw.
3. **Update overlay manager:** Provide `update_inventory`, `show_inventory_widget`, and `hide_inventory_widget` methods; register the new widget with the manager when the GUI initializes.
4. **Create the ImGui widget:** Implement the renderer and input handler using the DTO; ensure it registers itself with `overlay_manager` and processes events before they reach the curses UI.
5. **Wire up lifecycle calls:** Modify the inventory selector to open the widget when the inventory UI starts and close it when done.
6. **Test end-to-end:** Build the project (`ninja cbn`) and run the game with GUI mode enabled. Press `i` to open inventory; verify the ImGui window matches the terminal layout, updates as you filter/sort/favorite items, and that commands (select, examine, toggle favorite, assign letters) work through the overlay.

## Validation Strategy
- **Visual parity checks:** Capture screenshots of the curses inventory and ImGui overlay side-by-side to confirm column contents, highlighting, and stats match.
- **Interaction tests:**
  - Filter text updates both views instantly.
  - Favorite toggles change colors/hotkey priorities.
  - Column navigation via arrow keys and mouse clicks stays synchronized.
  - Examine menu (`e`) opens correctly when triggered from the ImGui overlay.
- **Regression testing:** Run existing unit tests and GUI smoke tests (if any) to ensure no build regressions. Specifically verify non-GUI builds by compiling without `CBN_GUI_ENABLED`.

## Future Enhancements
- **Drag-and-drop support:** Allow dragging items between columns in the GUI as a shortcut for existing actions.
- **Contextual buttons:** Surface common inventory actions (favorite, drop, wield) as explicit ImGui buttons once the core overlay is stable.
- **Performance tuning:** Profile DTO extraction frequency and optimize data reuse if necessary; consider incremental updates to reduce copying in large inventories.
- **Accessibility:** Offer scalable fonts and colorblind-friendly palettes within the ImGui widget.

## References
- `src/inventory_ui.cpp` — Inventory selector implementation.
- `cbngui/src/overlay_manager.cpp` — Overlay lifecycle and event routing.
- `cbngui/src/widgets/map_widget.cpp` — Existing ImGui widget integration example.
- `src/game.cpp` (inventory action dispatch) — Entry point for the selector.
