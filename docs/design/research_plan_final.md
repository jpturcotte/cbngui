# Cataclysm-BN Menu System Architecture Blueprint: States, Save/Load UI, Settings, Navigation, and Backward Compatibility

## Executive Summary and Objectives

This blueprint specifies a comprehensive, non-breaking menu system for Cataclysm: Bright Nights (BN) that modernizes the user interface while preserving the integrity of the ASCII display, input routing, core rendering pipeline, and JSON-based persistence. It is designed to integrate seamlessly with both the cursesport and SDL tiles backends, respect established input contexts, and work within the existing UI manager lifecycle. The system prioritizes:

- Menu state management that coexists with the main menu, gameplay, and pause flows using deterministic, modal overlays and stack-based transitions.
- A Save/Load interface that presents a visual file browser with thumbnails and rich metadata, with robust error handling, atomic writes, and previews.
- A settings panel for key bindings, display options (ASCII vs tiles), and GUI toggles, all validated and persisted via options and JSON preferences.
- A clear navigation model that enforces focus ownership, cancel/escape semantics, and consistent event routing.
- Backward compatibility that isolates GUI preferences from world saves and uses the versioning and migration mechanisms already present in savegame_json.

The approach leverages the project’s own documentation, source structure, and established UI practices, embedding overlays as opt-in layers above the composed frame. It is deliberately minimal in code changes, keeps additional dependencies modest, and adheres to the project’s encapsulation and PIMPL (pointer-to-implementation) conventions. Success is defined as correct layering and z-order with no visual artifacts, deterministic state transitions, and no regressions in frame time or save integrity.[^1][^9]

## Architectural Baseline: Rendering, Input, UI, and Persistence Boundaries

BN’s architecture cleanly separates responsibilities across five core areas: rendering (cursesport and sdltiles), input (input_manager and input_context), UI (uilist, ui_manager, ui_adaptor, panel_manager), game state (game class orchestrating world, map, overmap, weather, and event systems), and persistence (JSON serialization with versioning and RLE). This separation defines safe integration surfaces for the menu system, allowing overlays to draw above the frame without modifying the renderer, route input through contexts without changing the global dispatcher, and persist preferences through JSON while avoiding world save schema changes.[^3][^4][^5][^6][^7]

To make these boundaries concrete, Table 1 enumerates the core subsystems, their responsibilities, and the primary interfaces the menu system will use or extend.

Table 1 — Core subsystems and responsibilities

| Subsystem | Files (representative) | Responsibility | Key interfaces |
|---|---|---|---|
| Rendering (ASCII/curses) | cursesport.cpp | Curses-like buffers; Unicode-aware text; window refresh | WINDOW, wnoutrefresh, doupdate |
| Rendering (SDL tiles) | sdltiles.cpp | Compositor for final frame; fonts; geometry | Present-stage hooks; font utilities |
| Input | input.cpp | Global input manager; per-mode contexts; bindings | input_manager, input_context, action IDs |
| UI components | ui.cpp | uilist, string_input_popup; ui_manager; ui_adaptor | Modal lifecycle; redraw/resize hooks |
| Game state | game.cpp | Central loop; world subsystems; autosave | game instance; uquit; safe_mode |
| Persistence | savegame.cpp | JSON I/O; versioning; RLE; legacy loaders | JsonOut/JsonIn; migration paths |

This baseline informs layering and event routing. Overlays are drawn after the frame is composed, never mutating sdltiles or cursesport internals. Input contexts are local and restored cleanly on close. UI lifecycle hooks ensure redraws and resizes are coherent. JSON preferences remain isolated from world saves and validated before application. The result is a disciplined integration that respects ownership boundaries and avoids regressions.[^4][^5][^6][^7][^3]

## Menu State Management: Modal UI Stack Integrated with Game States

The menu system is modeled as a modal overlay stack managed by ui_manager. Each overlay registers a dedicated input_context for its actions. When an overlay is active, events inside overlay hit regions are consumed; others pass through to the underlying screen, if and only if the overlay explicitly allows pass-through for non-invasive background interactions. The game class retains full ownership of world state; overlays only read for display and write settings through validated interfaces. The lifecycle follows push/pop semantics, with deterministic restoration of focus and UI state on close.[^6][^3][^5]

Table 2 consolidates the principal transitions among gameplay, pause, and overlays.

Table 2 — UI state machine transitions

| Current UI | Event | Next UI | Ownership rules | Restore behavior |
|---|---|---|---|---|
| Gameplay | Open menu/overlay | Overlay (modal) | Overlay owned by ui_manager; no state mutation | Underlay paused; focus on overlay; redraw scheduled |
| Overlay (modal) | Confirm/Apply/Close | Prior UI | Overlay popped; game state intact | Prior focus restored; coherent redraw |
| Gameplay | Pause toggled | Pause overlay | Pause owned by game; overlay limited to pause UI | Unpause dismisses overlay; gameplay resumes |
| Main menu | Open options | Options overlay | Overlay writes to options/preferences | Returns to main menu; applied or reverted |
| Options | Apply GUI toggles | Options | Options owns commit; overlay validates | Settings persisted; redraw and restore |

The lifecycle is supported by focus management rules (Table 3), which are validated through integration tests.

Table 3 — Focus and lifecycle rules per screen

| Screen | Focus owner | Redraw hooks | Context switch rules | Pass-through policy |
|---|---|---|---|---|
| Gameplay | Game | ui_adaptor on resize | Contexts remain bound; gameplay actions active | None (input consumed by game unless overlay open) |
| Pause overlay | Overlay | ui_adaptor on redraw | Pause context bound to overlay | Optional hover hints only |
| In-game menu | Overlay | ui_adaptor on redraw | Context switch to overlay | None; underlay paused |
| Settings | Overlay | ui_adaptor on apply/resize | Options context for commit | None; underlay paused |
| Save/Load | Overlay | ui_adaptor on file op | List/context bound to overlay | None; underlay paused |

### Modal Stack and Focus

ui_manager mediates visibility and z-order. Overlays register for redraw/resize via ui_adaptor; they are removed from the stack on close, restoring the previous screen’s context and scheduling a redraw. This approach mirrors existing modal flows and avoids ad hoc state management.[^6]

### Input Contexts and Escape/Cancel

Each overlay constructs a local input_context with registered actions; cancel and confirm are mapped to consistent actions. Overlays do not appropriate global keys unless explicitly configured by the user. When overlays close, their context is released and the previous context resumes, ensuring that event routing is deterministic and free of leakage.[^5]

## Save/Load Interface: Visual File Browser with Thumbnails and Metadata

The Save/Load interface is a visual file browser built on uilist for the file grid, with a details panel and optional preview. Metadata is acquired from JSON headers and world files (version, world name, character, playtime, timestamp). Thumbnails are optional and display when available; otherwise, the UI shows a textual summary and a placeholder. Error handling covers corrupt JSON, version mismatches, and failed writes, with atomic save semantics and clear user feedback.[^7][^6]

Table 4 lists metadata fields used by the UI.

Table 4 — Save metadata fields and sources

| Field | Source | Type | Notes |
|---|---|---|---|
| savegame_version | savegame.cpp | int | Versioning; triggers migration |
| worldname | world data | string | Displayed; used for filtering |
| player name | player data | string | Displayed; sortable |
| playtime | game state | string or int | Display-friendly |
| save timestamp | file header | string | Localizable |
| character stats snapshot | player | compact struct | Preview only |
| screenshot path (optional) | user gfx | string | Optional preview image |

Robust file operations are essential. Table 5 details their semantics and recovery.

Table 5 — File operation semantics and error handling

| Operation | Semantics | Errors | Recovery |
|---|---|---|---|
| Load | Validate JSON, check version, migrate if needed; confirm overwrite if world active | Corrupt JSON, version mismatch | User-facing error; logs; revert |
| Save | Write to temp; atomic replace; update metrics | Disk full, permissions | Retry prompt; alternate path; keep session |
| Delete | Confirm; remove file | Locked/in-use | Retry; notify |
| Duplicate | Copy to new slot; update metadata | Name conflict | Auto-rename; confirm |
| Rename | Confirm; update references | Invalid characters | Normalize; prompt |

Thumbnails are optional and governed by the rules in Table 6.

Table 6 — Thumbnail availability and fallback behavior

| Condition | Location | Fallback |
|---|---|---|
| Tiles build + image present | user gfx area | Display scaled image |
| ASCII/curses or missing image | none | Textual summary; placeholder |
| Corrupt/missing preview | none | Hide image; warning icon |

### Metadata and Display

The UI reads JSON metadata during directory scans and binds it to list entries. Sorting and filtering use uilist capabilities; selecting an entry refreshes the details panel. If previews are missing, the UI displays a summary to maintain consistency across builds.[^7][^6]

### Thumbnails and Previews

The browser attempts to load saved screenshots from the user gfx area when the tiles build is active. Absent images, it displays a textual summary. No changes are made to core save JSON schemas; the image path is optional and never required for load.[^7][^14]

## Settings Panel Design: Key Bindings, Display Options, and GUI Toggles

Settings are organized into key bindings, display options (ASCII vs tiles), and GUI toggles. Key bindings use the established binding editor with conflict detection and resolution. Display options are partitioned by backend: curses font/color attributes and tiles font/scaling/resolution. GUI toggles include overlay enablement, theme, position, and opacity, all persisted via JSON preferences with validation and rollback. Options are applied through the options system; conflicts are prevented through validation and user prompts.[^5][^6][^4][^7]

Table 7 enumerates key settings and persistence.

Table 7 — Key settings and persistence

| Setting | Type | Default | Persistence location | Validation |
|---|---|---|---|---|
| Action bindings (per context) | map<string, string> | defaults | keybindings JSON | Conflict detection; fallback |
| Curses font | string | system default | options JSON | Path valid; else revert |
| Curses color pairs | map<string, string> | defaults | options JSON | Valid indices; else revert |
| Tiles font | string | default | options JSON | Path valid; else revert |
| Tiles resolution | int | 1024x768 | options JSON | Supported modes; clamp |
| UI scaling | float | 1.0 | options JSON | Clamp [0.5, 2.0] |
| Overlay enabled | bool | false | preferences JSON | Boolean; revert |
| Overlay theme | string | “default” | preferences JSON | Known IDs; fallback |
| Overlay opacity | float | 0.8 | preferences JSON | Clamp [0.0, 1.0] |
| Panel layout | string | “sidebar” | preferences JSON | Allowed values; revert |

Table 8 highlights backend-specific display options.

Table 8 — Display options by backend and UI effects

| Backend | Option | UI effect | Notes |
|---|---|---|---|
| Curses | Font face; color pairs | Text and border rendering | Pseudo-curses semantics |
| Curses | Attribute styles | Emphasis in menus | Color + attribute mapping |
| Tiles | Font selection; size | Text clarity and scale | SDL2_ttf handling |
| Tiles | Resolution; scaling | Panel geometry; layout | High DPI aware |
| Tiles | Pixel minimap; tile size | Visual density | Impacts HUD layout |

### Key Bindings Editor

Users search actions, view bindings, and resolve conflicts through the editor. Updates are saved via the options system; invalid configurations revert to prior bindings, with clear user messaging. Overlay actions remain distinct unless remapped by the user.[^5]

### Display Options Partitioning

Curses options tune text and colors within window buffers; tiles options manage fonts, resolution, and scaling. UI elements subscribe to ui_adaptor for redraw and resize events, ensuring layout coherency after changes.[^4][^6]

## Navigation Flow: Seamless Transitions and Focus Management

Navigation follows deterministic rules. From the main menu, new/load leads to gameplay; in-game menus open overlays; escape/cancel pops the stack. Focus is exclusive to the topmost overlay, and cancel/confirm mappings are consistent across screens. The model preserves the game loop’s authority over time advancement, autosave, and state mutations.[^6][^3][^5]

Table 9 summarizes the navigation map.

Table 9 — Navigation map and hotkeys

| Screen | Entry action | Exit actions | Hotkeys | Focus rules |
|---|---|---|---|---|
| Main menu | Launch sub-screens | Confirm/cancel; ESC quits or returns | Confirm; Cancel; Arrows | Main menu owns focus |
| Gameplay | Enter world | Open menu/pause | Menu; Pause | Gameplay owns focus; pause toggles |
| In-game menu | Open from gameplay | Confirm/close; ESC pops | Confirm; Cancel; Filter | Overlay owns focus |
| Settings | Open from main/in-game | Apply/close; ESC cancels | Confirm; Cancel; Tab | Overlay owns focus |
| Save/Load | Open from main/in-game | Confirm/close; ESC cancels | Confirm; Cancel; Search | Overlay owns focus |

## Rendering and Input Integration: Overlay Layering and Event Interception

Overlay layering is implemented without invasive changes to the compositor. Two variants are viable: final-frame compositing (Variant A) and manager-driven layering (Variant B). In both cases, overlays draw above the composed frame and remain opt-in. Input interception is conditional: when overlays are active, events inside hit regions are consumed; others pass through as configured. High DPI scaling is supported, and drawing pauses when minimized to avoid wasted work.[^4][^6][^5][^2]

Table 10 describes the event flow and consumption rules.

Table 10 — Event flow matrix

| Source | Dispatcher | Interception point | Handler | Consumption rule |
|---|---|---|---|---|
| SDL events | input_manager | Pre-routing (overlay active) | Overlay input | Events within overlay bounds consumed |
| SDL events | input_manager | Normal routing (overlay inactive) | Existing handlers | No interception |
| Overlay widgets | ui_manager | Focus/visibility | Overlay UI | Filtered by focus/visibility |
| Global hotkeys | options/action | On-press (overlay active) | Overlay hotkeys | Consumed if active; else ignored |

Table 11 consolidates acceptance criteria for integration.

Table 11 — Acceptance criteria matrix

| Integration point | Test type | Expected outcome | Pass/Fail criteria |
|---|---|---|---|
| Rendering layering | Visual + unit | Overlay draws above ASCII/tiles; no artifacts; stable z-order | No flicker; deterministic draw order; perf within budget |
| Input interception | Integration + unit | Overlay consumes events when active; passes when inactive | No event loss; no duplicate handling; configurable hotkeys validated |
| Menu/state transitions | Integration | Modal lifecycle correct; state restored after close | No state leak; deterministic transitions; no side effects |
| Save/load preferences | Unit + integration | Preferences saved/loaded; rollback on error; schema unchanged | No corruption; invalid values fallback gracefully |
| Character/inventory data | Unit + integration | Bindings accurate; no write-through; consistent updates | No desync; accurate snapshots; no perf regressions |

## Backward Compatibility: Preferences, Versioning, and Legacy Support

GUI preferences are stored in a dedicated JSON file, separate from world and character saves. Version checks and migration leverage existing savegame logic; invalid configurations revert to defaults and are reported to the user. The overlay is disabled by default; when inactive, behavior is identical to baseline, ensuring zero impact on existing workflows.[^7][^6]

Table 12 defines preferences keys, defaults, and fallbacks.

Table 12 — Preferences keys, defaults, and fallbacks

| Key | Type | Default | Validation | Fallback behavior |
|---|---|---|---|---|
| overlay.enabled | bool | false | Boolean | Revert to false |
| overlay.theme | string | “default” | Known theme IDs | Revert to “default” |
| overlay.position | string | “bottom-right” | Allowed positions | Revert to “bottom-right” |
| overlay.opacity | float | 0.8 | Clamp [0.0, 1.0] | Clamp and apply |
| overlay.hotkeys | array | [] | Action IDs or key tokens | Clear array |

Table 13 outlines backward-compatibility test cases.

Table 13 — Backward compatibility test matrix

| Scenario | Expected behavior | Pass criteria |
|---|---|---|
| Load legacy save | Migration used; metadata displayed | Successful load; clear messaging |
| Preferences missing/corrupt | Revert to defaults; write clean file | Run without crash; user notice |
| Overlay disabled | No overlay involvement | Behavior identical to baseline |
| ASCII vs tiles | Previews degrade gracefully | Consistent navigation; textual summary |
| Options conflicts | Detected and prevented | User warned; binding unchanged |

## UI Mockups and Widget Strategy

Menus reuse uilist, string_input_popup, and modal dialogs. Theming is realized through theme files that control colors, spacing, and typography. High DPI scaling and minimize-pause behaviors are supported. For developer tools and debug overlays, an immediate-mode GUI such as Dear ImGui is recommended; for core menus, an SDL2-native retained-mode library (libsdl2gui or SDL_gui) provides predictable, long-lived UI screens. This dual-path balances rapid iteration for tools with stable, componentized UIs for gameplay interfaces.[^6][^2][^8][^12][^13]

To connect the mockup strategy to concrete implementation, Table 14 maps screens to widgets, data sources, and event bindings.

Table 14 — Screen-to-widget mapping and integration

| Screen | Widgets | Data source | Event bindings | Integration steps |
|---|---|---|---|---|
| Save/Load | uilist grid; details; preview | Save metadata JSON | Confirm load/delete/rename; filter/search | Directory scan; bind actions; handle file ops |
| Settings | Tabs; editors; toggles | Options + preferences | Apply/revert; conflict detection; redraw | Register contexts; wire options; ui_adaptor hooks |
| Main menu | Button grid; world list | World factory | New/load/mods/options | Register actions; push overlays; redraw |
| Pause | Modal actions | Game state | Resume; save/load; settings | Pause underlay; focus overlay; handle escape |
| Dialogs | Confirm/alert popups | UI state | Confirm/cancel | Modal behavior; redraw; pass-through rules |

Mockup narrative:

- Main menu: a button grid with New, Load, Mods, Options, and a world list. Focused items are highlighted; navigation follows action bindings consistently.
- Save/Load: a list/grid of saves with a details panel and optional preview. Search and filtering are available; file operations prompt for confirmation and are guarded by atomic write semantics.
- Settings: tabs for key bindings, display, and GUI toggles. Apply/revert actions are explicit; invalid values fallback with messaging.
- Pause: minimal modal with Resume, Save/Load, and Settings. Escape returns to gameplay.

## Implementation Plan: Phased Delivery

Delivery proceeds in six phases to minimize risk and ensure incremental validation:

- Phase 1: Foundations—overlay module skeleton, PIMPL, hooks, and integration with ui_manager (inactive by default).
- Phase 2: Rendering and input—final-frame compositing or ui_manager registration, input adapters, and z-order handling. Validate across ASCII and tiles with High DPI and minimize-pause.
- Phase 3: Menu/state—modal push/pop, focus restoration, cancel/confirm semantics, and no side effects on gameplay.
- Phase 4: Save/Load UI—visual file browser, metadata, previews, robust file operations, and version checks.
- Phase 5: Settings—key bindings editor, display options, GUI toggles, preferences persistence with validation and rollback.
- Phase 6: Testing—unit/integration/performance tests; CI coverage; fix issues; finalize acceptance criteria.

Table 15 consolidates the plan.

Table 15 — Phases, deliverables, and criteria

| Phase | Tasks | Outputs | Entry criteria | Exit criteria |
|---|---|---|---|---|
| 1 | Skeleton, PIMPL, hooks | Overlay module; ui_manager integration | Baseline build | Hooks present; overlays off by default |
| 2 | Rendering, input, z-order | Overlay draws; interception | Phase 1 | Visual/input tests pass |
| 3 | Modal lifecycle | Push/pop; focus restore | Phase 2 | Deterministic transitions; no side effects |
| 4 | Save/Load UI | Browser; metadata; previews | Phase 3 | Robust file ops; error handling |
| 5 | Settings | Bindings; display; GUI toggles | Phase 4 | Preferences persisted; validation |
| 6 | Testing | Unit; integration; CI | Phase 5 | All acceptance criteria met |

## Risks, Constraints, and Open Questions

Key risks include event contention (overlays consuming needed events), z-order conflicts, JSON schema risks, and performance regressions. Constraints include the need to avoid changes to core save schemas and to keep overlays opt-in. Information gaps requiring confirmation are:

- Exact draw order and frame lifecycle for final-frame compositing insertion.
- Canonical input interception and focus rules.
- UI modal stacking and focus management details in ui_manager.
- Existing JSON preferences files and naming conventions.
- Platform-specific input quirks across SDL2 targets.
- Performance telemetry and budgets for overlays.
- Licensing constraints for third-party GUI libraries.

Table 16 lists the risk register.

Table 16 — Risk register

| Risk | Likelihood | Impact | Mitigation | Owner | Status |
|---|---|---|---|---|---|
| Event contention | Medium | Medium | Consumption rules; pass-through; configurable hotkeys | UI lead | Open |
| Z-order misdraw | Low–Medium | High | ui_manager layering; deterministic compositing | Rendering lead | Open |
| JSON schema risk | Low | High | Separate preferences; validation; rollback | Persistence lead | Open |
| Perf regression | Medium | Medium–High | Measure; pause when minimized; optimize draw | Engineering lead | Open |
| Input conflicts | Medium | Medium | Options validation; context-local bindings | Input lead | Open |
| DPI scaling | Medium | Medium | Scale factor handling; test matrices | QA | Open |
| Licensing | Unknown | Medium | Legal review of third-party libraries | Maintainers | Open |

## Appendices: Data Models, JSON Preferences, and Action Maps

Menu state stack schema:

Table 17 — Menu state stack schema

| Field | Type | Default | Lifecycle hooks |
|---|---|---|---|
| id | string | “” | on_init, on_open, on_close |
| type | enum | “overlay” | on_enter, on_exit |
| z_order | int | 100 | on_draw ordering |
| focus_policy | enum | “exclusive” | on_input routing |
| input_context | string | “” | register_action set |
| visibility | bool | true | on_show, on_hide |
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

- Main menu context: CONFIRM, CANCEL, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT.
- Gameplay context: MOVE_North/South/East/West, LOOK, PAUSE, OPEN_MENU.
- Overlay context: CONFIRM, CANCEL, FILTER, APPLY, REVERT, FOCUS_NEXT, FOCUS_PREV.

Bindings are loaded from JSON and validated by the options system; conflicts are detected and surfaced in the editor, with fallbacks to prior bindings on error.[^5][^6][^7]

---

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