# Cataclysm-BN Dear ImGui GUI Overlay Architecture

Technical Architecture and Design Specification

## Executive Summary and Objectives

This document specifies a safe, high‑performance overlay architecture for Cataclysm: Bright Nights (Cataclysm‑BN) using Dear ImGui. The design is non‑invasive: it preserves the existing pseudo‑curses ASCII renderer and the SDL2 tiles pipeline, introduces a well‑defined dual input pathway for mouse and keyboard, and integrates via the established UI lifecycle and event bus. The overlay draws after the base frame has been committed, using batched vertex buffers to minimize overhead and avoid contention with the ASCII surface. Preferences are persisted separately from world saves, and the feature remains strictly opt‑in to guarantee backward compatibility.

The proposed pipeline ensures deterministic z‑order and clipping by rendering the overlay immediately after the base scene is presented. On graphical builds, ImGui SDL2Renderer is used; on ASCII builds, ImGui rendering is suppressed to avoid dependencies and ensure identical gameplay behavior. The integration path is aligned with the known structure of the main loop and UI manager so that overlay opening, updating, rendering, and closing are coordinated deterministically with screen resizing and redraws[^2][^3][^6].

Objectives are as follows:

- Seamless layering over the ASCII display without modifying the cursesport buffer.
- Use of ImGui to draw only overlay widgets (e.g., tool panels, HUD aids) above the base scene.
- Dual input routing to handle both mouse (ImGui) and keyboard (input_context) deterministically and with configurable priority.
- Integration with UI manager, input contexts, and the game’s event bus to avoid state coupling or side effects.
- Isolated preferences JSON separate from world serialization, with robust validation and rollback.
- Performance optimization, including batching, high‑DPI scaling, minimize‑pause behavior, and gating when not enabled[^26][^24][^23].

Success criteria:

- Correct z‑order with no flicker, tears, or input contention.
- No regressions in frame pacing or latency on both ASCII and tiles builds.
- Deterministic state transitions and consistent input behavior when overlay is active or inactive.
- Isolated preferences, robust save/load, and safe rollback on malformed JSON.

To make ownership explicit, Table 1 maps the system responsibilities to concrete components. The mapping clarifies where the overlay hooks into existing subsystems and how responsibilities are partitioned.

Table 1. System responsibility matrix

| Responsibility | Existing BN Component(s) | Overlay Component(s) | Notes |
|---|---|---|---|
| Base frame rendering | cursesport, sdltiles | ImGuiRenderer (optional) | Overlay draws only in graphical builds; ASCII builds suppress ImGui rendering |
| Overlay UI lifecycle | ui_manager, ui_adaptor | OverlayUI, OverlayManager | Overlay acts as a modal layer and participates in resize/redraw hooks |
| Input routing | input_manager, input_context | InputRouter, ImGui backends | ImGui consumes mouse/keyboard when overlay active; game input otherwise |
| Event distribution | event_bus | Bridge adapters | GUI events publish to bus; gameplay events subscribe via bus |
| Preferences | savegame_json (isolated JSON), options | PrefsStore | Preferences JSON is isolated; options integration optional |
| Data binding | character, player, inventory_ui | Panel bindings | Read‑only snapshot bindings; no write‑through to gameplay state |
| Performance optimization | main loop, renderer | OverlayManager | Gating, batching, minimize‑pause; metrics and budgets via OverlayManager |
| DPI and windowing | sdl_font, sdl_geometry, sdl_wrappers | OverlayDPI | Shared utilities; overlay computes scale and font sizing |
| Memory management | n/a | OverlayResourcePool | Font/texture lifecycles, arena allocators, scoped cleanup |

Information gaps to be confirmed during implementation include the exact present hook in sdltiles, detailed internal composition of cata_tiles, the authoritative panel registry managed by panel_manager, platform‑specific input quirks, precise per‑platform font/dpi handling, and quantitative performance budgets. These are explicitly noted where relevant.

## Architectural Context and Constraints

Cataclysm‑BN features a layered rendering architecture. cursesport maintains in‑memory pseudo‑curses buffers that store lines of colored/attributed text cells. sdltiles consumes these buffers and composes the final frame in the SDL2‑based tiles path. The ASCII display and the tiles renderer share common upstream UI code that renders to catacurses::window objects, while the backends differ in how the final scene is presented. This arrangement allows the same UI and gameplay code to run in terminal‑like and graphical environments, with cursesport providing a double‑buffered model (wnoutrefresh, doupdate) for efficient redraws[^4][^1].

Input handling is event‑driven and modal. A global input_manager loads keybindings and manages timeouts. Per‑mode input_context instances register action names and filter device events into actionable IDs. The system supports mouse coordinates capture and gamepad. Contexts are constructed per screen or mode and are disposed when the screen exits. This indirection is central to our dual input design, as it enables overlay events to be routed without breaking the default gameplay bindings[^5].

UI lifecycle coordination is handled via ui_manager and ui_adaptor. Components subscribe for redraw and screen resize callbacks, and ui_manager orchestrates when to invalidate and refresh panels. panel_manager organizes modular side panels, though an authoritative list of panels and their exact layout has not been extracted. The overlay must respect this lifecycle and should not implement its own resize logic, instead using ui_adaptor hooks to recompute layout and redraw[^6].

Constraints guiding this design:

- The overlay must not modify core cursesport structures or sdltiles/cata_tiles composition; it may only hook at well‑defined integration points after the base frame is complete.
- The ASCII path should not incur any new dependencies or behaviors when the overlay is disabled.
- All changes are opt‑in; no bindings or JSON schemas are altered for world/character state.
- The design is PIMPL‑friendly, with overlay state held in implementation classes to avoid header pollution[^8].

Table 2 cross‑references rendering files to integration points for the overlay.

Table 2. Rendering files and integration points

| File(s) | Role | Integration Point | Overlay Behavior |
|---|---|---|---|
| cursesport.cpp | Pseudo‑curses buffers, double buffering | Not modified | Overlay never writes to curses buffers; ASCII builds suppress ImGui |
| sdltiles.cpp | Final compositor for graphical builds | Present/final frame hook | Overlay draws immediately after base scene is presented |
| cata_tiles.cpp | Tiles rendering | Coordinate with sdltiles present | Overlay respects tiles view and clipping; no interference |
| drawing_primitives, output | Primitives and text helpers | Shared utilities | Overlay may use same helpers for consistent styling and metrics |
| sdl_font, sdl_geometry, sdl_wrappers | Fonts, geometry, SDL wrappers | DPI and font sizing | Overlay computes scale and uses project conventions for text |

## Rendering Pipeline Design: Dear ImGui Overlay Layering

We define the overlay as a post‑compositor that draws only in graphical builds. The base scene (ASCII or tiles) is committed first, and then ImGui renders its UI on top. This ensures a clean z‑order: base scene underneath, overlay widgets on top. No writes occur to cursesport buffers, and the ASCII path is unchanged.

On sdltiles, we insert a present‑stage hook. Two variants are viable:

- Variant A: Final‑frame compositing hook. Add a single hook at the end of the frame’s presentation path in sdltiles that calls into the overlay renderer. This is minimally invasive and makes the draw order deterministic.
- Variant B: UI manager‑driven layering. Register the overlay as a managed layer with ui_manager so that it participates in the established UI stack, z‑order, visibility, and focus management. This provides lifecycle guarantees and reuse of existing redraw/resize hooks[^6].

The recommended approach is a hybrid: the overlay registers with ui_manager for lifecycle and focus, and the render integration uses a final‑frame compositing hook in sdltiles. This combination minimizes changes, clarifies responsibilities, and guarantees consistent behavior across modes and screen transitions.

ImGui initialization and shutdown are conditional and guarded:

- Graphical builds initialize ImGui SDL2 backends and the SDL2Renderer.
- ASCII builds skip ImGui initialization and rendering entirely to avoid dependencies.
- When disabled via preferences, all overlay draw/input code is inactive.

High‑DPI handling reuses existing font/geometry utilities. The overlay computes a DPI scale and passes it to ImGui, aligning widget sizes and fonts with the project’s visual standards[^26].

Table 3 clarifies the draw order and z‑policy.

Table 3. Draw order and z‑policy

| Stage | Layer | Notes |
|---|---|---|
| 1 | Base scene | cursesport or tiles composition via sdltiles; double‑buffered updates applied |
| 2 | Overlay widgets | ImGui renders via SDL2Renderer; clipped to viewport; batched vertex buffers |
| 3 | Present | Swap/present to screen; overlay drawn above base scene |

Table 4 distinguishes graphical vs ASCII builds.

Table 4. Graphical vs ASCII build behavior

| Build Type | ImGui Rendering | Input | Event Consumption | Notes |
|---|---|---|---|---|
| Graphical (sdltiles) | Enabled; draws post‑compositor | ImGui backends for mouse/keyboard | ImGui consumes overlay events; pass‑through for others | DPI scaling and batching apply |
| ASCII (cursesport) | Disabled | No ImGui events | No overlay consumption | Ensures identical behavior; no new dependencies |

### Class Diagram: Overlay Rendering and Lifecycle

The rendering and lifecycle classes are structured to avoid header pollution and clarify responsibilities. PIMPL is used throughout to keep implementation detail out of public interfaces[^8].

- OverlayManager: owns the overlay lifecycle (open/update/close), registration with ui_manager, gating by preferences, and global toggles.
- OverlayRenderer: wraps ImGui integration, mediates draw order, and enforces batching. Provides hooks for sdltiles present.
- OverlayUI: owns widgets, panel definitions, and data bindings. Implements per‑frame describe‑and‑draw semantics.
- OverlayDPI: computes and applies DPI scaling, font sizing, and text metrics alignment.
- PrefsStore: manages preferences JSON, isolation, validation, and rollback.
- InputRouter: routes events between input_context and ImGui backends according to focus, visibility, and pass‑through rules.
- Bridge adapters: publish/subscribe to event_bus and mediate GUI events without state coupling.

The relationship to ui_manager/ui_adaptor is direct: OverlayManager subscribes to redraw and screen resize events to recompute layout and re‑draw deterministically.

### Code Sketches (SDL2Renderer Path)

The following minimal snippets illustrate the integration. They are intentionally simplified; the complete implementation follows the patterns described in the SDL2 + Dear ImGui integration guide[^26].

Initialization (graphical build only):

```cpp
// Called once on startup in graphical builds if overlay is enabled.
void OverlayRenderer::Init(SDL_Window* window, SDL_Renderer* sdl_renderer, float dpi_scale) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // avoid creating imgui.ini unless desired

    ImGui_ImplSDL2_InitForSDLRenderer(window, sdl_renderer);
    ImGui_ImplSDLRenderer_Init(sdl_renderer);

    // High-DPI: set renderer scale and pass to ImGui
    SDL_RenderSetScale(sdl_renderer, dpi_scale, dpi_scale);
    io.FontGlobalScale = dpi_scale;

    // Optional: enable keyboard navigation and docking/viewports if project approves
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;
}
```

Present hook in sdltiles:

```cpp
// Invoked after the base frame is presented.
void OverlayRenderer::RenderOverlay() {
    if (!enabled || minimized) return;

    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_ProcessEvent(/* pass SDL_Event list from input here */);
    ImGui::NewFrame();

    // OverlayUI::Draw() produces only ImGui widgets
    OverlayUI::Draw();

    ImGui::Render();
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
}
```

Shutdown:

```cpp
void OverlayRenderer::Shutdown() {
    ImGui_ImplSDLRenderer_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}
```

ASCII build suppression: conditional compilation gates all ImGui init/render/shutdown paths. When the overlay is disabled, the overlay module is idle and incurs zero overhead.

## Dual Input System Architecture

The overlay operates in a dual input model:

- ImGui backends handle mouse and keyboard events when the overlay is active and focused.
- Existing input_context instances continue to handle gameplay actions, including keyboard and gamepad, ensuring no regressions in default bindings.

Events are polled and filtered consistently with the project’s input system. When the overlay is active, we first route events to ImGui to determine if they are consumed by any widget (e.g., hover, click, text entry). Unconsumed events fall through to the active input_context, preserving gameplay input. This pass‑through is configurable, and hotkeys can be bound via options to avoid conflicts. Mouse coordinates are captured relative to the SDL window, with consistent coordinate spaces for tiles and ASCII paths[^5][^26].

Keyboard navigation within ImGui is enabled for accessibility, and optional docking/viewports can be considered if they do not interfere with the project’s own UI patterns[^26][^27].

The priority policy is:

1. Overlay active and focused: ImGui consumes events that intersect overlay hit regions or input widgets. Non‑overlay events optionally pass through to the game context.
2. Overlay inactive: no interception, all events route to input_context unchanged.
3. Global hotkeys: configurable bindings; overlay consumes them only when active.

Table 5 maps event types to the intended handler.

Table 5. Event type → handler routing

| Event Type | Overlay Active? | Focused Widget? | Handler | Consumption Rule |
|---|---|---|---|---|
| Keyboard (hotkey) | Yes | Any | ImGui or overlay hotkey | Consumed by overlay if matches; else pass‑through |
| Keyboard (gameplay) | Yes | No | input_context | Passed through; no change to existing bindings |
| Mouse move | Yes | Yes | ImGui | Consumed for hover/drag within overlay |
| Mouse button | Yes | Yes | ImGui | Consumed for clicks within overlay |
| Mouse wheel | Yes | Yes | ImGui | Consumed for scroll within overlay |
| Gamepad | No | n/a | input_context | Unchanged |
| Any event | No | n/a | input_context | No interception; zero overhead |

### Input Router Design

InputRouter mediates event flow. It encapsulates:

- Focus tracking and visibility: overlay input is enabled only when visible and has focus.
- Pass‑through policy: configurable per event type; default allows pass‑through of non‑overlay events.
- Hotkey registration: through options, mapped to actions; conflict detection and resolution preserve default gameplay bindings.

The lifecycle is explicit: InputRouter is constructed with references to the current input_context and ImGui backends; it is valid only while the overlay is active. On close, the router is destroyed, and event routing reverts to the base path.

Code sketch:

```cpp
void InputRouter::HandleEvent(const SDL_Event& e) {
    if (!overlay_active || !overlay_has_focus) {
        // Pass through unchanged to input_context
        game_context.handle_event(e);
        return;
    }

    // Give ImGui first chance to consume
    if (ImGui_ImplSDL2_ProcessEvent(&e)) {
        // Event consumed by ImGui
        return;
    }

    // Optionally pass through unconsumed events
    if (pass_through) {
        game_context.handle_event(e);
    }
}
```

Integration with options: overlay hotkeys are declared in options, validated for conflicts, and persisted as part of preferences. This aligns with the project’s binding editor and JSON persistence conventions[^5][^6].

## Event Communication Between GUI and Game Systems

The overlay communicates via an adapter that publishes to the existing event_bus. This ensures decoupling: GUI events are observed by gameplay systems without direct function calls or cyclic dependencies. The adapter is a thin shim that maps ImGui UI actions to bus messages and subscribes to gameplay events that require UI updates. State is never owned by the overlay; UI views are snapshots or read‑only references into character and player.

Typical events:

- UI: Open/close panels, apply filter, select item, confirm/cancel actions.
- Gameplay: status effect changes, inventory updates, map interactions, and notifications.

The event flow is: GUI action → adapter → event_bus → subscribers → state changes → UI refresh via ui_adaptor.

Table 6 illustrates representative event types and subscribers.

Table 6. Event types, publishers, subscribers, and effects

| Event Type | Publisher | Subscriber | Effect |
|---|---|---|---|
| ui_overlay_open | OverlayUI | game, ui_manager | Modal push; activate input router |
| ui_overlay_close | OverlayUI | game, ui_manager | Modal pop; restore input context |
| ui_filter_applied | OverlayUI | inventory_ui | Filter inventory list view |
| ui_item_selected | OverlayUI | character, map | Select item; may trigger examine/inspect |
| gameplay_status_change | game | OverlayUI | Update status panels; refresh metrics |
| gameplay_inventory_change | game | OverlayUI | Update inventory snapshot; lazy refresh |
| gameplay_notice | game | OverlayUI | Display transient notice or toast |

Integration leverages the known ownership split: game holds state; UI uses ui_adaptor to redraw; input_contexts manage actions. No overlay code should reach into private members of character or player; all bindings are via const‑safe accessors. The event_bus makes these interactions explicit and testable[^3][^6][^5].

## Memory Management Strategy

Overlay resources—ImGui context, font textures, meshes, and panel data—are managed with clear lifecycles:

- Initialization: create ImGui context; load project‑approved fonts and bake textures; allocate vertex/index buffers; construct overlay panels.
- Active phase: reuse cached fonts and textures; recycle panel instances; allocate transient UI buffers from arena allocators.
- Shutdown: release ImGui fonts/textures; destroy ImGui context; free pools; unregister with ui_manager; detach InputRouter.

Resource ownership:

- OverlayManager owns the overlay lifecycle and resources.
- OverlayRenderer owns ImGui backends, draw data pipelines, and render‑time resources.
- OverlayUI owns panel definitions and data binding handles.
- No long‑lowned references to transient UI buffers; snapshots of gameplay state are read‑only and do not prolong lifetimes.

On minimize/pause: drawing is paused to avoid unnecessary work; event processing may continue if needed for focus or tooltips but is gated to avoid CPU/GPU contention.

Table 7 summarizes resource lifecycles.

Table 7. Resource lifecycle map

| Resource | Creation | Active Use | Destruction | Owner |
|---|---|---|---|---|
| ImGui context | OverlayRenderer::Init | Entire active session | OverlayRenderer::Shutdown | OverlayRenderer |
| Font textures | OverlayRenderer::Init | On demand caching | OverlayRenderer::Shutdown | OverlayRenderer |
| Vertex/index buffers | OverlayUI::Draw (per frame) | Per frame; recycled | Scoped per frame | OverlayUI |
| DPI scale | OverlayDPI::Compute | On resize; cached | Recomputed on resize | OverlayDPI |
| Panel instances | OverlayUI::Init | Visible screens only | OverlayUI::Close | OverlayUI |
| Arena allocators | OverlayUI::BeginFrame | Per frame | OverlayUI::EndFrame | OverlayUI |
| Preferences JSON | PrefsStore::Load | On start; on demand | PrefsStore::Unload | PrefsStore |

## Performance Optimization Techniques

Immediate‑mode GUIs such as Dear ImGui do not perform immediate‑mode rendering; they produce optimized vertex buffers that are batched and rendered efficiently by the host backend[^27]. This design exploits that property by drawing the overlay after the base scene in a single pass, minimizing GPU state changes. The project’s own rendering and text helpers provide consistent metrics and font sizing, which we reuse to avoid re‑implementations.

Key practices:

- Batch and minimize draw calls: ImGui already consolidates draw data into batches. Ensure no spurious state changes.
- High‑DPI handling: compute and apply DPI scaling; keep widget sizes readable and visually consistent[^26].
- Vsync and minimize‑pause: enable vsync; pause UI drawing when minimized to avoid wasteful work[^26].
- Event gating: route only relevant events to ImGui; throttle hover updates and tooltip redraws when not needed.
- Conditional work: avoid UI recompute when state unchanged; use dirty flags for layout/text updates.

To ground expectations, empirical power measurements show immediate‑mode GUIs such as Dear ImGui are within the range of mainstream retained‑mode applications, including under worst‑case test conditions[^24]. Table 8 reproduces representative measurements.

Table 8. Power consumption comparison (Watts)

| Software Category | MacBook Pro (M1 Pro, 2021) | Razer Blade (2015) |
|---|---|---|
| Dear ImGui | 7.5 | 27.8 |
| ImPlot | 8.9 | 29.7 |
| Spotify | 5.8 | 25.6 |
| VS Code | 7.0 | 27.3 |
| YouTube | 11.5 | 30.9 |

These results support the claim that a well‑integrated ImGui overlay can meet demanding frame‑time and power budgets, particularly when drawing is batched, vsync is enabled, and minimize‑pause behavior is implemented[^24][^26].

## Preferences and Persistence

Overlay preferences are isolated from world saves to avoid changes to core JSON schemas. A dedicated preferences file stores overlay enabling, theme, position, opacity, and hotkeys. Validation rules ensure malformed values revert to defaults; write operations use a temporary copy to guarantee rollback. Optional integration with the options system allows conflicts to be detected and resolved before persistence[^7][^6].

Table 9 defines the preferences schema.

Table 9. Preferences schema

| Key | Type | Default | Location | Validation |
|---|---|---|---|---|
| overlay.enabled | bool | false | overlay_prefs.json | Must be boolean; revert on parse error |
| overlay.theme | string | “default” | overlay_prefs.json | Known theme identifier; else fallback |
| overlay.position | string | “bottom-right” | overlay_prefs.json | One of allowed positions; else fallback |
| overlay.opacity | number | 0.8 | overlay_prefs.json | Clamp [0.0, 1.0] |
| overlay.hotkeys | array | [] | overlay_prefs.json | Each entry validated against options bindings |

Persistence is integrated with save/load by performing overlay preference writes only after a temporary copy is safely written, and applying preferences after validating the JSON. The overlay never reads or writes world/character state JSON, ensuring strict isolation[^7].

## Data Binding to Character and Inventory

Overlay UI binds to const‑safe accessors in character and player. inventory_ui and panels modules provide patterns for list views, filtering, and layout. Bindings are read‑only; gameplay remains authoritative. UI updates are event‑driven, with lazy refresh during hover and on‑tick refresh for time‑sensitive metrics.

Table 10 lists representative data bindings.

Table 10. Data binding map

| GUI Element | Source | Read/Write | Update Strategy |
|---|---|---|---|
| Player stats | character/player | Read | On state change; per‑frame for time‑dependent |
| Equipment slots | character | Read | After equip/unequip events |
| Inventory list | inventory_ui, character | Read | On inventory change; lazy refresh during hover |
| Panel layout | panels | Read/Write | Rebuild on theme change; instant apply |
| Status effects | character | Read | On effect added/removed; per‑frame for timers |

This approach avoids tight coupling and ensures that overlay panels remain views into the game’s authoritative state, consistent with the project’s encapsulation principles[^6][^3].

## Testing and Validation

Testing spans unit, integration, and compatibility. The goal is to prove that layering is correct, input routing is deterministic, preferences persist safely, and data bindings are accurate—without regressions in frame pacing or input latency.

- Visual validation: ensure overlay renders above base scene, with no artifacts or flicker, and respects z‑order and clipping.
- Integration tests: simulate SDL2 event sequences, including rapid bursts, window focus loss, and minimize/restore, to verify no lost or duplicate events and correct pass‑through behavior.
- Compatibility tests: run across ASCII and tiles configurations, and across supported platforms, to ensure consistent behavior and performance.
- Performance tests: measure per‑frame cost and validate against budgeted thresholds.

Table 11 captures acceptance criteria per integration point.

Table 11. Acceptance criteria matrix

| Integration Point | Test Type | Expected Outcome | Pass/Fail Criteria |
|---|---|---|---|
| Rendering layering | Visual + Unit | Overlay draws above ASCII/tiles; deterministic | No flicker; correct z‑order; perf within budget |
| Input interception | Integration + Unit | Overlay consumes events when active; passes otherwise | No lost/duplicate events; configurable hotkeys validated |
| Menu/state transitions | Integration | Modal lifecycle correct; state restored after close | No state leak; deterministic transitions |
| Save/load preferences | Unit + Integration | Preferences saved/loaded; rollback on error | No corruption; invalid values fall back gracefully |
| Character/inventory | Unit + Integration | Bindings accurate; no write‑through | No desync; accurate snapshots; perf within budget |

CI procedures include scenario tests and regression detection. Performance expectations remain conservative: overlay rendering adds minimal per‑frame cost and does not alter baseline throughput.

## Implementation Roadmap and Risk Register

A phased plan reduces risk and enables incremental validation.

Phase 1: Foundations
- Introduce overlay module skeleton; define PIMPL‑backed state; add hooks for rendering and input.
- Integrate with ui_manager for lifecycle, redraw, and resize.

Phase 2: Rendering and Input
- Implement final‑frame compositing or ui_manager registration; add overlay input adapters; validate z‑order and clipping.
- Ensure correct behavior across ASCII and tiles.

Phase 3: Menu/State
- Implement modal lifecycle and focus management; ensure deterministic restoration and no side effects.

Phase 4: Save/Load
- Add preferences JSON with validation and rollback; integrate optional options bindings.

Phase 5: Data Binding
- Bind character/inventory data via const accessors; implement panel updates on change events.

Phase 6: Testing
- Cover unit, integration, and performance tests; fix issues; finalize acceptance criteria.

Key risks and mitigations:

- Event contention: mitigate with strict consumption rules, configurable hotkeys, and pass‑through defaults.
- Z‑order conflicts: mitigate with explicit compositing hook and ui_manager‑managed visibility.
- JSON schema changes: mitigate by isolating preferences; validate before apply; rollback on error.
- Performance regressions: mitigate with batching, minimize‑pause, and per‑frame metrics; gate work to when overlay is active.

Table 12 enumerates phases and deliverables.

Table 12. Phases and deliverables

| Phase | Tasks | Outputs | Entry Criteria | Exit Criteria |
|---|---|---|---|---|
| 1 | Skeleton, PIMPL, hooks | Overlay module; interfaces | Codebase baseline | Builds pass; hooks inactive by default |
| 2 | Rendering, input adapters, z‑order | Overlay draws; input interception | Phase 1 | Visual + input tests pass |
| 3 | Modal lifecycle, transitions | Overlay modal with ui_manager | Phase 2 | State tests pass; no side effects |
| 4 | Preferences JSON, validation | Prefs file; options integration | Phase 3 | Round‑trip tests pass; rollback verified |
| 5 | Data bindings, panel updates | GUI elements bound to game data | Phase 4 | Accuracy and perf within targets |
| 6 | Testing, CI integration | Test suites; CI results | Phase 5 | All acceptance criteria met |

## Class Diagram and Module Breakdown

The module map clarifies files and responsibilities for both existing BN code and the overlay components. Overlay code uses PIMPL to keep headers clean and minimize compilation impact[^8][^9].

Table 13. Module‑to‑function map

| Function Area | Representative Files (src/) | Role in Integration |
|---|---|---|
| Rendering (SDL2) | sdltiles.cpp/h, sdl_font.cpp/h, sdl_geometry.cpp/h, sdl_wrappers.cpp/h | Window, surface, fonts, primitives; present hook for overlay |
| Tiles | cata_tiles.cpp/h, lightmap.cpp/h, pixel_minimap.cpp/h | Tile pipeline; coordinate with overlay clipping |
| ASCII/Curses | cursesport.cpp/h, cursesdef.h, ascii_art.cpp/h | ASCII path; ImGui disabled; no changes to buffers |
| Drawing Primitives | drawing_primitives.cpp/h, output.cpp/h | Text and primitive helpers; reuse for consistency |
| Input | input.cpp/h, handle_action.cpp, action.cpp/h | Event polling and action mapping; InputRouter integration |
| UI/Manager | ui.cpp/h, ui_manager.cpp/h, uistate.h, game_ui.cpp/h | Lifecycle hooks; modal behavior; resize/redraw |
| Menus/Screens | main_menu.cpp/h, newcharacter.cpp/h, options.cpp/h, debug_menu.cpp/h | Patterns for screens; overlay behaves analogously |
| Game State | game.cpp/h, game_object.cpp/h, gamemode.cpp/h | Event bus; state ownership; updates and timing |
| Character/Inventory | character.cpp/h, player.cpp/h, inventory_ui.cpp/h, panels.cpp/h | Data binding; read‑only references; panel layout patterns |
| Save/Load | savegame.cpp, savegame_json.cpp, json.cpp/h, filesystem.cpp/h | Preferences JSON isolation; validation; rollback |
| Overlay | overlay_manager.cpp/h, overlay_renderer.cpp/h, overlay_ui.cpp/h, overlay_dpi.cpp/h, prefs_store.cpp/h, input_router.cpp/h | Overlay lifecycle, rendering, widgets, DPI, preferences, input routing |

Class responsibilities (selected):

- OverlayManager: open/update/close; register with ui_manager; gate by preferences; orchestrates lifecycle.
- OverlayRenderer: ImGui backends; present hook; batching; draw ordering; optional docking/viewports configuration.
- OverlayUI: widgets and panel definitions; ImGui::NewFrame → DescribeAndDraw → ImGui::Render pattern.
- OverlayDPI: compute DPI scale; font sizing; coordinate space alignment.
- PrefsStore: load/save/validate overlay_prefs.json; rollback on error; integrate with options.
- InputRouter: focus and visibility; event pre‑filter for ImGui; pass‑through to input_context; hotkey registration.
- Bridge adapters: event_bus publication/subscription; mapping GUI events to gameplay effects and vice versa.

## Code Examples and Sketches

The following sketches illustrate core patterns. They are simplified; full implementation follows the project’s conventions and the SDL2 + ImGui integration guide[^26][^27].

Lifecycle orchestration:

```cpp
// Pseudocode illustrating overlay lifecycle within the main loop
void game::do_turn() {
    // ... existing input, action dispatch, time advance, UI refresh ...

    // ui_manager.redraw may be called by screens; overlay is a managed layer
    ui_manager.update();

    // sdltiles present hook triggers overlay rendering post-compositor
    if (overlay_enabled && graphical_build) {
        overlay_renderer.RenderOverlay();
    }

    // ... autosave heuristics, next turn or exit evaluation ...
}
```

SDL2Renderer ImGui integration:

```cpp
// Initialize ImGui once if overlay enabled and build is graphical
if (overlay_enabled && is_graphical) {
    overlay_renderer.Init(window, sdl_renderer, dpi_scale);
}

// Each frame:
ImGui_ImplSDLRenderer_NewFrame();
ImGui_ImplSDL2_ProcessEvent(&sdl_event);
ImGui::NewFrame();

overlay_ui.Draw(); // widgets built with ImGui

ImGui::Render();
ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
```

Asynchronous event bus communication:

```cpp
// Publish a GUI event
void OverlayUI::ConfirmSelection() {
    event_bus.Publish(ui_item_selected{ selected_id, context });
}

// Subscribe to gameplay event
void OverlayUI::OnStatusChange(const gameplay_status_change& e) {
    status_model.Update(e.status);
    MarkDirty(UI_REDRAW_STATUS);
}
```

Preferences store with validation and rollback:

```cpp
bool PrefsStore::Save(const OverlayPrefs& p) {
    OverlayPrefs tmp = p;
    if (!Validate(tmp)) return false;

    // write to temp, then replace atomically
    if (write_file(temp_path, tmp) && replace_file(temp_path, prefs_path)) {
        return true;
    }
    return false;
}
```

ASCII build suppression:

```cpp
#ifdef GRAPHICAL_BUILD
    overlay_renderer.Init(window, sdl_renderer, dpi_scale);
#else
    // No ImGui init; overlay disabled in ASCII builds
#endif
```

## Information Gaps

Several details require confirmation or validation during implementation and testing:

- Exact present hook in sdltiles for final‑frame compositing; ensure consistent integration across minor versions.
- Internal composition of cata_tiles and precise ordering with sdltiles for blending and font loading.
- Authoritative list of panels managed by panel_manager and their layout dependencies.
- Canonical input interception pattern and focus management rules beyond basic polling in input.cpp.
- Platform‑specific input quirks across SDL2 targets that may affect mouse or keyboard behavior.
- Precise per‑platform font/dpi handling, especially on macOS Retina and Wayland.
- Quantitative performance budgets and telemetry to set acceptable per‑frame overhead for the overlay.

These do not block the overall plan but will be resolved in Phases 1 and 2 through targeted review and integration tests.

## Conclusion

This architecture delivers a safe, high‑performance Dear ImGui overlay for Cataclysm‑BN. It respects the ASCII and tiles rendering pipelines, introduces a dual input system that preserves gameplay bindings, and integrates through established UI and event mechanisms. Preferences are isolated, memory is managed explicitly, and performance is optimized using batching, high‑DPI handling, and minimize‑pause behavior. The result is an opt‑in overlay that adds modern UI tooling and panels without compromising the core game experience or maintenance burden.

## References

[^1]: Cataclysm: Bright Nights — GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN  
[^2]: Cataclysm-BN src/main.cpp (entry points, initialization, loop scaffolding). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/main.cpp  
[^3]: Cataclysm-BN src/game.cpp (game state, UI integration, main loop). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/game.cpp  
[^4]: Cataclysm-BN src/cursesport.cpp (pseudo-curses implementation and ASCII rendering). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/cursesport.cpp  
[^5]: Cataclysm-BN src/input.cpp (input manager, contexts, keybindings). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/input.cpp  
[^6]: Cataclysm-BN src/ui.cpp (uilist, UI manager, adaptor, windowing). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/ui.cpp  
[^7]: Cataclysm-BN src/savegame.cpp (JSON serialization, versioning, RLE, legacy support). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/savegame.cpp  
[^8]: game.h (Cataclysm-BN) — PIMPL Usage and Core Components. https://github.com/cataclysmbnteam/Cataclysm-BN/blob/master/src/game.h  
[^9]: Cataclysm-BN src/ Directory — Source Files Overview. https://github.com/cataclysmbnteam/Cataclysm-BN/tree/master/src  
[^23]: CEGUI GitHub Repository. https://github.com/cegui/cegui  
[^24]: Forrest The Woods, “Proving Immediate Mode GUIs are Performant.” https://www.forrestthewoods.com/blog/proving-immediate-mode-guis-are-performant/  
[^25]: Nuklear GitHub Repository. https://github.com/Immediate-Mode-UI/Nuklear  
[^26]: Martin Fieber, “GUI Development with C++, SDL2, and Dear ImGui.” https://martin-fieber.de/blog/gui-development-with-cpp-sdl2-and-dear-imgui/  
[^27]: Nicolas Guillemot, “Dear ImGui” (CppCon 2016). https://www.youtube.com/watch?v=LSRJ1jZq90k  
[^33]: Dear ImGui GitHub Repository. https://github.com/ocornut/imgui