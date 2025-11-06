# Cataclysm-BN GUI Overlay Integration Points: A Safe, Layered Technical Plan

## Executive Summary: Objectives, Constraints, and Non-Breaking Approach

This plan specifies a non-invasive way to add a graphical user interface (GUI) overlay to Cataclysm: Bright Nights (Cataclysm-BN) while preserving the integrity of the ASCII game display, existing rendering pipeline, input routing, and save/load systems. The approach is deliberately minimal in code改动, aligns with the project’s long-standing SDL2-based architecture, and adheres to established development conventions so that the overlay can be built, tested, and maintained without destabilizing core systems.

The primary objective is to deliver a GUI overlay that coexists with the ASCII renderer and tiles renderer, uses the existing input event system to capture mouse and keyboard interactions, and integrates with the menu and UI framework already present. The plan targets five integration surfaces:

1. Rendering pipeline layering for overlays above the ASCII display.
2. Input event interception and routing for the overlay.
3. Game state and menu system integration through a modal/managed approach.
4. Save/load system enhancements to persist overlay preferences.
5. Character and inventory data structures for GUI bindings.

The guiding constraints are explicit: no breaking changes to existing gameplay, input, rendering, or save/load behaviors; backward compatibility with SDL2, tiles, and ASCII/curses ports; and adherence to the project’s encapsulation and compilation-time minimization practices, including the pointer-to-implementation (PIMPL) idiom where appropriate.[^1][^2][^4]

Success will be measured by: correct layering and z-ordering with no visual artifacts or performance regressions; preserved default input routing when the overlay is inactive; deterministic state transitions for menus and popups; and no increase in JSON or save/load failure modes. The design is informed by Cataclysm-BN’s SDL2-based rendering and input handling, the PIMPL-heavy architecture in game.h, and the established UI management and panel constructs in the codebase.[^1][^2][^3]

## Codebase Context: SDL2, Rendering Modules, Input Handling, and UI/State

Cataclysm-BN is a C++ roguelike built on SDL2 for rendering and input, with support for TrueType font rendering, tile-based graphics, and a curses/ASCII port. The rendering surface, window creation, and event polling are anchored in SDL2, providing a stable cross-platform substrate for both the existing UI and the new overlay.[^2]

The source structure reflects clear separation of concerns. Rendering is represented by sdltiles and cata_tiles for tiles output, cursesport for ASCII/curses, output and drawing_primitives for low-level drawing primitives, and sdl_font, sdl_geometry, and sdl_wrappers for fonts and geometry. Input is handled through input and handle_action, with action for action mapping. UI management is consolidated in ui and ui_manager, with a variety of specialized UIs for inventory, panels, crafting, bionics, and dialogue. The game state is centered in game.*, with player/character classes and auxiliary systems (e.g., map, weather) managed through PIMPL as declared in game.h.[^3]

This context is important for integration: overlay rendering must plug into an existing layering and draw-order model; input must be intercepted only when the overlay is active, then restored cleanly; menu integration must respect UI and panel constructs; save/load must not alter JSON schemas; and GUI bindings must use const-safe accessors into character and inventory structures.

To illustrate the module mapping, Table 1 lists the key files and directories by function.

Table 1. Module-to-Function Map

| Function Area         | Representative Files (src/)                                         | Role in Integration                                                                 |
|----------------------|----------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| Rendering (SDL2)     | sdltiles.cpp/h, sdl_font.cpp/h, sdl_geometry.cpp/h, sdl_wrappers.cpp/h | Window, surface management; font and primitive drawing; wrappers around SDL2       |
| Tiles                | cata_tiles.cpp/h, lightmap.cpp/h, pixel_minimap.cpp/h                | Tile rendering pipeline and minimaps                                                |
| ASCII/Curses         | cursesport.cpp/h, cursesdef.h, ascii_art.cpp/h                       | ASCII/curses output path                                                            |
| Drawing Primitives   | drawing_primitives.cpp/h, output.cpp/h                               | Low-level draw routines; text and primitive helpers                                 |
| Input                | input.cpp/h, handle_action.cpp, action.cpp/h                         | Event polling, action mapping, interception and routing                             |
| UI/Manager           | ui.cpp/h, ui_manager.cpp/h, uistate.h, game_ui.cpp/h                 | Modal/stateful UI integration points; existing UI stack                             |
| Menus/Screens        | main_menu.cpp/h, newcharacter.cpp/h, options.cpp/h, debug_menu.cpp/h | Screens/flows likely used by overlay for setup and configuration                    |
| Game State           | game.cpp/h, game_object.cpp/h, gamemode.cpp/h                        | State ownership and transitions                                                     |
| Character/Inventory  | character.cpp/h, player.cpp/h, inventory_ui.cpp/h, panels.cpp/h      | Data binding for GUI; panel layout                                                  |
| Save/Load            | savegame.cpp, savegame_json.cpp, json.cpp/h, filesystem.cpp/h        | JSON serialization, preferences, file I/O                                           |

This organization, combined with the project’s emphasis on PIMPL in game.h, supports a safe insertion of a GUI overlay as an opt-in augmentation rather than a structural change.[^3][^4]

## Integration Point 1: Rendering Pipeline and GUI Overlay Layering

The rendering integration is designed to add an overlay layer above the existing ASCII/curses or tiles output without modifying core draw functions. Conceptually, the overlay occupies a higher z-order and draws after the base scene. The design assumes that sdltiles and cursesport are the final stages that present the frame to the SDL window; therefore, the overlay is invoked immediately before control returns to the event loop or screen refresh, ensuring deterministic draw order and minimal disturbance to existing code.

Two design variants are viable:

- Variant A: Non-intrusive final-frame compositing. Introduce a single entry point in the rendering path (e.g., at the end of a frame-complete callback or in a present-stage hook) that invokes overlay rendering routines. This avoids changes to primitive functions and confines overlay code to a clearly defined surface.

- Variant B: Manager-driven layering via ui_manager. Register the overlay as a managed layer with ui_manager. This leverages the existing UI layering mechanism to control z-order, visibility, and input focus, and to participate in the UI stack already present in the codebase.

Both variants preserve the ASCII/curses path and the tiles path: overlays draw over whatever the base renderer has composed. Visibility and clipping are governed by the overlay manager; the base renderer is never modified. Font and geometry utilities already used by the project (sdl_font, sdl_geometry, drawing_primitives, output) remain authoritative for text metrics, color handling, and primitive drawing, ensuring that overlay rendering uses the same conventions as the rest of the UI.[^3][^4]

This approach adheres to project conventions by using PIMPL and encapsulation where necessary. If the overlay requires additional state kept within the game class, it can be added via PIMPL in game.h to avoid cascading header inclusions. This minimizes compilation impact and keeps implementation details out of public headers, consistent with best practices.[^4]

## Integration Point 2: Input Event Interception and Processing

The input integration relies on the existing event system. SDL2 events—mouse motion, mouse button, keyboard, and window events—are polled and processed in input and handle_action. The overlay should introduce an interception layer that is active only when the overlay is visible. When inactive, events pass through unchanged.

The interception strategy is straightforward:

- When the overlay is active, input events are first evaluated against overlay focus and hit regions. Events within overlay bounds are consumed by the overlay; other events are optionally passed to the underlying screen to allow simultaneous interaction (e.g., hover effects or resizing near edges) if that is desired for the specific overlay.

- When the overlay is inactive, the event flow is unaffected; the overlay installs no hooks and incurs no overhead.

Care is taken with action mapping. Cataclysm-BN supports key binding customization; the overlay’s hotkeys must not conflict with existing bindings. In practice, this means overlay hotkeys are either configurable through the options system or limited to combinations unlikely to collide with gameplay actions. The existing action and options modules are the right place to declare and manage these bindings.

To make the flow explicit, Table 2 summarizes the event flow and interception points.

Table 2. Event Flow Matrix

| Source        | Dispatcher           | Interception Point            | Handler                       | Consumption Rule                                        |
|---------------|----------------------|-------------------------------|-------------------------------|---------------------------------------------------------|
| SDL2 events   | input + handle_action | Pre-input routing (overlay active) | Overlay input manager          | Events within overlay hit regions are consumed          |
| SDL2 events   | input + handle_action | Normal routing (overlay inactive) | Existing action handlers       | No change; no interception                              |
| Overlay UI    | ui_manager            | Focus/visibility management   | Overlay widgets                | Events filtered by overlay focus and visibility         |
| Global hotkeys| options + action      | On-keypress (overlay active)  | Overlay hotkey handlers        | Consumed by overlay if active; otherwise ignored        |

This design respects the existing event pipeline and avoids invasive changes to input.cpp or handle_action. The overlay provides adapters that bind to the event queue only when it is active, preserving default behavior and compatibility.[^3]

## Integration Point 3: Game State and Menu System Integration

The menu/state integration uses a modal overlay model governed by ui_manager. When the overlay is open, it is treated as a modal layer with explicit focus. When closed, state is restored to the previous UI screen without side effects. The overlay participates in the existing UI lifecycle: opening, updating per-frame, rendering, and closing.

Menus and screens such as main_menu, newcharacter, options, and debug_menu are examples of the UI flows the overlay should emulate. These modules provide established patterns for modal behavior, state transitions, and consistent interaction. The overlay reuses these constructs rather than inventing a separate state machine, which simplifies integration and reduces risk.

State preservation is explicit: opening the overlay pushes it onto the UI stack, and closing pops it, returning control to the prior UI context. The overlay never takes ownership of gameplay state; it only reads data for display and writes preference changes through well-defined interfaces.

Table 3 outlines the state transitions and ownership.

Table 3. State Transition Map

| Current UI        | Event                  | Next UI            | Ownership Rule                                   |
|-------------------|------------------------|--------------------|--------------------------------------------------|
| Game screen       | Open overlay request   | Overlay (modal)    | Overlay owned by ui_manager; game state unchanged|
| Overlay (modal)   | Close/confirm/cancel   | Prior UI context   | Overlay popped; state restored; no side effects  |
| Options/Config    | Apply overlay settings | Options/Config     | Overlay writes preferences; options owns commit  |
| Main menu         | Launch overlay setup   | Overlay (modal)    | Overlay temporary; returns to main menu on exit  |

By using ui_manager, the overlay behaves like any other UI layer, benefiting from existing focus management, visibility handling, and deterministic transitions.[^3]

## Integration Point 4: Save/Load and Preferences Enhancement for GUI

The overlay’s configurable settings—enabled/disabled, theme, position, opacity, and key bindings—must persist through the game’s save/load system without altering core gameplay JSON schemas. The safe approach is to store overlay preferences in a dedicated JSON file under the user data configuration area, using the existing savegame_json and filesystem utilities.

Backup and rollback are critical. On save, the overlay writes a new preferences file only after writing a temporary copy; on load, it validates the JSON before applying. This avoids partial writes and ensures that malformed configuration does not break the game. If options are introduced that could conflict with key bindings, the options module is extended to declare them, and conflicts are resolved through validation and user prompts.

Table 4 captures the I/O interface.

Table 4. Save/Load I/O Interface

| Key                     | Type    | Default | Location               | Validation Rule                                        |
|-------------------------|---------|---------|------------------------|--------------------------------------------------------|
| overlay.enabled         | bool    | false   | Overlay preferences JSON | Must be boolean; if invalid, revert to default         |
| overlay.theme           | string  | “default” | Overlay preferences JSON | Must be known theme identifier; else fallback          |
| overlay.position        | string  | “bottom-right” | Overlay preferences JSON | Must be one of allowed positions; else fallback         |
| overlay.opacity         | number  | 0.8     | Overlay preferences JSON | Clamped to [0.0, 1.0]; if out of range, clamp          |
| overlay.hotkeys         | array   | []      | Overlay preferences JSON | Each entry validated against options bindings          |

This design respects the project’s JSON handling conventions, uses existing serialization and file I/O routines, and isolates overlay preferences from world-state saves, reducing risk to save integrity.[^3]

## Integration Point 5: Data Binding to Character and Inventory Structures

The overlay reads character and inventory data for display through const-safe accessors in character and player. These classes expose player state, stats, equipment, and inventory information that can be bound to GUI widgets. The inventory_ui module and panels module offer additional guidance on how to present data in panels and lists without altering underlying data structures.

The binding model is read-only for gameplay-critical data. Inventory changes and equipment modifications remain the responsibility of existing systems; the overlay only displays snapshots updated per frame or on change events. If the overlay provides inventory filtering or search, the result is a view into existing data; no new item IDs or structures are introduced.

Table 5 lists the principal data bindings.

Table 5. Data Binding Map

| GUI Element            | Source Class/Module     | Read/Write | Update Strategy                              |
|------------------------|-------------------------|------------|----------------------------------------------|
| Player stats           | character.cpp/h, player.cpp/h | Read       | Per-frame poll; refresh on state change events|
| Equipment slots        | character.cpp/h         | Read       | On tick and after equip/unequip actions      |
| Inventory list         | inventory_ui.cpp/h, character.cpp/h | Read       | On inventory change; lazy refresh during hover|
| Panel layout           | panels.cpp/h            | Read/Write | Rebuild on theme change; instant apply        |
| Status effects         | character.cpp/h         | Read       | On effect added/removed; per-frame for timers|

This approach aligns with encapsulation and information hiding principles in the project’s best practices, avoiding unnecessary getters/setters and preventing UI-induced data coupling.[^3][^4]

## Non-Breaking Design: Extensibility, Encapsulation, and Backward Compatibility

The overlay is designed as a self-contained module that exposes minimal public interfaces. All implementation details are hidden using PIMPL where appropriate, following the model in game.h. This reduces compilation dependencies and prevents pollution of headers across the codebase. The overlay does not alter core JSON schemas for world or character state; preferences remain in a separate file, validated and isolated from gameplay serialization.

Backward compatibility is preserved across the ASCII/curses and tiles paths. The overlay’s presence is optional and controlled by preferences; when disabled, the game behaves exactly as before. Input bindings are configurable and validated; conflicts are detected at configuration time and resolved without affecting default gameplay mappings.

Adherence to naming and type usage conventions maintains code quality. The overlay follows snake_case naming, uses enum class for binding states and modal flags, and avoids pair/tuple in headers by introducing named structs where needed. These choices reduce the risk of regressions and improve maintainability.[^4]

## Testing and Validation Plan

Testing ensures that the overlay integrates safely and performs as expected. Unit tests validate input interception toggling, z-order rendering correctness, and JSON serialization round-trips for preferences. Integration tests simulate SDL2 event sequences, including edge cases like window focus loss and rapid input bursts, verifying that the overlay neither blocks nor hijacks events inappropriately. Compatibility tests run across ASCII/curses and tiles configurations and across different platforms supported by SDL2 to ensure consistent behavior and performance.

To make expectations clear, Table 6 defines acceptance criteria per integration point.

Table 6. Acceptance Criteria Matrix

| Integration Point       | Test Type            | Expected Outcome                                                 | Pass/Fail Criteria                                       |
|-------------------------|----------------------|------------------------------------------------------------------|----------------------------------------------------------|
| Rendering layering      | Visual + Unit        | Overlay draws above ASCII/tiles; no artifacts; stable z-order    | No flicker; deterministic draw order; perf within budget |
| Input interception      | Integration + Unit   | Overlay consumes events when active; passes when inactive        | No丢失 events; no duplicate handling; configurable hotkeys validated |
| Menu/state transitions  | Integration          | Modal lifecycle correct; state restored after close              | No state leak; deterministic transitions; no side effects|
| Save/load preferences   | Unit + Integration   | Preferences saved/loaded; rollback on error; schema unchanged    | No corruption; invalid values fall back gracefully       |
| Character/inventory     | Unit + Integration   | Data bindings accurate; no write-through; consistent updates     | No desync; accurate snapshots; no performance regressions|

Performance expectations are conservative: overlay rendering adds minimal per-frame cost and does not alter the baseline throughput of the main loop. Render tests and input event tests run on continuous integration (CI) with representative scenarios and capture regressions through thresholds and diffs.[^2]

## Implementation Roadmap and Risk Register

A phased approach mitigates risk and allows for incremental validation:

- Phase 1: Foundations. Introduce overlay module skeleton, define PIMPL-backed state, and add hooks for rendering and input. Integrate with ui_manager for layering and focus. No user-visible changes until enabled via preferences.

- Phase 2: Rendering and Input. Implement final-frame compositing or ui_manager registration, overlay input adapters, and z-order handling. Validate across ASCII and tiles configurations.

- Phase 3: Menu/State. Implement modal lifecycle, state transitions, and focus management. Ensure deterministic restoration and no gameplay state ownership.

- Phase 4: Save/Load. Add preferences JSON with validation and rollback, and integrate with options for configurable bindings.

- Phase 5: Data Binding. Bind character and inventory data through const accessors; implement panel updates on change events.

- Phase 6: Testing. Cover unit, integration, and performance tests; fix issues; finalize acceptance criteria matrix.

Key risks and mitigations:

- Event contention. Overlays consuming needed events. Mitigation: strict consumption rules, configurable hotkeys, and pass-through for non-overlay events when appropriate.

- Z-order conflicts. Overlay drawn under or above unintended layers. Mitigation: explicit z-order via ui_manager and deterministic compositing.

- JSON schema changes affecting saves. Mitigation: isolate preferences to separate file; validate before apply; rollback on error.

- Performance regressions. Mitigation: measure per-frame cost; optimize draw routines; cap update frequency where feasible.

Rollback and feature flags are used throughout. The overlay can be disabled via preferences with no side effects; module flags allow enabling or disabling each integration point independently.

Table 7 enumerates the phases and deliverables.

Table 7. Phases and Deliverables

| Phase | Tasks                                              | Outputs                                 | Entry Criteria                         | Exit Criteria                                |
|-------|----------------------------------------------------|------------------------------------------|----------------------------------------|----------------------------------------------|
| 1     | Skeleton, PIMPL, hooks                             | Overlay module; interfaces               | Codebase baseline                      | Builds pass; hooks present but inactive       |
| 2     | Rendering, input adapters, z-order                 | Overlay draws; input interception        | Phase 1 complete                       | Visual + input tests pass                     |
| 3     | Modal lifecycle, state transitions                 | Overlay modal integrated with ui_manager | Phase 2 complete                       | State tests pass; no side effects             |
| 4     | Preferences JSON, validation, options bindings     | Preferences file; options integration    | Phase 3 complete                       | Round-trip tests pass; rollback verified      |
| 5     | Data bindings, panel updates                       | GUI elements bound to game data          | Phase 4 complete                       | Accuracy and perf metrics within targets      |
| 6     | Testing, CI integration, fixes                     | Test suites; CI results                  | Phase 5 complete                       | All acceptance criteria met                    |

## Known Information Gaps

A few details require confirmation before implementation or in early phases:

- Exact draw order and frame lifecycle in the main loop (where to best insert final-frame compositing hooks).
- Canonical input interception pattern currently used by the project beyond basic event polling (priorities and focus rules).
- Current UI modal stacking and focus management details within ui_manager for state transitions.
- Whether any existing JSON preferences are loaded from user data directories and exact file naming conventions for overlay preferences.
- Platform-specific input quirks across SDL2 targets that could affect mouse/keyboard handling.
- Performance budgets and telemetry available to assess rendering overlays’ per-frame cost.
- Licensing or policy constraints on introducing new dependencies or significant build changes.

These gaps do not block the overall plan but must be addressed during Phase 1 and 2 to ensure precise, non-breaking integration.

## References

[^1]: Cataclysm: Bright Nights - Official Documentation. https://docs.cataclysmbn.org  
[^2]: Cataclysm: Bright Nights GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN  
[^3]: Cataclysm-BN src/ Directory - Source Files Overview. https://github.com/cataclysmbnteam/Cataclysm-BN/tree/master/src  
[^4]: game.h (Cataclysm-BN) - PIMPL Usage and Core Components. https://github.com/cataclysmbnteam/Cataclysm-BN/blob/master/src/game.h