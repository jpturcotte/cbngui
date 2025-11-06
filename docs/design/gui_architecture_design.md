# Dear ImGui Overlay Architecture for Cataclysm-BN: Technical Design Blueprint

Technical Architecture Design Specification

Executive Summary: Non-invasive GUI Overlay Using Dear ImGui

Cataclysm: Bright Nights (BN) already distinguishes itself through a disciplined, layered architecture that cleanly separates rendering, input, UI, and gameplay state. The system maintains a unified ASCII/text surface and supports two distinct backends: a pseudo-curses layer and an SDL2 tiles renderer. Against this foundation, this blueprint presents a safe, efficient, and non-invasive overlay design using Dear ImGui. The overlay draws above the base frame without modifying the ASCII buffer, it integrates with the existing input contexts for both mouse and keyboard, and it communicates through the established event bus and UI manager. Preferences are isolated from world saves, ensuring backward compatibility and a low blast radius for any future changes[^1][^3][^6].

Architecturally, the design adheres to a few key principles. First, post-compositor layering: the base scene (cursesport or sdltiles) is presented first, then the overlay is rendered in a single pass using ImDrawLists, ensuring deterministic z-order and minimal overhead. Second, dual input: mouse and keyboard events are routed to ImGui when the overlay is active; otherwise, events flow through the existing input_context unchanged. Third, modal lifecycle: the overlay participates in the UI manager’s open/update/close sequence, using ui_adaptor hooks for redraw and resize. Fourth, isolated preferences: overlay settings reside in a dedicated JSON file and are validated before application. Finally, performance: the overlay is opt-in, fully disabled in ASCII builds unless explicitly compiled, and implements batching, font/texture reuse, DPI scaling, and pause-on-minimize behavior.

Three design variants were considered. Option A, final-frame compositing, inserts a single present-stage hook in sdltiles for minimal intrusion. Option B, ui_manager layering, treats the overlay as a managed layer within the established UI stack for stronger lifecycle guarantees. Option C, retain-mode GUI (SDL2-native or CEGUI), offers a robust widget set and theming but is heavier to integrate and does not fit the rapid-iteration, overlay-first goal. The recommended path is a hybrid: Option B for lifecycle plus Option A for rendering. This combination ensures that the overlay behaves like a proper UI screen while touching only one integration point in the rendering path[^3][^6][^26][^24][^27].

Risks and mitigations are explicit. Event contention is addressed via configurable priorities, pass-through behavior, and a small, clear consumption policy. Z-order conflicts are prevented by rendering after the base scene and by registering visibility and focus with ui_manager. JSON schema risks are eliminated by isolating preferences outside world saves, with validation and rollback. Performance is controlled through batching, reuse, gating, and pause-on-minimize, all measurable via built-in metrics. The outcome is a blueprint that preserves the existing game experience while enabling modern, efficient overlay tooling for developers and players.

## Rendering Pipeline Design: Overlay Layering without Conflicts

The overlay must render above the existing ASCII or tiles display without altering the cursesport buffer or sdltiles compositing logic. The chosen design achieves this by rendering the overlay immediately after the base frame is presented. The overlay is purely additive: it does not write to curses cells, it does not modify sdltiles blending or font loading, and it respects the viewport and cliprect of the tiles renderer. This ensures that the ASCII path remains identical when the overlay is disabled, and the tiles path remains authoritative for blending, fonts, and present timing.

Two variants are viable:

- Option A: Final-frame compositing hook. Insert a single, clearly identifiable hook in sdltiles at the present stage, after the base frame has been committed. This is minimally invasive and gives a deterministic draw order. It leverages the fact that sdltiles composits window buffers to the final surface; the overlay is an extra layer that draws last[^3][^4].
- Option B: ui_manager-driven layering. Register the overlay as a layer in ui_manager, allowing it to participate in visibility, focus, redraw, and resize hooks. This creates strong lifecycle guarantees and aligns with how other UI screens behave in BN, improving maintainability and testability[^6].

The recommended approach is a hybrid: the overlay registers with ui_manager for lifecycle and focus, while the rendering integration uses a final-frame compositing hook. This reduces the need to modify multiple draw paths and ensures that the overlay is drawn precisely once, after the base scene.

ImGui initialization and shutdown are gated. Graphical builds (sdltiles) initialize ImGui’s SDL2 backends and the renderer backend; ASCII builds suppress the overlay entirely. When overlay.enabled is false, no ImGui code executes. All rendering occurs via ImDrawLists to maximize batching and minimize draw calls. High-DPI and font sizing reuse sdl_font and sdl_geometry conventions so that text and widgets are consistent with the project’s visual standards[^26][^9].

The layering is explicit:

Table 1. Draw order and z-policy

| Stage | Layer | Notes |
|---|---|---|
| 1 | Base scene (cursesport or tiles) | cursesport buffers or tiles composited via sdltiles; doupdate/present applied |
| 2 | Overlay widgets (ImGui) | Rendered via ImDrawLists; clipped to viewport; no writes to curses buffers |
| 3 | Present/swap | Final present to the window; overlay drawn above base scene |

As a further guard, graphical vs ASCII behavior is described below.

Table 2. Graphical vs ASCII build behavior

| Build Type | Overlay Rendering | Input Handling | Integration Notes |
|---|---|---|---|
| Graphical (sdltiles) | Enabled; post-compositor draw | ImGui backends for mouse/keyboard; pass-through for non-overlay events | High-DPI scaling, batching, minimize-pause implemented |
| ASCII (cursesport) | Disabled (compile-time) | No ImGui event handling; default input_context unchanged | Ensures identical gameplay behavior; no new dependencies |

Event contention is avoided by ensuring the overlay consumes only events within its widget regions or dedicated hotkeys, with all other events optionally passed through to the active input_context. This preserves the default gameplay bindings, which is central to BN’s design[^5][^26].

Rendering Pipeline Class Diagram

To clarify the core responsibilities and relationships, the following classes and interfaces are defined. They use PIMPL to minimize header pollution and align with BN’s practices for reducing compilation coupling[^8][^3].

- OverlayManager: owns lifecycle, registration with ui_manager, visibility, and global enable/disable.
- OverlayRenderer: wraps ImGui backends, compositing order, batching, and the present hook.
- OverlayUI: holds widget definitions and data binding; implements per-frame describe-and-draw.
- OverlayDPI: computes and applies DPI scaling and font sizing; aligns with sdl_font/geometry.
- FontManager: loads and caches fonts and textures; exposed to ImGui via AddFontCustomTTF.
- InputAdapter: bridges SDL events to ImGui when overlay active; otherwise, transparent.
- EventBusAdapter: publishes overlay events and subscribes to gameplay events for UI updates.
- PrefsStore: loads/saves overlay preferences to a dedicated JSON; validates and rolls back on error.

OverlayRenderer depends on ImGui’s SDL2 backends, which in turn rely on the SDL2Renderer for efficient batching. OverlayManager subscribes to ui_manager hooks for redraw and resize. OverlayUI publishes events through EventBusAdapter and receives gameplay updates via the same bus. FontManager is optional for custom font injection; otherwise, default ImGui fonts are used and scaled via OverlayDPI.

Code Sketches and API Usage Patterns

The following simplified sketches illustrate the intended usage, consistent with the SDL2 + Dear ImGui integration guide[^26]. They avoid including URLs and focus on patterns.

Initialization (graphical builds only):

```cpp
// Initialize ImGui backends (SDL2 + SDL2Renderer) with DPI scaling.
// Call once at startup if overlay is enabled and build is graphical.
void OverlayRenderer::Init(SDL_Window* window, SDL_Renderer* renderer, float dpi_scale) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;               // avoid imgui.ini by default
    io.LogFilename = nullptr;

    // SDL2 + Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer_Init(renderer);

    // High-DPI: set scale for renderer and ImGui
    SDL_RenderSetScale(renderer, dpi_scale, dpi_scale);
    io.FontGlobalScale = dpi_scale;
}
```

Present hook after base frame:

```cpp
// Called after sdltiles presents the frame.
// Render ImGui overlay widgets in a single pass.
void OverlayRenderer::RenderOverlay() {
    if (!enabled || minimized) return;

    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_ProcessEvent(/* pass latest SDL events */);
    ImGui::NewFrame();

    // OverlayUI builds widgets and panels
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

## Dual Input System: Coordinated Mouse and Keyboard Handling

BN’s input architecture already balances device diversity through a global input_manager and per-screen input_context. Events—keyboard, gamepad, mouse—are polled and translated into actionable IDs, with binding persistence in JSON. This design embraces that pattern and adds a dual pathway for overlay input without contention[^5][^9][^26].

When the overlay is active, mouse and keyboard events are first given to ImGui. If a widget consumes the event (e.g., a hover, click, text input), the event is not passed to the game. If the event is not consumed, the InputAdapter optionally passes it to the active input_context. This ensures that, for example, moving the mouse over an overlay panel does not accidentally move the player, but moving the mouse outside the overlay continues to function as before.

A small, configurable priority policy governs the routing:

Table 3. Event type → handler routing

| Event | Overlay Active? | Consumed by ImGui? | Handler | Pass-Through Rule |
|---|---|---|---|---|
| Mouse move | Yes | Yes | ImGui | No further handling |
| Mouse move | Yes | No | input_context | Pass-through allowed |
| Mouse button | Yes | Yes | ImGui | No further handling |
| Mouse button | Yes | No | input_context | Pass-through allowed |
| Keyboard (hotkey) | Yes | Any | ImGui or overlay hotkey | Consumed if matches overlay binding |
| Keyboard (gameplay) | Yes | No | input_context | Pass-through allowed |
| Any event | No | n/a | input_context | Default unchanged |

Keyboard navigation is enabled within ImGui for accessibility, and docking/viewports can be considered if the project deems them appropriate. Android-specific virtual keys and gamepad handling remain unchanged; the overlay simply adds an additional input surface when visible[^5][^26].

Event contention is a primary concern. The following rules and tests ensure no regressions:

Table 4. Event contention matrix

| Scenario | Expected Behavior | Test Approach |
|---|---|---|
| Overlay hover | Overlay consumes mouse events; gameplay unchanged | Simulate rapid mouse movement over overlay; verify no player movement |
| Overlay text input | ImGui consumes keyboard; no gameplay action | Type into an ImGui field; verify no bound actions fire |
| Pass-through needed | Non-overlay events reach input_context | Generate events outside overlay regions; verify default behavior |
| Gameplay focus | ImGui does not consume when overlay inactive | Disable overlay; verify default event routing unchanged |
| High-frequency input | No duplicate handling or lost events | Burst test keyboard/mouse; count handlers per event; assert idempotence |

Input Router Design and Conflict Resolution

The InputAdapter is responsible for focus, visibility, and pass-through. It allows hotkeys to be defined via options and validated for conflicts. It respects BN’s binding editor and JSON persistence conventions and ensures that the overlay does not hijack gameplay bindings when inactive.

Key aspects:

- Focus tracking: overlay input is active only when the overlay is visible and focused.
- Pass-through: configurable; default is to pass through non-overlay events to input_context.
- Hotkey registration: uses the same options/action system as gameplay; conflicts are detected and resolved.
- Safety: when the overlay is inactive or minimized, all events flow to input_context unchanged.

```cpp
void InputAdapter::HandleEvent(const SDL_Event& e) {
    if (!overlay_active || !overlay_has_focus) {
        // No interception; default gameplay handling.
        game_input_context.HandleEvent(e);
        return;
    }

    // Let ImGui attempt to consume first.
    if (ImGui_ImplSDL2_ProcessEvent(&e)) {
        // Event consumed by an ImGui widget; do not propagate.
        return;
    }

    // Optionally pass through unconsumed events.
    if (pass_through_enabled) {
        game_input_context.HandleEvent(e);
    }
}
```

## Event Communication: GUI ↔ Game Systems via Event Bus

The overlay must communicate with gameplay systems without introducing tight coupling. The established mechanism is the event bus. The overlay publishes UI events and subscribes to gameplay state changes that require UI updates. The data flow is: GUI action → adapter → event bus → subscribers → state changes → UI refresh via ui_adaptor. No overlay code directly mutates gameplay state; all changes occur via published events or by invoking established interfaces (e.g., menus, actions), which themselves are driven through input_contexts[^3][^6][^5].

A representative event taxonomy is shown below.

Table 5. Event taxonomy

| Event Type | Publisher | Subscriber | Effect |
|---|---|---|---|
| ui_overlay_open | OverlayUI | ui_manager, game | Modal push; input routing changes |
| ui_overlay_close | OverlayUI | ui_manager, game | Modal pop; input routing restored |
| ui_filter_applied | OverlayUI | inventory_ui | Apply filter to inventory list |
| ui_item_selected | OverlayUI | character, map | Select item; initiate examine/inspect |
| gameplay_status_change | game | OverlayUI | Update status effects; refresh panels |
| gameplay_inventory_change | game | OverlayUI | Refresh inventory snapshot; update views |
| gameplay_notice | game | OverlayUI | Show transient notification |

Event Bus Integration and Contract

The adapter guarantees decoupling and explicit contracts:

- Publish: overlay publishes UI intents (open/close, filter applied, item selected).
- Subscribe: overlay subscribes to gameplay state changes (status effects, inventory updates, notices).
- Lifecycle: subscriptions are created when the overlay opens and torn down when it closes.
- State access: overlay reads const snapshots; gameplay systems remain authoritative.

This adheres to BN’s layering: the game owns world logic, UI owns presentation and redraw, and input contexts drive actions. The event bus isolates components and makes the flow testable and traceable[^3][^6].

## Memory Management: Efficient Allocation and Cleanup

Overlay lifecycles must be explicit and resource-safe. ImGui contexts, font textures, draw data, and panels are owned by overlay components and cleaned up deterministically. Font/texture caching is mandatory. Arena allocators are used transiently for per-frame UI buffers. On minimize/pause, the overlay pauses drawing to avoid unnecessary work.

Resource ownership and cleanup:

- OverlayManager: owns lifecycle and global resources; shuts down backends deterministically.
- OverlayRenderer: owns ImGui context and renderer backends; caches fonts and textures.
- OverlayUI: owns panel instances and per-frame buffers; recycles memory across frames.
- FontManager (optional): preloads fonts and textures; shares across overlay widgets.

Table 6. Resource lifecycle map

| Resource | Owner | Creation | Active Use | Destruction |
|---|---|---|---|---|
| ImGui context | OverlayRenderer | Init | Active session | Shutdown |
| Renderer backends | OverlayRenderer | Init | Active session | Shutdown |
| Font textures | OverlayRenderer/FontManager | Init or on-demand | Cached; reused | Shutdown or on-unload |
| Draw data buffers | OverlayUI | Per-frame | Recreated per frame | Scoped per frame |
| Panel instances | OverlayUI | On open | Visible screens | On close |
| Arena allocators | OverlayUI | BeginFrame | Per-frame | EndFrame |

Memory ownership follows BN’s PIMPL-friendly style to avoid header pollution and reduce compilation times. Scoped cleanup on close guarantees no leaks or dangling references, even under error conditions[^8][^3].

## Performance Optimization: Minimal Overhead Strategies

The performance objective is straightforward: the overlay must add minimal per-frame cost and never disturb frame pacing. Immediate-mode GUIs are often misunderstood; in practice, they do not perform immediate-mode rendering. Dear ImGui generates optimized vertex buffers that can be batched and rendered efficiently by the host backend. The overlay leverages this by drawing after the base scene in a single pass, using ImDrawLists to minimize draw calls[^27][^26].

Best practices implemented:

- Batching: rely on ImDrawLists; avoid unnecessary Begin/End calls; group widgets by context.
- High-DPI: scale via SDL_RenderSetScale and ImGui IO; keep text and widgets readable across resolutions.
- Vsync: enable vsync through the SDL2Renderer; cap CPU/GPU contention.
- Minimize-pause: skip drawing and most updates when minimized; retain minimal event handling for focus.
- Gating: do no work when the overlay is disabled or inactive; avoid recompute if state unchanged.
- Conditional updates: only rebuild panels and layout when dirty; use timestamps or change events to trigger refresh.

These practices are supported by empirical evidence. Power measurements show immediate-mode GUIs draw within the range of mainstream retained-mode applications, including under worst-case scenarios, implying that CPU/GPU overhead is manageable when integration is sound[^24]. The SDL2 + ImGui guide outlines essential flags and patterns for DPI, event handling, and pause behavior[^26].

To ground expectations, the following table reproduces representative power measurements.

Table 7. Power consumption comparison (Watts)

| Software Category | MacBook Pro (M1 Pro, 2021) | Razer Blade (2015) |
|---|---|---|
| Dear ImGui | 7.5 | 27.8 |
| ImPlot | 8.9 | 29.7 |
| Spotify | 5.8 | 25.6 |
| VS Code | 7.0 | 27.3 |
| YouTube | 11.5 | 30.9 |

Interpretation: even with-intensive plotting libraries (ImPlot), immediate-mode GUIs are within the envelope of common applications. BN’s overlay, which will typically render simple panels and HUD elements, should incur even less overhead. Combined with batching, vsync, and minimize-pause, the net effect is negligible under typical workloads[^24][^26].

## Integration with Character/Inventory Data

Data bindings must be read-only to respect BN’s encapsulation model. The overlay uses const-safe accessors to read player stats, equipment, and inventory snapshots. inventory_ui and panels patterns inform how to present list views, filters, and status effects. Update strategies rely on per-frame polling for time-dependent metrics and event-driven refresh for changes like inventory modifications or status effect updates[^6][^3].

Representative bindings:

Table 8. Data binding map

| GUI Element | Source | Read/Write | Update Strategy |
|---|---|---|---|
| Player stats | character/player | Read | Per-frame poll; refresh on status change events |
| Equipment slots | character | Read | After equip/unequip events |
| Inventory list | inventory_ui, character | Read | On inventory change; lazy refresh on hover |
| Panel layout | panels | Read/Write | Rebuild on theme change; instant apply |
| Status effects | character | Read | On effect added/removed; per-frame for timers |

Thread-safety and race conditions are avoided by treating overlay data as snapshots. No write-through is permitted; gameplay systems remain authoritative. If a GUI action requires mutation (e.g., selecting an item), it publishes an event or invokes a bound action that goes through the established UI/input mechanisms.

## Testing and Validation Plan

Testing validates the non-invasive design and ensures performance and behavior are correct. Unit tests confirm input interception toggling, z-order rendering, and preferences round-trips. Integration tests simulate SDL2 event sequences, including window focus loss and rapid input bursts, verifying that the overlay neither blocks nor hijacks events. Compatibility tests cover ASCII and tiles builds across platforms, and performance tests measure per-frame cost against budgeted thresholds[^2][^5].

Acceptance criteria are captured below.

Table 9. Acceptance criteria matrix

| Integration Point | Test Type | Expected Outcome | Pass/Fail Criteria |
|---|---|---|---|
| Rendering layering | Visual + Unit | Overlay draws above ASCII/tiles; stable z-order | No flicker; deterministic draw order; perf within budget |
| Input interception | Integration + Unit | Overlay consumes when active; passes when inactive | No lost/duplicate events; configurable hotkeys validated |
| Menu/state transitions | Integration | Modal lifecycle correct; state restored after close | No state leak; deterministic transitions |
| Save/load preferences | Unit + Integration | Preferences saved/loaded; rollback on error | No corruption; invalid values fall back gracefully |
| Character/inventory | Unit + Integration | Bindings accurate; no write-through | No desync; accurate snapshots; perf within budget |

Performance expectations: the overlay must not alter baseline throughput. Tests use representative scenarios (open overlay, move mouse, type into a field, apply filter, close overlay) and capture regression via thresholds. CI procedures include these tests to catch any drift.

## Implementation Roadmap and Risk Register

A phased approach reduces risk and yields incremental validation.

Phase 1: Foundations
- Introduce overlay module skeleton, define PIMPL-backed state, add hooks for rendering and input.
- Register with ui_manager for lifecycle, redraw, and resize.

Phase 2: Rendering and Input
- Implement final-frame compositing or ui_manager registration; implement InputAdapter and focus management.
- Validate layering and input routing across ASCII and tiles.

Phase 3: Menu/State
- Implement modal lifecycle, ensure deterministic restoration, and confirm no side effects.

Phase 4: Save/Load
- Add preferences JSON with validation and rollback; integrate optional options bindings.

Phase 5: Data Binding
- Bind character/inventory via const accessors; event-driven refresh for changes.

Phase 6: Testing
- Unit, integration, and performance tests; fix issues; finalize acceptance matrix.

Table 10. Phases and deliverables

| Phase | Tasks | Outputs | Entry Criteria | Exit Criteria |
|---|---|---|---|---|
| 1 | Skeleton, PIMPL, hooks | Overlay module; interfaces | Codebase baseline | Builds pass; hooks inactive by default |
| 2 | Rendering, input adapters | Overlay draws; input interception | Phase 1 | Visual + input tests pass |
| 3 | Modal lifecycle | Overlay modal integrated | Phase 2 | State tests pass; no side effects |
| 4 | Preferences JSON | Prefs file; options integration | Phase 3 | Round-trip tests pass; rollback verified |
| 5 | Data bindings | GUI elements bound to data | Phase 4 | Accuracy and perf within targets |
| 6 | Testing, CI | Test suites; CI results | Phase 5 | All acceptance criteria met |

Risk register and mitigations:

Table 11. Risk register

| Risk | Description | Mitigation |
|---|---|---|
| Event contention | Overlay consumes needed events | Strict consumption rules; configurable hotkeys; pass-through defaults |
| Z-order conflicts | Overlay renders under/over unintended layers | Explicit compositing hook; ui_manager visibility/focus |
| JSON schema changes | Overlay modifies core save schemas | Isolate preferences; validate before apply; rollback on error |
| Performance regressions | Overlay adds per-frame cost | Batching; minimize-pause; metrics; gate when disabled |

## Appendices: Class Diagrams, Code Examples, and API Mapping

Class Responsibility Summary

Table 12. Class responsibilities

| Class | Responsibility | Key Methods |
|---|---|---|
| OverlayManager | Lifecycle, ui_manager registration, enable/disable | Open/Update/Close |
| OverlayRenderer | ImGui backends, present hook, batching | Init/Shutdown/RenderOverlay |
| OverlayUI | Widgets, panels, data binding | Draw (NewFrame→Render) |
| OverlayDPI | DPI scaling, font sizing | ComputeScale/ApplyScale |
| FontManager | Font loading, texture caching | LoadFont/GetFont |
| InputAdapter | Event routing, focus, pass-through | HandleEvent |
| EventBusAdapter | Publish/subscribe for GUI↔game | Publish/Subscribe |
| PrefsStore | Preferences JSON load/save/validate | Load/Save/Validate |

Code Examples

Overlay lifecycle with ui_manager:

```cpp
void OverlayManager::Open() {
    ui_manager.RegisterLayer(this);
    renderer.Init(window, sdl_renderer, dpi_scale);
    ui_adaptor.on_redraw += [] { OverlayUI::Redraw(); };
    ui_adaptor.on_screen_resize += [](int w, int h) { OverlayDPI::Recompute(w, h); };
}

void OverlayManager::Close() {
    renderer.Shutdown();
    ui_manager.UnregisterLayer(this);
}
```

ImGui panel draw with batching and data binding:

```cpp
void OverlayUI::Draw() {
    ImGui::Begin("Overlay Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // Player stats snapshot
    const auto stats = Character::GetStats();
    ImGui::Text("Strength: %d", stats.str);
    ImGui::Text("Dexterity: %d", stats.dex);

    // Inventory list (read-only)
    if (ImGui::BeginListBox("Inventory")) {
        for (const auto& item : Inventory::GetItems()) {
            if (ImGui::Selectable(item.name.c_str())) {
                EventBusAdapter::Publish(ui_item_selected{item.id});
            }
        }
        ImGui::EndListBox();
    }

    ImGui::End();
}
```

Event publishing and subscription:

```cpp
void OverlayUI::OnFilterApplied(const std::string& filter) {
    EventBusAdapter::Publish(ui_filter_applied{filter});
}

void OverlayUI::Init() {
    EventBusAdapter::Subscribe<gameplay_inventory_change>([this](const auto& e) {
        MarkInventoryDirty();
    });
}
```

Preferences store with validation and rollback:

```cpp
bool PrefsStore::Save(const OverlayPrefs& p) {
    OverlayPrefs tmp = p;
    if (!Validate(tmp)) return false;
    // Write to temp file, then replace atomically.
    return write_file(temp_path, tmp) && replace_file(temp_path, prefs_path);
}
```

Module-to-Function Map

Table 13. Module-to-function map

| Function Area | Representative Files | Role in Integration |
|---|---|---|
| Rendering (SDL2) | sdltiles.cpp/h, sdl_font.cpp/h, sdl_geometry.cpp/h | Window, surface, fonts, primitives; present hook |
| Tiles | cata_tiles.cpp/h, lightmap.cpp/h | Tile rendering; coordinate with overlay clipping |
| ASCII/Curses | cursesport.cpp/h | ASCII path; overlay disabled; no buffer changes |
| Drawing Primitives | drawing_primitives.cpp/h, output.cpp/h | Text/primitive helpers; consistent styling |
| Input | input.cpp/h, handle_action.cpp, action.cpp/h | Event polling and mapping; InputAdapter integration |
| UI/Manager | ui.cpp/h, ui_manager.cpp/h, uistate.h | Lifecycle, redraw, resize hooks |
| Menus/Screens | main_menu.cpp/h, newcharacter.cpp/h, options.cpp/h | Screen patterns; overlay behaves analogously |
| Game State | game.cpp/h, game_object.cpp/h | Event bus, state ownership |
| Character/Inventory | character.cpp/h, player.cpp/h, inventory_ui.cpp/h | Data binding; read-only views |
| Save/Load | savegame.cpp, savegame_json.cpp, json.cpp/h | Preferences JSON; validation; rollback |
| Overlay | overlay_manager.cpp/h, overlay_renderer.cpp/h, overlay_ui.cpp/h | Overlay lifecycle, rendering, widgets |

Information Gaps

A few details are not fully extracted and require confirmation during implementation:

- Exact present hook and function composition in sdltiles for final-frame compositing.
- Detailed internal composition of cata_tiles and its interaction with sdltiles during blending and font loading.
- Authoritative list of panels and layout details managed by panel_manager.
- Canonical input interception pattern and focus rules in ui_manager beyond basic polling.
- Platform-specific input quirks across SDL2 targets that may affect mouse/keyboard.
- DPI and font handling details per platform (e.g., macOS Retina, Wayland) and project guidelines.
- Quantitative performance budgets and telemetry available to set acceptable per-frame overhead.

These gaps do not block the proposed design; they will be addressed in Phases 1 and 2 through targeted code review and integration tests.

Conclusion

This blueprint provides a rigorous, implementation-ready plan to add a Dear ImGui overlay to Cataclysm-BN without disrupting the ASCII renderer, tiles pipeline, input contexts, or game state. The hybrid approach—ui_manager layering plus final-frame compositing—achieves deterministic draw order with minimal intrusion. Dual input handling balances overlay interactivity with gameplay continuity, and event bus integration ensures decoupling and testability. Isolated preferences and a disciplined memory model protect save integrity and reduce maintenance burden. Performance practices—batching, DPI scaling, vsync, minimize-pause—are informed by well-documented integration guidance and empirical evidence. The phased roadmap and risk register provide the governance needed to deliver a robust feature that enhances developer productivity and player experience while preserving BN’s architectural integrity.

## References

[^1]: Cataclysm: Bright Nights — GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN  
[^2]: Cataclysm-BN src/main.cpp (entry points, initialization, loop scaffolding). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/main.cpp  
[^3]: Cataclysm-BN src/game.cpp (game state, UI integration, main loop). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/game.cpp  
[^4]: Cataclysm-BN src/cursesport.cpp (pseudo-curses implementation and ASCII rendering). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/cursesport.cpp  
[^5]: Cataclysm-BN src/input.cpp (input manager, contexts, keybindings). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/input.cpp  
[^6]: Cataclysm-BN src/ui.cpp (uilist, UI manager, adaptor, windowing). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/ui.cpp  
[^9]: Cataclysm: Bright Nights — Official Documentation. https://docs.cataclysmbn.org  
[^24]: Forrest The Woods, “Proving Immediate Mode GUIs are Performant.” https://www.forrestthewoods.com/blog/proving-immediate-mode-guis-are-performant/  
[^26]: Martin Fieber, “GUI Development with C++, SDL2, and Dear ImGui.” https://martin-fieber.de/blog/gui-development-with-cpp-sdl2-and-dear-imgui/  
[^27]: Nicolas Guillemot, “Dear ImGui” (CppCon 2016). https://www.youtube.com/watch?v=LSRJ1jZq90k  
[^33]: Dear ImGui GitHub Repository. https://github.com/ocornut/imgui