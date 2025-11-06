# Cataclysm-BN Menu System: Architecture, UI Mockups, and Implementation Blueprint

## Executive Summary and Objectives

This blueprint delivers a modern, accessible, and maintainable menu system for Cataclysm: Bright Nights (BN) that integrates seamlessly with the game’s existing architecture. The design addresses five core requirements: menu state management, a visual Save/Load interface, a comprehensive settings panel, deterministic navigation flow, and full backward compatibility. It is intentionally non-invasive, respects both ASCII/curses and SDL tiles backends, and preserves BN’s JSON-based save system. The outcome is a menu experience that feels native, performs reliably, and can be extended without destabilizing core systems.

To anchor the design in the project’s established patterns, the architecture leverages the pseudo-curses buffer model for ASCII rendering, SDL tiles for graphical compositing, mode-specific input contexts for event routing, and ui_manager-driven lifecycles for redraw and resize. The Save/Load interface is built on the JSON serialization framework, reusing existing metadata and migration paths. The settings panel adheres to BN’s options and keybinding conventions. The result is a cohesive, standards-aligned blueprint consistent with the project’s documentation and codebase.[^1][^4][^6][^9]

## Architectural Baseline: Rendering, Input, UI, and Persistence

BN’s rendering splits into a curses-like buffer and a display compositor. The cursesport layer maintains Unicode-aware text cells and window buffers, with double-buffered refresh via wnoutrefresh and doupdate. The sdltiles compositor turns these buffers into the visible frame, handling fonts and geometry. Input is mediated by input_manager and per-mode input_context instances, which translate raw events into action IDs. The UI system provides reusable components such as uilist and string_input_popup, and an adaptor/manager layer to coordinate redraws and resizes. The game class orchestrates the main loop, world subsystems, autosave heuristics, and UI state anchoring. Persistence uses JSON with versioning, migration for legacy saves, and RLE for repetitive layers.[^4][^5][^6][^3][^7]

To orient integration around existing boundaries, Table 1 summarizes the subsystems, responsibilities, and touchpoints.

Table 1 — Core subsystems and responsibilities

| Subsystem | Files | Responsibility | Key interfaces |
|---|---|---|---|
| ASCII/curses rendering | cursesport.cpp | Unicode-aware buffers; window refresh | WINDOW; wnoutrefresh; doupdate |
| SDL tiles rendering | sdltiles.cpp | Compositor; fonts; geometry | Present-stage hooks; font utilities |
| Input handling | input.cpp | Global manager; contexts; bindings | input_manager; input_context; actions |
| UI components | ui.cpp | uilist; string_input_popup; ui_manager | Modal lifecycle; redraw/resize hooks |
| Game state | game.cpp | Main loop; world/map/overmap; events | game; uquit; safe_mode; autosave |
| Persistence | savegame.cpp | JSON I/O; versioning; RLE; migration | JsonOut/JsonIn; legacy loaders |

These boundaries define safe insertion points: overlay rendering occurs above the composed frame; input interception is localized to active overlays; UI lifecycle follows ui_manager; preferences are stored separately from world saves. This discipline prevents regressions and simplifies maintenance.[^4][^6][^7][^3][^5]

## Menu State Management: Modal UI Stack with Gameplay and Pause

The menu system is implemented as a modal overlay stack under ui_manager. Each overlay owns its input_context and drawing surface, pushes onto the stack when opened, and pops when closed, restoring the prior screen’s focus. Input is intercepted only when overlays are active, with event consumption limited to hit regions and pass-through configured for non-invasive background hints. The game class retains full authority over world state; overlays read for display and write settings through validated interfaces. Lifecycle semantics ensure redraw coherence and deterministic transitions.[^6][^3][^5]

Table 2 captures the principal state transitions among gameplay, pause, and overlays.

Table 2 — UI state machine transitions

| Current UI | Event | Next UI | Ownership | Restore behavior |
|---|---|---|---|---|
| Gameplay | Open menu | Overlay (modal) | Overlay owned by ui_manager; no state mutation | Underlay paused; focus on overlay |
| Overlay | Confirm/Close | Prior UI | Overlay popped; state intact | Prior focus restored; redraw |
| Gameplay | Pause toggled | Pause overlay | Pause owned by game; overlay limited to pause UI | Unpause dismisses overlay |
| Main menu | Open options | Options overlay | Overlay writes to options/preferences | Return to main menu; applied or reverted |
| Options | Apply toggles | Options | Options owns commit; overlay validates | Settings persisted; redraw and restore |

Focus management rules are explicit and tested (Table 3).

Table 3 — Focus and lifecycle rules

| Screen | Focus owner | Redraw hooks | Context switch | Pass-through policy |
|---|---|---|---|---|
| Gameplay | Game | ui_adaptor | Gameplay contexts | None |
| Pause overlay | Overlay | ui_adaptor | Overlay context | Optional hover hints |
| In-game menu | Overlay | ui_adaptor | Overlay context | None |
| Settings | Overlay | ui_adaptor | Options context | None |
| Save/Load | Overlay | ui_adaptor | List context | None |

This design harmonizes with BN’s existing modal flows and ensures overlays never destabilize gameplay state.[^6][^3][^5]

## Save/Load Interface: Visual File Browser with Thumbnails and Metadata

The Save/Load interface presents a file browser built on uilist, with metadata (version, world, character, playtime, timestamp), optional tiles thumbnails, preview panels, search/filter, and robust file operations. Errors such as corrupt JSON or version mismatches are handled with clear messaging and fallback. Saves use atomic write semantics via temp files, preventing partial writes. ASCII/curses builds degrade gracefully to textual previews. The UI reads metadata from JSON headers and world files, ensuring consistency with BN’s persistence model.[^6][^7][^14]

Table 4 defines the metadata fields presented.

Table 4 — Save metadata fields and sources

| Field | Source | Type | Notes |
|---|---|---|---|
| savegame_version | savegame.cpp | int | Versioning and migration |
| worldname | world data | string | Display; filtering |
| player name | player data | string | Display; sorting |
| playtime | game state | string or int | Display-friendly |
| save timestamp | JSON header | string | Localizable |
| stats snapshot | player data | compact struct | Preview only |
| screenshot path | user gfx | string | Optional preview image |

File operation semantics are summarized in Table 5.

Table 5 — File operations and error handling

| Operation | Semantics | Errors | Recovery |
|---|---|---|---|
| Load | Validate; version check; migrate | Corrupt JSON; mismatch | Error dialog; logs; revert |
| Save | Temp file; atomic replace | Disk full; permissions | Retry; alternate path; keep session |
| Delete | Confirm prompt | Locked/in-use | Retry; notify |
| Duplicate | Copy; update metadata | Name conflict | Auto-rename; confirm |
| Rename | Confirm prompt | Invalid chars | Normalize; prompt |

Thumbnail rules are straightforward (Table 6).

Table 6 — Thumbnail availability and fallback

| Condition | Location | Fallback |
|---|---|---|
| Tiles + image present | user gfx | Scaled image |
| ASCII or missing | none | Textual summary; placeholder |
| Corrupt/missing preview | none | Hide image; warning icon |

### Metadata Acquisition and Preview

The UI reads JSON metadata during directory scans and binds it to list entries. Selecting an entry updates a details panel; previews are shown if available. This approach keeps the interface consistent across backends and avoids changes to save schemas.[^7][^6]

## Settings Panel Design: Key Bindings, Display Options, and GUI Toggles

Settings are organized into three groups: key bindings, display options (ASCII vs tiles), and GUI toggles. Key bindings use BN’s binding editor, with conflict detection and resolution via options. Display options are backend-specific: curses font/color attributes and tiles font/scaling/resolution. GUI toggles control overlay enablement, theme, position, and opacity, persisted in a preferences JSON with validation and rollback. Apply/revert semantics are explicit, and invalid values fallback with messaging.[^5][^6][^4][^7]

Table 7 lists key settings and persistence.

Table 7 — Key settings and persistence

| Setting | Type | Default | Persistence | Validation |
|---|---|---|---|---|
| Action bindings | map<string, string> | defaults | keybindings JSON | Conflict detection |
| Curses font | string | system default | options JSON | Path valid |
| Curses colors | map<string, string> | defaults | options JSON | Valid indices |
| Tiles font | string | default | options JSON | Path valid |
| Tiles resolution | int | 1024x768 | options JSON | Supported modes |
| UI scaling | float | 1.0 | options JSON | Clamp [0.5, 2.0] |
| Overlay enabled | bool | false | preferences JSON | Boolean |
| Overlay theme | string | “default” | preferences JSON | Known IDs |
| Overlay opacity | float | 0.8 | preferences JSON | Clamp [0.0, 1.0] |
| Panel layout | string | “sidebar” | preferences JSON | Allowed values |

Backend-specific effects are summarized in Table 8.

Table 8 — Display options by backend and UI effects

| Backend | Option | Effect | Notes |
|---|---|---|---|
| Curses | Font; color pairs | Text/color rendering | Pseudo-curses semantics |
| Curses | Attribute styles | Emphasis in menus | Color + attributes |
| Tiles | Font; size | Clarity; scale | SDL2_ttf |
| Tiles | Resolution; scaling | Panel geometry | High DPI aware |
| Tiles | Minimap; tile size | Visual density | HUD layout impacts |

### Key Bindings Editor

Users search actions, view current bindings, and resolve conflicts. Updates persist via options; invalid bindings revert with messaging. Overlay hotkeys remain distinct unless remapped by the user.[^5]

### Display Options Partitioning

Curses options adjust window buffer rendering; tiles options manage fonts, resolution, and scaling. UI elements register for redraw/resize via ui_adaptor to ensure coherent layouts after changes.[^4][^6]

## Navigation Flow: Deterministic Transitions and Focus Management

Navigation adheres to deterministic rules: main menu branches to new/load; in-game menus open overlays; escape/cancel pops the stack. Focus is exclusive to the top overlay, and cancel/confirm actions are consistently mapped. The model ensures that the game loop’s authority over time, autosave, and state mutations is never compromised.[^6][^3][^5]

Table 9 summarizes the navigation map.

Table 9 — Navigation map and hotkeys

| Screen | Entry | Exit | Hotkeys | Focus rules |
|---|---|---|---|---|
| Main menu | Launch sub-screens | Confirm/cancel/ESC | Confirm; Cancel; Arrows | Main menu owns focus |
| Gameplay | Enter world | Open menu/pause | Menu; Pause | Gameplay owns focus |
| In-game menu | Open from gameplay | Confirm/close/ESC | Confirm; Cancel; Filter | Overlay owns focus |
| Settings | Open from main/in-game | Apply/close/ESC | Confirm; Cancel; Tab | Overlay owns focus |
| Save/Load | Open from main/in-game | Confirm/close/ESC | Confirm; Cancel; Search | Overlay owns focus |

## Rendering and Input Integration: Overlay Layering and Event Interception

Overlays are layered above the composed frame without altering sdltiles or cursesport internals. Two integration variants are supported: final-frame compositing at a present-stage hook (Variant A) and manager-driven layering via ui_manager (Variant B). Input events are intercepted only when overlays are active; consumption is limited to hit regions, with pass-through configured where appropriate. High DPI scaling is respected, and drawing pauses when minimized to avoid wasted work.[^4][^6][^5][^2]

Table 10 describes the event flow matrix.

Table 10 — Event flow matrix

| Source | Dispatcher | Interception | Handler | Consumption rule |
|---|---|---|---|---|
| SDL events | input_manager | Pre-routing (overlay active) | Overlay input | Events inside bounds consumed |
| SDL events | input_manager | Normal routing (overlay inactive) | Existing handlers | No interception |
| Overlay widgets | ui_manager | Focus/visibility | Overlay UI | Filtered by focus/visibility |
| Global hotkeys | options/action | On-press (overlay active) | Overlay hotkeys | Consumed if active; else ignored |

Table 11 provides acceptance criteria.

Table 11 — Acceptance criteria matrix

| Integration point | Test | Expected outcome | Pass/Fail |
|---|---|---|---|
| Rendering layering | Visual + unit | Overlay above ASCII/tiles; no artifacts | No flicker; stable draw order |
| Input interception | Integration + unit | Events consumed when active; pass when inactive | No loss; validated hotkeys |
| Menu/state | Integration | Lifecycle correct; state restored | No leaks; deterministic |
| Save/load prefs | Unit + integration | Persisted; rollback on error | No corruption; fallback |
| Data bindings | Unit + integration | Accurate; no write-through | No desync; perf maintained |

## Backward Compatibility: Preferences Isolation and Versioning

GUI preferences are stored in a dedicated JSON file, separate from world/character saves. Versioning and migration reuse existing savegame logic. Invalid preferences revert to defaults without breaking the game. Overlays are disabled by default; when inactive, behavior is identical to baseline.[^7][^6]

Table 12 lists preferences keys, defaults, and fallbacks.

Table 12 — Preferences keys, defaults, and fallbacks

| Key | Type | Default | Validation | Fallback |
|---|---|---|---|---|
| overlay.enabled | bool | false | Boolean | false |
| overlay.theme | string | “default” | Known IDs | “default” |
| overlay.position | string | “bottom-right” | Allowed positions | “bottom-right” |
| overlay.opacity | float | 0.8 | Clamp [0.0, 1.0] | Clamp |
| overlay.hotkeys | array | [] | Action IDs/keys | Clear |

Table 13 summarizes backward-compatibility test cases.

Table 13 — Backward compatibility test matrix

| Scenario | Expected | Pass criteria |
|---|---|---|
| Load legacy save | Migration used | Successful load; messaging |
| Prefs missing/corrupt | Revert; write clean | No crash; notice |
| Overlay disabled | No effect | Baseline behavior |
| ASCII vs tiles | Graceful preview | Consistent nav; text summary |
| Options conflicts | Prevented | Warn user; no change |

## UI Mockups and Widget Strategy

The menu system reuses established widgets (uilist, string_input_popup, modal dialogs) and applies a consistent theme. High DPI scaling is supported; minimize-pause prevents wasted rendering. For tools and debug overlays, Dear ImGui offers efficient immediate-mode panels; for core menus, a retained-mode SDL2-native library (libsdl2gui or SDL_gui) provides stable, componentized UIs. This hybrid approach balances rapid iteration for developer tools with predictable long-lived screens for players.[^6][^2][^8][^12][^13]

Table 14 maps screens to widgets and integration steps.

Table 14 — Screen-to-widget mapping

| Screen | Widgets | Data source | Event bindings | Integration steps |
|---|---|---|---|---|
| Save/Load | uilist; details; preview | JSON metadata | Load/delete/rename; filter | Directory scan; bind actions |
| Settings | Tabs; editors; toggles | Options + prefs | Apply/revert; redraw | Register contexts; wire options |
| Main menu | Button grid; list | World factory | New/load/mods/options | Register actions; push overlays |
| Pause | Modal actions | Game state | Resume; save/load; settings | Pause underlay; focus overlay |
| Dialogs | Confirm/alert | UI state | Confirm/cancel | Modal behavior; redraw |

Mockup descriptions:

- Main menu: button grid with New, Load, Mods, Options; world list; consistent navigation and focus.
- Save/Load: list/grid of saves; details panel; optional thumbnail; search/filter; confirm dialogs for file operations.
- Settings: tabs for key bindings, display, GUI toggles; explicit apply/revert; invalid values fallback.
- Pause: minimal modal with Resume, Save/Load, Settings; escape returns to gameplay.

## Implementation Plan: Phased Delivery and Milestones

Delivery proceeds in six phases to mitigate risk and ensure incremental validation:

- Phase 1: Foundations—overlay module skeleton, PIMPL, and ui_manager integration (disabled by default).
- Phase 2: Rendering and input—overlay compositing or manager registration, input adapters, and z-order handling across ASCII/tiles; High DPI and minimize-pause supported.
- Phase 3: Menu/state—modal push/pop, focus restoration, and consistent cancel/confirm semantics without side effects.
- Phase 4: Save/Load UI—visual file browser with metadata, previews, search/filter, and robust file operations.
- Phase 5: Settings—key bindings editor, display options (backend-partitioned), GUI toggles; preferences persisted with validation and rollback.
- Phase 6: Testing—unit, integration, performance, and CI coverage; finalization of acceptance criteria.

Table 15 consolidates phases, deliverables, and exit criteria.

Table 15 — Phases and deliverables

| Phase | Tasks | Outputs | Entry criteria | Exit criteria |
|---|---|---|---|---|
| 1 | Skeleton; PIMPL; hooks | Overlay module; ui_manager integration | Baseline build | Hooks present; overlays inactive |
| 2 | Rendering; input; z-order | Overlay draws; interception | Phase 1 | Visual/input tests pass |
| 3 | Modal lifecycle | Push/pop; focus restore | Phase 2 | Deterministic transitions |
| 4 | Save/Load UI | Browser; metadata; previews | Phase 3 | Robust file ops; error handling |
| 5 | Settings | Bindings; display; GUI toggles | Phase 4 | Preferences persisted; validation |
| 6 | Testing | Unit; integration; CI | Phase 5 | All criteria met |

## Risks, Constraints, and Open Questions

Key risks include event contention, z-order conflicts, JSON schema risks, and performance regressions. Constraints include the requirement to keep overlays opt-in and to avoid changes to core save schemas. Information gaps that require confirmation during implementation include the exact draw order for compositing hooks, canonical input interception and focus rules, modal stacking details, preferences naming conventions, platform-specific input quirks, performance telemetry, and licensing constraints for third-party GUI libraries.

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

Bindings are loaded and validated via the options system; conflicts are resolved in the editor with fallbacks to prior bindings when needed.[^5][^6][^7]

## Integration of UI Mockups (Embedded from Design Assets)

To ground the specification in concrete interaction patterns, the following design assets illustrate the proposed menu architecture, state flow, and visual mockups. These diagrams are embedded for direct reference and should be used by developers and designers to align implementation and validation.

![Proposed menu system architecture: layering, state, and integration points.](\/workspace\/docs\/design\/menu_system_architecture.png)

The architecture diagram shows overlays layered above the composed frame, input interception only when active, and UI lifecycle mediated by ui_manager. It underscores that overlays are opt-in, have their own input contexts, and never own gameplay state. This aligns with BN’s separation of concerns and provides a clear mental model for integration.

![UI state flow: transitions among main menu, gameplay, overlays.](\/workspace\/docs\/design\/menu_state_flow.png)

The state flow diagram visualizes push/pop semantics and deterministic transitions among main menu, gameplay, pause overlays, and settings. It emphasizes that cancel/confirm actions are consistent and that focus is restored to the prior screen on close, ensuring predictable behavior and reducing user confusion.

![Main menu wireframe: layout, world list, action panel.](\/workspace\/docs\/design\/main_menu_wireframe.png)

The main menu wireframe demonstrates a clean layout with a world list, action buttons, and a game information panel. Navigation is consistent, focus is clear, and theming is applied uniformly. It sets expectations for spacing, typography, and interaction patterns that carry across the other screens.

![Save/Load interface wireframe: metadata, preview, file operations.](\/workspace\/docs\/design\/save_load_interface.png)

The Save/Load wireframe shows a file grid, details panel, optional thumbnail preview, and file operations guarded by confirm dialogs. It clarifies the user journey and underscores the need for atomic writes, version checks, and graceful error handling—core to BN’s persistence model.

![Settings panel wireframe: key bindings, display options, GUI toggles.](\/workspace\/docs\/design\/settings_panel_wireframe.png)

The settings wireframe organizes key bindings, display options (backend-partitioned), and GUI toggles into a coherent, tabbed interface. Apply/revert semantics and validation are highlighted, along with redraw hooks for resize and DPI changes.

![Navigation flow diagram: screen-to-screen transitions and hotkeys.](\/workspace\/docs\/design\/navigation_flow_diagram.png)

The navigation flow diagram consolidates entry/exit actions, hotkeys, and focus rules across screens, serving as a reference for implementation of input contexts and the modal stack.

![Technical architecture: module boundaries and interfaces.](\/workspace\/docs\/design\/technical_architecture.png)

The technical architecture diagram details module boundaries and interfaces—rendering overlays above composed frames, input interception conditioned on overlay activity, UI lifecycle coordination via ui_manager, and JSON-safe preferences. It provides a system-level view for maintainers and reviewers.

## Known Information Gaps

A small set of specifics must be confirmed during implementation:

- Precise draw order and frame lifecycle for final-frame compositing hooks in sdltiles and the main loop.
- Canonical input interception and focus rules beyond event polling in existing overlays.
- UI modal stacking and focus management in ui_manager for complex overlays.
- Existing JSON preferences files and naming conventions for BN-specific user data.
- Platform-specific input quirks across SDL2 targets relevant to overlays.
- Performance telemetry and budgets to assess per-frame overlay cost.
- Licensing constraints for any third-party GUI libraries beyond SDL2-native code.

These gaps are non-blocking but should be resolved during Phases 1–2 to ensure precise, non-breaking integration.

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