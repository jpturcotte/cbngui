# Cataclysm: Bright Nights (BN) Codebase Architecture — A System Blueprint

## Executive Summary

Cataclysm: Bright Nights (BN) is a C++ roguelike built on a layered architecture that cleanly separates rendering, input, UI, gameplay state, and persistence. The engine presents a unified ASCII/text surface on top of two backends: a pseudo-curses layer (for terminal-like rendering) and an SDL tiles renderer. This abstraction allows BN to target terminals and graphical environments from a single codebase. A global game state object orchestrates the main update cycle and coordinates UI and input via mode-specific contexts. The save system is JSON-based with versioning and compression, facilitating robust loading and forward evolution of save formats.[^1]

The core architectural pillars are:

- Rendering split into curses-like buffers (cursesport) and the final display compositor (sdltiles), with Unicode-aware text storage and color/attribute handling.
- Input routed through a global input manager and per-mode input contexts, translating raw events into action IDs and supporting timeouts, mouse, and gamepad.
- UI constructed from shared building blocks (uilist, string_input_popup) and a redraw/resize coordination layer (ui_manager/ui_adaptor), rendering to catacurses::window objects.
- Game state centralized in a single game instance, with logical subsystems (map, overmap, weather, event bus) owned and coordinated by the game class.
- Persistence based on JSON serialization, run-length encoding for repetitive layers, and migration logic for legacy save versions.
- Main loop progression from entry-point initialization into game::do_turn, with autosave heuristics and UI state anchoring updates.

Primary subsystems: cursesport/sdltiles (rendering), input manager and input contexts (input), uilist and UI manager (UI), game class with world/map/overmanagers (state), and savegame_json with versioning (persistence). These boundaries are stable and clearly trace the flow from event to action to state mutation to render to save.[^2][^3][^5][^6][^7]

Known information gaps include incomplete coverage of sdltiles.cpp composition, the exact ordering inside game::do_turn, legacy loader internals across older save versions, worldfactory/world creation flows, and an authoritative list of panels managed by panel_manager. The analysis notes these and confines claims to observed, verifiable code.

## Repository Layout and Build Overview

The repository is organized along conventional lines for a large C++ application: source in src/, game data in data/, graphics in gfx/, sound in sound/, localization in lang/, and mods under mods/. Build scaffolding includes CMake, Make, and platform-specific folders for Android and macOS. Precompiled headers in pch accelerate compilation. A docs/ directory hosts project documentation, while developer tooling and CI/CD are configured under .github and editor-specific folders.[^1]

To illustrate subsystem location, Table 1 summarizes key directories and their roles.

Table 1. Key directories and purpose

| Directory | Purpose |
|---|---|
| src/ | C++ source for engine and game logic (rendering, input, UI, state, save, map, entities) |
| data/ | Game data (JSON definitions for items, monsters, terrain, etc.) |
| gfx/ | Graphics assets used by the tiles build |
| sound/ | Audio assets |
| lang/ | Localization files and workflows |
| mods/ | In-repository mods |
| pch/ | Precompiled headers for faster builds |
| android/, build-data/osx/ | Platform-specific build support |
| CMakeModules/, msvc-full-features/ | Build system modules and MSVC configurations |
| docs/, doc/ | Project documentation and site sources |
| .github/, .vscode/ | CI/CD and developer tooling configurations |

The split ensures rendering, input, and UI code are co-located in src/, with platform abstractions and build tooling in adjacent folders. This layout is congruent with multi-backend rendering and cross-platform builds documented in the repository and official documentation.[^1][^9]

## Core Rendering System and ASCII Display

BN’s rendering architecture decouples a curses-like buffer from the final display pipeline. Cursesport maintains in-memory windows and text cells, while sdltiles consumes those buffers to produce the visible frame on graphical systems. This indirection allows the same UI and gameplay code to target a terminal-like experience and an SDL-based tiles renderer.[^4][^1]

The cursesport implementation centers on struct WINDOW, which stores dimensions, position, cursor state, color pairs, and text lines. Text is stored as lines of cells, each carrying foreground/background colors and a UTF‑8 character. The system supports wide characters (two display cells) with invariants to ensure layout integrity. Window updates are buffered via wnoutrefresh, and a final doupdate hands control to the display backend. Colors and attributes are applied through conventional functions (init_pair, wattron, wattroff) that translate to stored FG/BG and attribute flags.[^4]

Key rendering primitives and calls are shown in Table 2.

Table 2. Cursesport API and roles

| Function/Struct | Role |
|---|---|
| WINDOW | Base struct for a window: size, pos, cursor, FG/BG pairs, text storage |
| curseline/cursecell | Line and cell containers for text, foreground, background, UTF‑8 character |
| printstring | Writes UTF‑8 strings into cells, handling newlines, cursor advance, and wide-char invariants |
| fill, UTF8_getch | Parse input string into code points, compute display width; handle control chars |
| mk_wcwidth, wcwidth.h | Determine wide character width for layout correctness |
| init_pair, wattron, wattroff | Initialize and apply color pairs; map bold/blink to color + 8 semantics |
| wborder, mvwhline, mvwvline | Draw window borders and lines using specified characters |
| wmove_internal, wmove, mvwprintw | Manage cursor movement within windows |
| wclear, werase, erase | Clear window content and mark touched lines for redraw |
| wnoutrefresh | Queue a window for redraw (buffered updates) |
| doupdate, refresh_display | Commit buffered updates via display backend (sdltiles) |
| wrefresh | Convenience: wnoutrefresh + doupdate |

The pseudo-curses buffer is Unicode-aware. Characters are extracted as UTF‑8 code points, and wide characters consume two cells, leaving the subsequent cell empty to preserve alignment. This handling is critical for ASCII renderers supporting international characters and special symbols.[^4]

### Curses-like Text Buffer Model

Cursesport stores text as cursecell structures in curseline arrays, each cell encoding a character (as UTF‑8), foreground color, and background color. The printstring function appends text to the current cursor position, advances the cursor, and clears the neighbor cell when a two-cell character is written. This approach is compatible with both monospaced terminal rendering and SDL-based compositing that translates cells to glyphs.[^4]

### Color and Attributes

Color pairs are maintained by cursesport, and attributes like bold and blink are modeled by incrementing the color index (color + 8). wattron enables attributes, wattroff resets them to defaults. This abstraction yields consistent visual behavior across backends while delegating final blending and font/glyph selection to sdltiles.[^4]

### Unicode and Wide Character Support

Wide characters are identified via wcwidth functions, with control characters replaced by spaces to avoid buffer corruption. The system enforces invariants that prevent wide characters from being placed at the last cell of a line, ensuring the rendering cursor and buffer remain coherent. This is essential for correctly drawing box-drawing characters, East Asian glyphs, and other two-cell symbols in the ASCII surface.[^4]

### Window Management and Refresh

Windows can be created, moved, cleared, and refreshed. wnoutrefresh queues changes, and doupdate applies them in batch to the display backend. This double-buffering improves performance and enables atomic redraws across multiple windows during a single frame. sdltiles, as the display composer, translates buffered cells to visible output.[^4]

## Input Handling and Keyboard Event Processing

BN’s input system converts heterogeneous events into abstract actions bound to game contexts. A global input_manager singleton initializes mappings, loads/saves keybindings, and manages timeouts. An input_context per screen or mode registers action names and filters raw events (keyboard, gamepad, mouse) into actionable IDs. Direction handling supports directional actions and isometric coordinate rotation when iso_mode is enabled.[^5]

The system supports input timeouts, multi-key sequences, conflict detection among bindings, and an in-game binding editor. Mouse coordinates can be captured for point-relative UIs, and Android builds support virtual “manual keys” under conditional compilation.[^5]

Table 3 outlines event types and representative keys.

Table 3. Input event types and representative keys

| Event type | Examples |
|---|---|
| Keyboard | Space (‘ ’) to tilde (‘~’); function keys F1–F12; directional keys; combinations like CTRL+A |
| Gamepad | JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN; diagonal combinations; JOY_0 |
| Mouse | MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, SCROLLWHEEL_UP/DOWN, MOUSE_MOVE |
| Special | ANY_INPUT, COORDINATE, TIMEOUT, HELP_KEYBINDINGS, CATA_ERROR |

Bindings are loaded from JSON in default and user directories. The system resolves conflicts when adding bindings and provides a menu to edit, add, or remove key assignments. Actions are filtered, and human-readable descriptions (get_desc) are generated for prompts. Direction registration helpers (register_directions, get_direction) unify movement across UIs and game modes.[^5]

### Event Representation and Polling

Events are represented by a struct storing type, modifiers, a sequence for multi-key inputs, and mouse coordinates. input_context::handle_input polls events and returns the corresponding action ID. This indirection allows screen code to remain independent of device specifics while retaining access to raw input when necessary (e.g., coordinate capture for map look-around).[^\*]

### Keybindings and JSON Persistence

Keybindings reside in JSON files under data/raw and user-specific directories. Each binding associates an action with a key or device input and can support sequences. The system tracks user-created bindings, validates conflicts, and writes updated bindings back to disk, ensuring a stable, editable input surface for players.[^5]

### UI Context Switching

Each screen or mode constructs its own input_context and registers the actions it handles, often overlapping with global defaults. For example, uilist registers navigation and confirmation actions, while a game mode registers movement, look, and activity actions. Context-local registration ensures inputs are meaningful and reduces cross-talk between screens.[^5]

## Game State Management and UI System Structure

BN centralizes gameplay state in a single game instance. The game class orchestrates initialization, the main update cycle, world and map management, and persistence. UI state is coordinated separately through a uistate instance and an adaptor/manager layer, while panel management organizes modular UI panels like sidebars. The result is a disciplined split: the game owns world logic; UI owns presentation and redraw coordination.[^3][^1]

Table 4 summarizes the principal game state components.

Table 4. Game state components and responsibilities

| Component | Responsibility |
|---|---|
| game (singleton) | Central orchestrator: loop, initialization, world/map/overmap, events |
| uquit | Quit state (died, suicide, watch) controlling flow termination |
| safe_mode | SAFE_MODE_ON/OFF/STOP to pause gameplay under threat |
| gamemode | Abstract game mode; concrete instances managed by game |
| map | Local map and submap data, visibility, lighting, terrain/items/vehicles |
| overmap_buffer | Overworld region data and tracking |
| calendar | In-game time; turn tracking; start times and seasons |
| weather_manager | Weather state, events, and transitions |
| critter_tracker | Active monster management |
| active_npc | Non-player characters under management |
| event_bus | Inter-component event distribution |
| zone_manager | Player-defined zones for automation and targeting |
| timed_events | Scheduled callbacks and pulses |
| stats/achievements/kill_tracker/memorial_logger | Progression metrics and record keeping |
| moves_since_last_save, last_save_timestamp | Autosave heuristic tracking |

BN’s UI is built from reusable components with consistent lifecycle and redraw semantics. uilist provides selection, filtering, and description panels; string_input_popup handles text entry; ui_manager and ui_adaptor ensure redraws and resize callbacks are propagated; panel_manager organizes modular panels like sidebars. catacurses::window provides the underlying surface for all UI elements.[^6][^3]

Table 5 lists key UI classes and interactions.

Table 5. UI classes and key interactions

| Class | Role | Key methods/interactions |
|---|---|---|
| uilist | Selectable list/menu | Construction with auto or manual sizing/positioning; entries with hotkeys/descriptions; FILTER input; scrollby; callbacks; apply_scrollbar |
| string_input_popup | Text input | Position relative to a window; editing commands; confirm/cancel |
| ui_manager | Global UI redraw control | Redraw coordination; callback scheduling |
| ui_adaptor | Redraw/resize hooks | on_redraw; on_screen_resize; invalidation and refresh |
| panel_manager | Panel layout | Organize and render side panels (e.g., sidebar) |
| input_context | Mode-specific input | Register actions; handle_input; binding editor integration |
| catacurses::window | Rendering surface | Window creation, print, border, move, clear, refresh |

The UI lifecycle typically follows: a screen constructs windows and UI elements, registers an input context, and subscribes for redraw/resize events via ui_adaptor. When the screen exits, it releases the context and invalidates any cached UI state to avoid stale redraws.[^6]

### Global Game State and Events

The game singleton holds references to map, overmap_buffer, calendar, weather, event bus, and trackers. It processes timed events and publishes inter-component events through the bus. This centralization makes save/load coherent, ensures consistent world updates, and provides stable anchoring for UI state.[^3]

### UI Components and Patterns

uilist supports auto-sizing around content with folding text and manual overrides, and it integrates hotkeys and filter prompts. Descriptions and scrollbars provide navigable context for long lists. Callbacks (including pointmenu_cb) bridge UI selections to gameplay coordinates or actions, using the game instance to update view offsets and trigger redraws.[^6]

### Resize and Redraw Coordination

ui_adaptor registers redraw and resize callbacks. When the terminal size changes or a panel’s layout needs refresh, ui_manager::redraw is invoked to re-layout and repaint. This separation ensures individual UI elements do not implement their own resize logic, reducing duplication and edge-case bugs.[^6]

## File I/O System for Saves/Loads

BN’s persistence is JSON-based, versioned, and designed for backward compatibility. A savegame_version number tracks incompatible changes, and the code uses explicit migration paths for earlier formats. The serialization architecture uses JsonOut/JsonIn for structured output/input, mongroup templated io for compact grouping, and run-length encoding (RLE) for repetitive data such as scent maps and overmap boolean layers.[^7]

Saves serialize:

- Game state: turn, calendar, player data, messages, safe_mode, monsters, events, trackers, and global scent/visibility.
- Overmap: terrain (RLE), monster groups (binned and hashed), cities, connections, radio towers, vehicles, scents, NPCs, special placements, and mapgen arguments.
- Overmap view: visible, explored, path tiles (RLE compacted), notes, extras.
- Master save: next IDs, missions, factions, world seed, weather flags.

Legacy support includes deserialization shims for pre-version calendars, pre-JSON kills, and older faction formats. The system uses standard C++ iostreams for file access and wraps file operations with try/catch to surface JSON errors during load.[^7]

Table 6 outlines the serialized data and compression techniques.

Table 6. Serialized data and compression techniques

| Area | Data | Format/Technique |
|---|---|---|
| Game state | turn, calendar times, safe_mode, messages, monsters, trackers, player | JSON members; explicit read/write |
| Global scent | grscent, typescent | RLE to compress contiguous runs |
| Overmap terrain | Terrain layer per overmap | RLE for long runs of identical terrain |
| Monster groups | Type, position, radius, population, flags | Templated mongroup::io; hash/binning for efficiency |
| Overmap view | visible, explored, path | RLE compacted boolean arrays |
| Notes/extras | Positioned annotations | JSON arrays of objects |
| Master save | IDs, missions, factions, world seed, weather | JSON; legacy checks for older versions |

### Versioning and Migration

When save content changes in incompatible ways, savegame_version is bumped. Loading code checks the saved version and applies migration logic as needed. Notably, older calendars and faction formats have dedicated conversion paths. The current version observed in the codebase is 28, and legacy checks include pre-26 calendar migration.[^7]

### Efficient Encoding Strategies

RLE reduces storage for scent maps and overmap boolean layers by compressing repeated values. Monster groups are binned and hashed so that identical group descriptors are serialized once with positions aggregated, minimizing repetition. These choices improve both disk footprint and load time for large worlds.[^7]

## Main Game Loop and Update Cycle

BN’s entry points initialize platform-specific contexts and command-line options, then hand control to the game class. The main loop centers on game::do_turn, which processes input in the current context, advances time and events, updates subsystems, and triggers redraws. Autosave is driven by elapsed turns or minutes and user activity metrics.[^2][^3]

The following sequence is representative:

1. Initialize options and platform backends (curses or SDL).
2. Create or load a world via worldfactory (implied by references to world and worldfactory), constructing a new game instance.
3. Enter game::do_turn:
   a. Poll input via the active input_context to obtain an action.
   b. Dispatch the action to gameplay logic (movement, look-around, menus).
   c. Advance time and process timed events, weather, and monster AI.
   d. Update UI state as needed (view offsets, panel layouts, active screens).
   e. Commit window buffers to the display via cursesport and sdltiles.
4. Evaluate autosave heuristics (turns/minutes since last save).
5. Repeat or exit based on quit state (uquit).[^2][^3]

### Entry Points and Initialization

The application defines multiple entry points (console main, Windows WinMain, and SDL_main) to accommodate platforms. Early initialization parses arguments and sets up options, rendering backends, and the game instance. From here, the system transitions to world creation or loading and then to the main loop.[^2]

### Turn Processing and Autosave

game::do_turn drives the core cycle. Actions from the input context are dispatched to screens and gameplay systems. Time advances by turns; the calendar, weather, and event bus evolve accordingly. The game tracks moves since the last save and elapsed minutes; upon thresholds (e.g., AUTOSAVE_TURNS, AUTOSAVE_MINUTES), it triggers a save via savegame routines, ensuring progress is preserved without excessive I/O.[^3][^7]

## Game Screen Organization

Screens are functions or classes that temporarily own rendering and input for a specific task. They construct windows, register an input context, and return control when the user completes the task or cancels. This pattern is visible across main menus, gameplay overlays, and inventory UIs, and is consistent with BN’s emphasis on modular, reusable components.[^1][^3][^6]

Representative screens and modes include:

- Main menu: entry point to new, load, mods, and options.
- Gameplay screens: look_around with debug overlays, list_items/list_monsters toggles, zones_manager, and NPC interaction menus.
- Inventory and crafting: inventory UI and crafting GUIs, often implemented with uilist and specialized input contexts.
- Debug overlays: display_scent, display_temperature, display_lighting, display_radiation, display_transparency, display_vehicle_ai, display_visibility.

Modal flows typically register their own input context, drive uilist/string_input_popup for user interaction, and exit on a completion action or cancel. The game instance is available to update view offsets, trigger redraws, or mutate state as users confirm choices.[^3][^6]

### Main Menu and World Management

World selection and creation are managed by worldfactory and related code paths. The main menu branches to new/load, after which the game class takes over. This separation ensures world lifecycle is handled consistently and allows the main menu to remain small and focused.[^3]

### Gameplay Overlays and Lists

look_around provides tile inspection with cursor movement and can feed coordinates to other tools (e.g., zones). list_items and list_monsters offer filtered views of items and entities, with actions to examine, compare, or blacklist. Zones management is a dedicated UI for defining, moving, and editing player zones. Each overlay owns its input and rendering until completion.[^3][^6]

## Key Classes, Methods, and Data Structures (Quick Reference)

Table 7 provides a quick index of core classes and notable members/methods.

Table 7. Key classes, files, and responsibilities

| Class/Struct | File | Responsibility | Notable members/methods |
|---|---|---|---|
| game | game.cpp | Central game state and loop | do_turn; setup/start; save/load; map; overmap_buffer; calendar; weather; event_bus; uquit; safe_mode; autosave heuristics |
| uistatedata | game.cpp | UI-specific state | UI preferences and caches anchoring UI to the game instance |
| ui_manager | ui.cpp | Global UI redraw | Redraw orchestration and callback scheduling |
| ui_adaptor | ui.cpp | Redraw/resize hooks | on_redraw; on_screen_resize; invalidation |
| panel_manager | ui.cpp | Panel layout | Sidebar and panel arrangement (authoritative list not fully verified) |
| input_manager | input.cpp | Global input management | Initialize keycodes; load/save bindings; timeouts |
| input_context | input.cpp | Mode-specific input | register_action; handle_input; get_desc; binding editor |
| input_event | input.cpp | Event representation | type, modifiers, sequence, mouse_pos |
| WINDOW | cursesport.cpp | Curses-like window buffer | width, height, cursor, FG/BG, lines |
| curseline/cursecell | cursesport.cpp | Text storage | Cursecell: FG/BG and UTF‑8 character; wide-char handling |
| uilist | ui.cpp | Menu UI | entries; filter; hotkeys; callbacks; scrollbar |
| string_input_popup | ui.cpp | Text input | positioning; editing; confirm/cancel |
| JsonOut/JsonIn | savegame.cpp | JSON serialization | start_object/end_object; member; arrays |
| mongroup | savegame.cpp | Monster group | io templated serialize/deserialize; hash/binning; legacy deserialize |
| overmap, overmap_buffer | savegame.cpp | Overworld data | RLE terrain; view layers; notes/extras |
| map | game.cpp | Local map data | Visibility, items, vehicles, terrain |
| calendar | game.cpp | In-game time | turn; start times; seasons |
| weather_manager | game.cpp | Weather state | Conditions and transitions |
| event_bus | game.cpp | Event distribution | Publish/subscribe for inter-component events |
| zone_manager | game.cpp | Player zones | Define, move, edit zones |
| critter_tracker | game.cpp | Monster registry | Active monsters and lifecycle |
| item, vehicle | game.cpp | Items and vehicles | State and interactions |

The coupling points are deliberate: UI elements use input_context for action dispatch, game for state mutation and timing, and cursesport for rendering surfaces. ui_manager/ui_adaptor coordinate redraws across these layers.[^3][^5][^6][^7][^4]

## System Interactions and Data Flow

A typical frame flows from input to action to state to render to save:

1. The active input_context polls via input_manager and returns an action ID (e.g., MOVE_North, LOOK, FILTER).
2. The game instance (or the active screen) executes the action, mutating map, player, or UI state and advancing time as necessary.
3. UI components refresh; ui_adaptor notifies ui_manager, which triggers redraws across catacurses::window surfaces.
4. Cursesport buffers updates (wnoutrefresh); sdltiles composes the final display in doupdate.
5. On certain turns or time thresholds, the game triggers savegame_json, serializing JSON to disk with RLE where appropriate.[^3][^5][^6][^7][^4]

State ownership is centralized in the game class. UI subscribes to redraw and resize hooks and does not directly own world or event state. Input contexts are per-mode, allowing a screen to focus on relevant actions without intercepting global keys unnecessarily. Rendering is a pure function of buffered window state and backend selection, enabling tiles or ASCII backends without changing upstream code.[^3][^5][^6]

## Risks, Constraints, and Open Questions

- Rendering backend composition: While cursesport is documented and visible, the internal function-by-function composition of sdltiles is not fully extracted here. Any changes to blending, batching, or font loading in sdltiles should be reviewed alongside cursesport to ensure buffer semantics are preserved.
- Main loop timing details: The precise ordering of input polling, action dispatch, and UI redraw within game::do_turn is not exhaustively extracted. However, the general flow and autosave logic are clear; further micro-optimization requires source verification.
- Legacy save migrations: Although versioning and selected migration paths are documented, a full catalog of legacy loaders and edge-case migrations should be reviewed when maintaining or evolving save formats.
- UI panel registry: An authoritative list of panels and their layouts managed by panel_manager is not included. Adding or modifying panels should be coordinated with ui_adaptor to ensure redraw correctness.
- World creation flow: worldfactory and world creation/loading interactions are referenced but not fully extracted. Modifications to world initialization should be cross-checked with game::setup/start paths.

These constraints reflect the boundaries of extracted sources and are flagged to guide future work and prevent assumptions beyond verified code.

## References

[^1]: Cataclysm: Bright Nights — GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN  
[^2]: Cataclysm-BN src/main.cpp (entry points, initialization, loop scaffolding). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/main.cpp  
[^3]: Cataclysm-BN src/game.cpp (game state, UI integration, main loop). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/game.cpp  
[^4]: Cataclysm-BN src/cursesport.cpp (pseudo-curses implementation and ASCII rendering). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/cursesport.cpp  
[^5]: Cataclysm-BN src/input.cpp (input manager, contexts, keybindings). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/input.cpp  
[^6]: Cataclysm-BN src/ui.cpp (uilist, UI manager, adaptor, windowing). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/ui.cpp  
[^7]: Cataclysm-BN src/savegame.cpp (JSON serialization, versioning, RLE, legacy support). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/savegame.cpp  
[^9]: Cataclysm: Bright Nights — Official Documentation. https://docs.cataclysmbn.org

[\*] Event representation and polling details are derived from src/input.cpp; see reference [^5].