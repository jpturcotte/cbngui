# Cataclysm-BN Menu System Design: Comprehensive Architecture, UI Mockups, and Implementation Blueprint

## Executive Summary and Objectives

This blueprint presents a modern, accessible, and robust menu system for Cataclysm: Bright Nights (BN) that integrates seamlessly with the game’s established architecture. The solution is deliberately non-invasive: overlays draw above the composed frame without modifying renderer internals, input contexts are localized and restored cleanly, UI lifecycle hooks manage redraw and resize, and preferences are persisted through JSON while preserving backward compatibility. The primary objectives are:

- Establish a modal, stack-based menu system that coexists with the main menu, gameplay, and pause states through deterministic transitions.
- Deliver a Save/Load interface with a visual file browser, thumbnails, rich metadata, search/filter, and atomic file operations.
- Provide a settings panel that unifies key bindings, display options, and GUI toggles, with validation and rollback.
- Define navigation rules that ensure consistent focus, cancel/escape semantics, and coherent event routing.
- Ensure backward compatibility by isolating GUI preferences and respecting versioning and migration mechanisms.

The design leverages BN’s pseudo-curses buffer model and SDL tiles compositor for rendering, input_manager and per-mode input_context for events, ui_manager for UI lifecycle, and savegame_json for persistence. It adheres to project documentation and source conventions, aligning with the repository’s established development practices.[^1][^4][^5][^6][^7][^9]

## Architectural Baseline

BN’s architecture separates rendering, input, UI, game state, and persistence into well-defined subsystems:

- Rendering: cursesport provides Unicode-aware text buffers and window refresh primitives; sdltiles composes the final frame using fonts and geometry.
- Input: input_manager orchestrates global input behavior; input_context instances register actions for screens and modes, translating raw events into actionable IDs.
- UI: uilist and string_input_popup are reused across screens; ui_manager and ui_adaptor coordinate redraw/resize; panel_manager organizes modular panels.
- Game state: the game class owns the main loop, world/map/overmap, weather, event bus, and autosave heuristics.
- Persistence: savegame_json manages JSON serialization, versioning, migration for legacy saves, and RLE for repetitive layers.

Table 1 enumerates the subsystem responsibilities and interfaces relevant to the menu system.

Table 1 — Core subsystems and responsibilities

| Subsystem | Files | Responsibility | Key interfaces |
|---|---|---|---|
| ASCII/curses rendering | cursesport.cpp | Unicode buffers; window refresh | WINDOW; wnoutrefresh; doupdate |
| SDL tiles rendering | sdltiles.cpp | Frame composition; fonts; geometry | Present-stage hooks |
| Input handling | input.cpp | Global input; contexts; bindings | input_manager; input_context |
| UI components | ui.cpp | uilist; string_input_popup; ui_manager | Modal lifecycle; redraw/resize |
| Game state | game.cpp | Main loop; world subsystems; autosave | game; uquit; safe_mode |
| Persistence | savegame.cpp | JSON I/O; versioning; migration; RLE | JsonOut/JsonIn; legacy loaders |

This baseline clarifies safe integration surfaces: overlays never mutate renderer internals, input interception is localized to active overlays, UI lifecycle hooks are reused for redraw/resize, and preferences are stored separately from world saves. The design respects these boundaries to prevent regressions and maintainability risks.[^4][^5][^6][^3][^7]

## Menu State Management

The menu system is implemented as a modal overlay stack managed by ui_manager. Overlays are pushed when opened and popped when closed; each overlay owns its input_context and rendering surface. Event interception is conditional—only active overlays consume events within hit regions, with pass-through configured for non-invasive background hints. The game class retains full authority over world state; overlays read for display and write settings via validated interfaces. Redraw and resize are coordinated via ui_adaptor, ensuring coherent layouts and avoiding stale redraws.[^6][^3][^5]

Table 2 outlines the principal UI state transitions.

Table 2 — UI state machine transitions

| Current UI | Event | Next UI | Ownership | Restore behavior |
|---|---|---|---|---|
| Gameplay | Open menu | Overlay (modal) | Overlay owned by ui_manager; no state mutation | Underlay paused; focus on overlay |
| Overlay | Confirm/Close | Prior UI | Overlay popped; state intact | Prior focus restored; redraw |
| Gameplay | Pause toggled | Pause overlay | Pause owned by game; overlay limited to pause UI | Unpause dismisses overlay |
| Main menu | Open options | Options overlay | Overlay writes to options/preferences | Return to main menu; applied or reverted |
| Options | Apply toggles | Options | Options owns commit; overlay validates | Settings persisted; redraw |

Focus management rules are explicit (Table 3).

Table 3 — Focus and lifecycle rules per screen

| Screen | Focus owner | Redraw hooks | Context switch | Pass-through policy |
|---|---|---|---|---|
| Gameplay | Game | ui_adaptor | Gameplay contexts | None |
| Pause overlay | Overlay | ui_adaptor | Overlay context | Optional hover hints |
| In-game menu | Overlay | ui_adaptor | Overlay context | None |
| Settings | Overlay | ui_adaptor | Options context | None |
| Save/Load | Overlay | ui_adaptor | List context | None |

This approach is consistent with BN’s existing modal flows, ensures deterministic behavior, and avoids side effects on gameplay state.[^6][^3][^5]

## Save/Load Interface

The Save/Load interface is a visual file browser that presents save entries, metadata, and optional preview images. It uses BN’s JSON metadata—version, world name, character, playtime, and timestamp—and degrades gracefully to textual previews in ASCII builds or when images are missing. File operations (load, save, delete, duplicate, rename) use confirm dialogs and atomic write semantics to prevent partial writes. Error handling covers corrupt JSON, version mismatches, and I/O failures, with clear user feedback and rollback.[^6][^7][^14]

Table 4 defines metadata fields used by the UI.

Table 4 — Save metadata fields and sources

| Field | Source | Type | Notes |
|---|---|---|---|
| savegame_version | savegame.cpp | int | Versioning; migration triggers |
| worldname | world data | string | Display; filtering |
| player name | player data | string | Display; sorting |
| playtime | game state | string or int | Display-friendly |
| save timestamp | JSON header | string | Localizable |
| stats snapshot | player data | compact struct | Preview only |
| screenshot path | user gfx | string | Optional preview image |

File operation semantics and error handling are summarized in Table 5.

Table 5 — File operations and error handling

| Operation | Semantics | Errors | Recovery |
|---|---|---|---|
| Load | Validate JSON; version check; migrate | Corrupt JSON; version mismatch | Error dialog; logs; revert |
| Save | Temp file; atomic replace | Disk full; permissions | Retry prompt; alternate path; keep session |
| Delete | Confirm prompt | Locked/in-use | Retry; notify |
| Duplicate | Copy; update metadata | Name conflict | Auto-rename; confirm |
| Rename | Confirm prompt | Invalid characters | Normalize; prompt |

Thumbnail rules ensure consistent behavior across builds (Table 6).

Table 6 — Thumbnail availability and fallback behavior

| Condition | Location | Fallback |
|---|---|---|
| Tiles build + image present | user gfx area | Display scaled image |
| ASCII/curses or missing image | none | Textual summary; placeholder |
| Corrupt/missing preview | none | Hide image; warning icon |

### Metadata and Preview Strategy

The UI reads JSON headers and world files to populate metadata and binds this data to list entries. Selecting an entry refreshes a details panel and optional preview. If thumbnails are unavailable, the interface presents a textual summary. This preserves consistency across backends without altering save schemas.[^7][^6]

## Settings Panel Design

Settings are organized into three groups: key bindings, display options (ASCII vs tiles), and GUI toggles. The key bindings editor leverages BN’s existing action mapping and conflict detection; display options are partitioned by backend (curses font/color attributes vs tiles font/scaling/resolution); GUI toggles persist overlay enablement, theme, position, and opacity via a preferences JSON with validation and rollback. Apply/revert semantics are explicit; invalid values revert with user messaging.[^5][^6][^4][^7]

Table 7 enumerates key settings and persistence.

Table 7 — Key settings and persistence

| Setting | Type | Default | Persistence | Validation |
|---|---|---|---|---|
| Action bindings | map<string, string> | defaults | keybindings JSON | Conflict detection |
| Curses font | string | system default | options JSON | Path valid |
| Curses color pairs | map<string, string> | defaults | options JSON | Valid indices |
| Tiles font | string | default | options JSON | Path valid |
| Tiles resolution | int | 1024x768 | options JSON | Supported modes; clamp |
| UI scaling | float | 1.0 | options JSON | Clamp [0.5, 2.0] |
| Overlay enabled | bool | false | preferences JSON | Boolean |
| Overlay theme | string | “default” | preferences JSON | Known IDs |
| Overlay opacity | float | 0.8 | preferences JSON | Clamp [0.0, 1.0] |
| Panel layout | string | “sidebar” | preferences JSON | Allowed values |

Backend-specific display options and effects are summarized in Table 8.

Table 8 — Display options by backend and UI effects

| Backend | Option | UI effect | Notes |
|---|---|---|---|
| Curses | Font; color pairs | Text/color rendering | Pseudo-curses semantics |
| Curses | Attribute styles | Emphasis in menus | Color + attributes |
| Tiles | Font; size | Clarity; scale | SDL2_ttf |
| Tiles | Resolution; scaling | Panel geometry; responsiveness | High DPI aware |
| Tiles | Pixel minimap; tile size | Visual density | HUD layout impacted |

### Key Bindings and Conflict Resolution

The editor supports action search, binding viewing, and conflict resolution. Updates persist via the options system; invalid bindings revert with clear messaging. Overlay bindings are distinct by default and configurable by the user.[^5]

### Display Options Partitioning

Curses options affect window buffer rendering and color handling; tiles options manage fonts, resolution, and scaling. UI elements register for redraw/resize via ui_adaptor, ensuring coherent layouts after changes.[^4][^6]

## Navigation Flow and Focus Management

Navigation follows deterministic rules: main menu branches to new/load; in-game menus open overlays; escape/cancel pops the stack. Focus is exclusive to the top overlay, and cancel/confirm actions are consistently mapped. The game loop retains authority over time advancement, autosave, and state mutations. These rules ensure predictable user experience and prevent context leakage.[^6][^3][^5]

Table 9 summarizes the navigation map.

Table 9 — Navigation map and hotkeys

| Screen | Entry | Exit | Hotkeys | Focus rules |
|---|---|---|---|---|
| Main menu | Launch sub-screens | Confirm/cancel/ESC | Confirm; Cancel; Arrows | Main menu owns focus |
| Gameplay | Enter world | Open menu/pause | Menu; Pause | Gameplay owns focus |
| In-game menu | Open from gameplay | Confirm/close/ESC | Confirm; Cancel; Filter | Overlay owns focus |
| Settings | Open from main/in-game | Apply/close/ESC | Confirm; Cancel; Tab | Overlay owns focus |
| Save/Load | Open from main/in-game | Confirm/close/ESC | Confirm; Cancel; Search | Overlay owns focus |

## Rendering and Input Integration

Overlay layering is achieved without invasive changes to the compositor. Two variants are supported:

- Variant A: final-frame compositing at a present-stage hook.
- Variant B: manager-driven layering via ui_manager.

Both preserve the ASCII and tiles paths; overlays draw above the composed frame and are opt-in. Input interception is conditional: active overlays consume events inside hit regions; non-overlay events pass through as configured. High DPI scaling is supported, and drawing pauses when the window is minimized to avoid wasted work.[^4][^6][^5][^2]

Table 10 provides the event flow matrix.

Table 10 — Event flow matrix

| Source | Dispatcher | Interception | Handler | Consumption rule |
|---|---|---|---|---|
| SDL events | input_manager | Pre-routing (overlay active) | Overlay input | Events inside bounds consumed |
| SDL events | input_manager | Normal routing (overlay inactive) | Existing handlers | No interception |
| Overlay widgets | ui_manager | Focus/visibility | Overlay UI | Filtered by focus/visibility |
| Global hotkeys | options/action | On-press (overlay active) | Overlay hotkeys | Consumed if active; else ignored |

Table 11 lists acceptance criteria.

Table 11 — Acceptance criteria matrix

| Integration point | Test | Expected outcome | Pass/Fail |
|---|---|---|---|
| Rendering layering | Visual + unit | Overlay above ASCII/tiles; no artifacts | No flicker; deterministic draw order |
| Input interception | Integration + unit | Events consumed when active; pass when inactive | No event loss; validated hotkeys |
| Menu/state | Integration | Lifecycle correct; state restored | No state leak; deterministic transitions |
| Save/load prefs | Unit + integration | Preferences saved/loaded; rollback on error | No corruption; invalid values fallback |
| Character/inventory | Unit + integration | Data bindings accurate; no write-through | No desync; accurate snapshots; perf maintained |

## Backward Compatibility and Preferences

GUI preferences are stored in a dedicated JSON file, separate from world/character saves. Versioning and migration reuse existing savegame logic; invalid configurations revert to defaults. Overlays are disabled by default; when inactive, behavior is identical to baseline. This ensures zero impact on existing workflows and safe adoption.[^7][^6]

Table 12 defines preferences keys, defaults, and fallbacks.

Table 12 — Preferences keys, defaults, and fallbacks

| Key | Type | Default | Validation | Fallback behavior |
|---|---|---|---|---|
| overlay.enabled | bool | false | Boolean | Revert to false |
| overlay.theme | string | “default” | Known theme IDs | Revert to “default” |
| overlay.position | string | “bottom-right” | Allowed positions | Revert to “bottom-right” |
| overlay.opacity | float | 0.8 | Clamp [0.0, 1.0] | Clamp and apply |
| overlay.hotkeys | array | [] | Action IDs or key tokens | Clear array |

Table 13 summarizes backward-compatibility test cases.

Table 13 — Backward compatibility test matrix

| Scenario | Expected behavior | Pass criteria |
|---|---|---|
| Load legacy save | Use migration; display metadata | Successful load; clear messaging |
| Preferences missing or corrupt | Revert to defaults; write clean file | Run without crash; user notice |
| Overlay disabled | No overlay involvement | Identical behavior to baseline |
| ASCII vs tiles builds | Graceful preview degradation | Consistent navigation; textual summary |
| Options conflicts | Detect and prevent | User warned; binding unchanged |

## UI Mockups and Widget Strategy

Menus reuse established widgets (uilist, string_input_popup, modal dialogs) and follow a consistent theme. High DPI scaling is supported, and minimize-pause prevents wasted rendering. For developer tools and debug overlays, Dear ImGui provides an efficient immediate-mode solution; for core menus, an SDL2-native retained-mode library (libsdl2gui or SDL_gui) offers predictable, componentized UIs. This hybrid approach balances rapid iteration with stability for gameplay interfaces.[^6][^2][^8][^12][^13]

Table 14 maps screens to widgets and integration steps.

Table 14 — Screen-to-widget mapping and integration

| Screen | Widgets | Data source | Event bindings | Integration steps |
|---|---|---|---|---|
| Save/Load | uilist grid; details; preview | JSON metadata | Load/delete/rename; filter/search | Directory scan; bind actions; handle file ops |
| Settings | Tabs; editors; toggles | Options + preferences | Apply/revert; conflict detection; redraw | Register contexts; wire options; ui_adaptor hooks |
| Main menu | Button grid; world list | World factory | New/load/mods/options | Register actions; push overlays; redraw |
| Pause | Modal actions | Game state | Resume; save/load; settings | Pause underlay; focus overlay; handle escape |
| Dialogs | Confirm/alert popups | UI state | Confirm/cancel | Modal behavior; redraw; pass-through rules |

Mockup descriptions:

- Main menu: a button grid with clear focus and a world list for selection; consistent navigation and theming.
- Save/Load: a list/grid of saves with metadata and optional preview; robust file operations and error handling.
- Settings: tabs for key bindings, display options, and GUI toggles; explicit apply/revert; validation and rollback.
- Pause: minimal modal overlay for Resume, Save/Load, and Settings; escape returns to gameplay.

## Implementation Plan: Phased Delivery

Delivery proceeds in six phases to minimize risk and ensure incremental validation:

- Phase 1: Foundations—overlay module skeleton, PIMPL, hooks, and ui_manager integration (inactive by default).
- Phase 2: Rendering and input—overlay compositing or manager registration, input adapters, z-order handling across ASCII/tiles; High DPI and minimize-pause.
- Phase 3: Menu/state—modal push/pop, focus restoration, cancel/confirm semantics; no side effects on gameplay.
- Phase 4: Save/Load UI—visual file browser, metadata, previews, search/filter; robust file operations.
- Phase 5: Settings—key bindings editor, display options (backend-partitioned), GUI toggles; preferences persistence with validation and rollback.
- Phase 6: Testing—unit, integration, performance; CI coverage; finalize acceptance criteria.

Table 15 consolidates the phased plan.

Table 15 — Phases, deliverables, and criteria

| Phase | Tasks | Outputs | Entry criteria | Exit criteria |
|---|---|---|---|---|
| 1 | Skeleton; PIMPL; hooks | Overlay module; ui_manager integration | Baseline build | Hooks present; overlays inactive |
| 2 | Rendering; input; z-order | Overlay draws; interception | Phase 1 | Visual/input tests pass |
| 3 | Modal lifecycle | Push/pop; focus restore | Phase 2 | Deterministic transitions; no side effects |
| 4 | Save/Load UI | Browser; metadata; previews | Phase 3 | Robust file ops; error handling |
| 5 | Settings | Bindings; display; GUI toggles | Phase 4 | Preferences persisted; validation |
| 6 | Testing | Unit; integration; CI | Phase 5 | All acceptance criteria met |

## Risks, Constraints, and Open Questions

Key risks include event contention, z-order conflicts, JSON schema risks, and performance regressions. Constraints include keeping overlays opt-in and preserving core save schemas. Information gaps that require confirmation during implementation include the exact draw order and frame lifecycle for compositing hooks, canonical input interception and focus rules, modal stacking details in ui_manager, preferences naming conventions, platform-specific input quirks, performance telemetry and budgets, and licensing constraints for third-party GUI libraries.

Table 16 captures the risk register.

Table 16 — Risk register

| Risk | Likelihood | Impact | Mitigation | Owner | Status |
|---|---|---|---|---|---|
| Event contention | Medium | Medium | Consumption rules; pass-through; configurable hotkeys | UI lead | Open |
| Z-order misdraw | Low–Medium | High | ui_manager layering; deterministic compositing | Rendering lead | Open |
| JSON schema risk | Low | High | Separate preferences; validation; rollback | Persistence lead | Open |
| Perf regression | Medium | Medium–High | Measure; pause on minimize; optimize draw | Engineering lead | Open |
| Input conflicts | Medium | Medium | Options validation; context-local bindings | Input lead | Open |
| DPI scaling | Medium | Medium | Scale handling; test matrices | QA | Open |
| Licensing | Unknown | Medium | Legal review of third-party libraries | Maintainers | Open |

## Appendices: Data Models, JSON Preferences, and Action Maps

Menu state stack schema (Table 17) defines entries and lifecycle hooks.

Table 17 — Menu state stack schema

| Field | Type | Default | Lifecycle hooks |
|---|---|---|---|
| id | string | “” | on_init; on_open; on_close |
| type | enum | “overlay” | on_enter; on_exit |
| z_order | int | 100 | on_draw ordering |
| focus_policy | enum | “exclusive” | on_input routing |
| input_context | string | “” | register_action set |
| visibility | bool | true | on_show; on_hide |
| parent | string or null | null | stack push/pop |

GUI preferences JSON proposal:

```
{
  "overlay": {
    "enabled": false,
    "theme": "default",
    "position": "bottom-right",
    "opacity": 0.8,
    "hotkeys": []
  },
  "panel": {
    "layout": "sidebar"
  }
}
```

UI action map skeleton:

- Main menu: CONFIRM, CANCEL, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT.
- Gameplay: MOVE_North, MOVE_South, MOVE_East, MOVE_West, LOOK, PAUSE, OPEN_MENU.
- Overlay: CONFIRM, CANCEL, FILTER, APPLY, REVERT, FOCUS_NEXT, FOCUS_PREV.

Bindings are loaded and validated via the options system; conflicts are resolved in the editor with fallbacks to prior bindings on error.[^5][^6][^7]

## Integration of UI Mockups (Embedded from Design Assets)

To connect specification to concrete visuals, the following design assets are embedded and referenced throughout the document. They should be used by developers and designers to align implementation and validation.

![Menu system architecture: layering, state ownership, integration points.](\/workspace\/docs\/design\/menu_system_architecture.png)

This architecture diagram shows overlays above the composed frame, input interception conditioned on overlay activity, and lifecycle hooks via ui_manager. It reinforces the opt-in, non-invasive approach and clarifies ownership boundaries.

![UI state flow: transitions among main menu, gameplay, and overlays.](\/workspace\/docs\/design\/menu_state_flow.png)

The state flow diagram illustrates push/pop semantics, deterministic transitions, and consistent cancel/confirm mappings across screens.

![Main menu wireframe: layout, world list, action panel.](\/workspace\/docs\/design\/main_menu_wireframe.png)

The main menu wireframe demonstrates a clean layout with a world list, action buttons, and a game information panel. It sets visual and interaction standards for the menu system.

![Save/Load interface wireframe: metadata, preview, file operations.](\/workspace\/docs\/design\/save_load_interface.png)

The Save/Load wireframe shows a file grid, details panel, and optional preview, highlighting atomic file operations and error handling.

![Settings panel wireframe: key bindings, display options, GUI toggles.](\/workspace\/docs\/design\/settings_panel_wireframe.png)

The settings wireframe organizes key bindings, display options, and GUI toggles into a coherent interface, with explicit apply/revert semantics and validation.

![Navigation flow diagram: screen-to-screen transitions and hotkeys.](\/workspace\/docs\/design\/navigation_flow_diagram.png)

The navigation flow diagram consolidates entry/exit actions and focus rules, serving as a reference for implementing input contexts and modal transitions.

![Technical architecture: module boundaries and integration interfaces.](\/workspace\/docs\/design\/technical_architecture.png)

The technical architecture diagram details module boundaries—rendering overlays above the frame, input interception when active, UI lifecycle coordination, and JSON-safe preferences—providing a system-level blueprint for maintainers.

## Known Information Gaps

Implementation should confirm:

- Exact draw order and frame lifecycle insertion points for final-frame compositing in sdltiles.
- Canonical input interception and focus rules in existing overlays.
- UI modal stacking and focus management details in ui_manager for complex overlays.
- BN-specific JSON preferences files and naming conventions.
- Platform-specific input quirks across SDL2 targets.
- Performance telemetry and budgets for overlay per-frame costs.
- Licensing constraints for third-party GUI libraries beyond SDL2-native code.

These gaps are non-blocking and will be resolved during Phases 1–2.

## References

[^1]: Cataclysm: Bright Nights — GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN  
[^2]: Martin Fieber, “GUI Development with C++, SDL2, and Dear ImGui.” https://martin-fieber.de/blog/gui-development-with-cpp-sdl2-and-dear-imgui/  
[^3]: Cataclysm-BN src/game.cpp (game state, UI integration, main loop). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/game.cpp  
[^4]: Cataclysm-BN src/cursesport.cpp (pseudo-curses implementation and ASCII rendering). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/cursesport.cpp  
[^5]: Cataclysm-BN src/input.cpp (input manager, contexts, keybindings). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/input.cpp  
[^6]: Cataclysm-BN src/ui.cpp (uilist, UI manager, adaptor, windowing). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/ui.cpp  
[^7]: Cataclysm-BN src/savegame.cpp (JSON serialization, versioning, RLE, legacy support). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/savegame.cpp  
[^8]: libsdl2gui GitHub Repository. https://github.com/adamajammary/libsdl2gui  
[^9]: Cataclysm: Bright Nights — Official Documentation. https://docs.cataclysmbn.org  
[^12]: Dear ImGui GitHub Repository. https://github.com/ocornut/imgui  
[^13]: Nicolas Guillemot, “Dear ImGui” (CppCon 2016). https://www.youtube.com/watch?v=LSRJ1jZq90k  
[^14]: Debian: cataclysm-dda-sdl package details (graphical SDL2-based interface). https://packages.debian.org/sid/games/cataclysm-dda-sdl