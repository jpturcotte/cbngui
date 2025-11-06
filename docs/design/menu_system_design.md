# Cataclysm-BN Menu System Architecture: States, Save/Load UI, Settings, Navigation, and Backward Compatibility

## 1. Executive Overview and Objectives

This document specifies a practical, non-breaking architecture to deliver a modern, accessible menu system for Cataclysm: Bright Nights (BN). The design integrates cleanly with the codebase’s established separation of rendering, input, UI, gameplay state, and persistence, and respects both the ASCII/curses and SDL tiles backends. The scope comprises five pillars:

- Menu state management: a modal, stack-based UI that coexists with gameplay and the main menu.
- Save/Load interface: a visual file browser that uses existing JSON save metadata, with optional tiles thumbnails, preview, search, and robust error handling.
- Settings panel design: a structured configuration surface for key bindings, display options, and GUI toggles that persists via JSON and the options system.
- Navigation flow: deterministic transitions among main menu, gameplay, and overlays, with consistent focus, escape/cancel handling, and input routing rules.
- Backward compatibility: guarantees that the new menus do not alter core save JSON schemas and that legacy systems continue to function.

Guiding principles throughout are:

- Non-breaking integration: overlays are modal and opt-in; when disabled, behavior is unchanged.
- Encapsulation and PIMPL: game.h’s pointer-to-implementation pattern is used to shield dependencies and compilation impact.
- Layering and z-order: overlay rendering draws above the composed frame, never mutating the base renderer’s internals.
- JSON-safe preferences: GUI settings live in a dedicated preferences file with validation and rollback.
- Standards adherence: actions, options, and input contexts follow the project’s established patterns.

The proposed design leverages the project’s own documentation and source structure to remain congruent with SDL2 and UI conventions, ensuring a maintainable path to a polished user experience without regressions.[^1][^9]

## 2. Architectural Baseline: Systems and Boundaries

Cataclysm-BN is structured around distinct subsystems whose boundaries are stable and well-understood. Rendering splits into a pseudo-curses buffer and an SDL-based display compositor; input is mediated by a global input manager and per-mode contexts; UI uses shared components such as uilist and string_input_popup; the game class centralizes world state and the main loop; persistence relies on JSON with versioning and run-length encoding (RLE) where beneficial.[^1][^3][^4][^5][^6][^7]

To make responsibilities explicit, Table 1 summarizes each subsystem’s remit and key interfaces.

To orient the reader before we discuss layering and integration, the following table consolidates the core responsibilities and touchpoints that the menu system will build upon.

Table 1 — Core subsystems and responsibilities

| Subsystem | Files (representative) | Responsibility | Key interfaces |
|---|---|---|---|
| Rendering (ASCII/curses) | cursesport.cpp | Curses-like buffers, Unicode-aware text, window refresh | WINDOW, wnoutrefresh, doupdate |
| Rendering (SDL tiles) | sdltiles.cpp | Compositor that turns buffers into visible frames | Present-stage hooks, font/geometry utilities |
| Input | input.cpp | Global input manager, per-mode contexts, bindings | input_manager, input_context, action mapping |
| UI components | ui.cpp | uilist, string_input_popup, ui_manager, ui_adaptor | Modal lifecycle, redraw/resize hooks |
| Game state | game.cpp | Central loop (do_turn), world/map/overmap, events | game instance, uquit, safe_mode, autosave |
| Persistence | savegame.cpp | JSON serialization, versioning, RLE | JsonOut/JsonIn, migration, legacy loaders |

This baseline informs where and how the menu system inserts itself:

- Rendering overlays draw after the base frame is composed, respecting z-order and avoiding changes to the compositor.
- Input interception is localized and conditional—only when overlays are visible—then restored cleanly to existing routing.
- UI layering is mediated by ui_manager; menus are modal and push/pop the stack deterministically.
- Preferences use JSON, separate from world saves; conflicts with options are prevented through validation.
- Game state is never owned by UI; menus read for display and write settings via well-defined interfaces.[^3][^5][^6][^7][^4][^1]

## 3. Menu State Management: Integrating GUI Menus with Existing Game States

The menu system is modeled as a modal overlay stack managed by ui_manager. Each overlay registers a dedicated input_context that describes which actions it handles. When an overlay is active, the input manager first tests events against overlay hit regions; non-overlay events pass through to the underlying screen. When overlays are closed, the previous context is restored without side effects, and gameplay resumes seamlessly. This approach mirrors existing modal flows in the codebase and avoids invasive changes to the main loop.[^6][^3][^5]

Focus management is explicit: overlays own pointer and keyboard focus while visible; underlay screens remain paused as needed, and only allowed background interactions (such as hover or edge-gesture pass-through for resize hints) can be opt-in per overlay. Escape/cancel actions are bound consistently across menus, and confirm is mapped to an action consistent with the active context. The model preserves the game’s central authority: time advancement, autosave, and state mutations remain owned by the game class; UI only responds and redraws.[^3][^6]

To make transitions unambiguous, Table 2 outlines the state machine between gameplay, pause, and overlays.

Table 2 — UI state machine transitions

| Current UI | Event | Next UI | Ownership rules | Restore behavior |
|---|---|---|---|---|
| Gameplay | Open menu/overlay | Overlay (modal) | Overlay owned by ui_manager; no game state ownership | Prior UI paused; inputs redirected to overlay |
| Overlay (modal) | Confirm/Apply/Close | Prior UI context | Overlay popped; game state unchanged | Focus returns to prior context; redraw scheduled |
| Gameplay | Pause toggled | Gameplay paused + overlay | Pause logic owned by game; overlay limited to pause UI | Unpause restores gameplay; overlay dismissed |
| Main menu | Open options/settings | Overlay (settings) | Overlay temporary; writes to options/preferences | Returns to main menu; settings applied or reverted |
| Options | Apply GUI toggles | Options/Config | Options module owns commit; overlay validates | Settings persisted; prior focus restored |

These transitions are deterministic and tested through integration cases summarized in Table 3.

Table 3 — State transition test cases matrix

| Case | Precondition | Action | Expected next state | Pass criteria |
|---|---|---|---|---|
| Open settings during gameplay | Game active; overlay inactive | Invoke settings hotkey | Settings overlay visible | Input redirected; underlying screen paused; no state mutations |
| Apply and close settings | Settings overlay open | Confirm apply | Gameplay | Options persisted; prior focus restored; no stray inputs |
| Pause then open menu | Game active | Toggle pause, then open menu | Pause overlay | Pause preserved; overlay modal; escape returns to pause or gameplay based on rules |
| Open and cancel | Overlay visible | Press cancel | Prior UI | No changes persisted; focus restored; redraw coherent |
| Main menu to settings | Main menu active | Open settings | Settings overlay | Returns cleanly; main menu remains intact; no world loaded |

### 3.1 Modal Stack and Focus Management

Menus follow a push/pop model: each overlay is registered with ui_manager, which handles visibility, z-order, and input focus. The topmost overlay consumes events inside its hit regions; other events pass through per the overlay’s configuration (e.g., to support edge interactions or hover effects). When dismissed, the overlay is popped; the previous screen resumes with restored focus and a redraw scheduled via ui_adaptor.[^6]

### 3.2 Input Context Registration and Escape/Cancel Handling

Every screen or overlay constructs a local input_context and registers the actions it handles. Overlays bind cancel/confirm to consistent actions that match the project’s action definitions. Conflict detection for keybindings is inherited from the options system; overlays must either use configurable bindings or avoid colliding with defaults. During overlay activation, the global input manager routes events to the overlay context first, preserving deterministic behavior.[^5][^6]

## 4. Save/Load Interface: Visual File Browser with Thumbnails and Metadata

The save/load interface leverages BN’s JSON-based persistence. Each world and character save has a set of metadata fields that the UI can read without deserializing the full world. When a save is selected, details are displayed in a side panel, and a preview area shows summary information, player status, and an optional tiles thumbnail if available. Sorting, filtering, and search are implemented via uilist patterns, with safe error handling and validation on load.[^7][^6][^14]

Table 4 lists the metadata fields exposed to the UI.

Table 4 — Save metadata fields and sources

| Field | Source | Type | Notes |
|---|---|---|---|
| savegame_version | savegame.cpp | int | Version number; used to check compatibility and migration |
| worldname | world data | string | Name of the world; displayed and used for filtering |
| player name | player data | string | Character name; sortable |
| playtime | game state | string or int | In-game time or session duration; display-friendly |
| save timestamp | file system or JSON header | string | Last modified time; localizable display |
| character stats snapshot | player data | compact struct | Minimal stats for preview |
| screenshot path (optional) | user gfx area | string | Tiles thumbnail; missing gracefully handled |

The file operations surface must be robust. Table 5 defines semantics for load, save, delete, duplicate, and rename, with error reporting and recovery.

Table 5 — File operation semantics and error handling

| Operation | Semantics | Errors | Recovery |
|---|---|---|---|
| Load | Validate JSON, check version, migrate if needed; confirm overwrite if world active | Corrupt JSON, version mismatch | Report to user; offer view logs; fall back to prior screen |
| Save | Write to a temp file, then atomic replace; record autosave metrics | Disk full, permissions | Show clear error; keep current session; allow retry or alternative path |
| Delete | Confirm dialog; remove selected file(s) | Locked or in-use | Show error; retry after release |
| Duplicate | Copy to new slot; update metadata | Path conflicts | Auto-rename or prompt; confirm before overwrite |
| Rename | Confirm dialog; update references | Name conflict | Suggest unique name; validate characters |

Thubnails are optional. Table 6 describes how preview images are located and what happens if they are missing.

Table 6 — Thumbnail availability rules

| Condition | Location | Fallback behavior |
|---|---|---|
| Tiles build with screenshot saved | user gfx subfolder per save | Show image; scale to fit preview area |
| ASCII/curses build or missing image | none | Show textual summary; icon placeholder |
| Version mismatch or corrupt preview | none | Hide image; show warning icon and metadata |

### 4.1 Metadata Acquisition and Display Strategy

Metadata is read from JSON headers and world files during directory scans. The UI displays the world name, character name, playtime, and last saved timestamp. When a save is selected, uilist refreshes the details panel; sorting and filtering use the list’s built-in mechanisms. If a preview is unavailable, the UI presents a textual summary and an appropriate placeholder.[^7][^6]

### 4.2 Thumbnails and Previews

The interface attempts to load a saved screenshot from the user gfx area. If the tiles build is active and an image exists, it is displayed; otherwise, the UI shows a textual preview. The design degrades gracefully across ASCII/curses and tiles builds, avoiding any change to core save JSON schemas and keeping the image optional to prevent user-facing failures.[^7][^14]

## 5. Settings Panel Design: Key Bindings, Display Options, and GUI Toggles

Settings are split into three groups: key bindings, display options (ASCII/tiles), and GUI toggles. Key bindings are edited via the existing binding editor pattern, with conflict detection and resolution through the options system. Display options are partitioned by backend: curses-related font and color attributes, and SDL2 tiles font, resolution, and scaling. GUI toggles include overlay enablement, theme selection, and panel layout, all persisted in a dedicated JSON preferences file with validation and rollback.[^5][^6][^4][^7]

Table 7 enumerates key settings and their persistence.

Table 7 — Key settings and persistence

| Setting | Type | Default | Persistence location | Validation |
|---|---|---|---|---|
| Action bindings (per context) | map<string, string> | project defaults | keybindings JSON (options) | Conflict detection; fallback to defaults |
| Font face (curses) | string | system default | options JSON | Must exist; else revert |
| Color pairs (curses) | map<string, string> | project defaults | options JSON | Valid color indices; else revert |
| Tiles font | string | project default | options JSON | Path validation; fallback |
| Tiles resolution | int | 1024x768 | options JSON | Supported modes; else clamp |
| UI scaling | float | 1.0 | options JSON | Clamp to [0.5, 2.0] |
| Overlay enabled | bool | false | GUI preferences JSON | Boolean; else revert |
| Overlay theme | string | “default” | GUI preferences JSON | Known theme IDs; else fallback |
| Overlay opacity | float | 0.8 | GUI preferences JSON | Clamp [0.0, 1.0] |
| Panel layout | string | “sidebar” | GUI preferences JSON | Allowed layouts; else revert |

Display options vary by backend; Table 8 highlights differences and effects.

Table 8 — Display options by backend and UI effects

| Backend | Option | Effect on UI | Notes |
|---|---|---|---|
| Curses | Font face, color pairs | Text rendering and colors in windows | Uses cursesport color handling |
| Curses | Border and attribute styles | Borders and emphasis in menus | Aligns with pseudo-curses semantics |
| Tiles | Font selection, size | Text clarity and scaling | Uses SDL2_ttf; High DPI handling |
| Tiles | Resolution and scaling | Panel sizes and layout responsiveness | Scale affects widget geometry |
| Tiles | Pixel minimap and tile size | Visual density and clarity | Impacts HUD layout and readability |

### 5.1 Key Bindings Editor

The editor uses the existing action mapping and binding patterns. Users can search actions, view current bindings, and resolve conflicts by reassigning keys or sequences. The editor saves updates to the keybindings JSON through the options system and restores previous bindings if validation fails. All changes are local to contexts; gameplay actions remain distinct from overlay actions unless explicitly mapped.[^5]

### 5.2 Display Options Partitioning (ASCII vs Tiles)

Display options are split by backend. Curses options affect text rendering and color pairs within pseudo-curses windows; tiles options manage font, resolution, scaling, and pixel minimap settings. UI elements register for redraw via ui_adaptor; resizing triggers layout recalculation and refresh.[^4][^6]

## 6. Navigation Flow: Transitions Between Menu Screens and Back to Gameplay

Navigation is governed by deterministic rules: from the main menu, new/load leads to gameplay; in-game menus open overlays; escape sequences pop the stack or return to the prior context. Focus is consistent—only one screen owns input at a time—and all transitions schedule redraws through ui_manager. Input contexts define action bindings per screen, and cancel/confirm actions are mapped consistently to avoid confusion.[^6][^3][^5]

Table 9 provides a navigation map across common screens.

Table 9 — Navigation map and hotkeys

| Screen | Entry action | Exit actions | Hotkeys | Focus rules |
|---|---|---|---|---|
| Main menu | Launch new/load/mods/options | Confirm/cancel; ESC returns or quits | Confirm, Cancel, Arrow nav | Main menu owns focus; overlays pushed as needed |
| Gameplay | Enter world | Open menu/pause | Menu, Pause | Gameplay owns focus; pause toggles state |
| In-game menu | Open from gameplay | Confirm/apply/cancel; ESC pops | Confirm, Cancel, Filter | Overlay owns focus; underlay paused |
| Settings | Open from main or in-game | Apply/close; ESC reverts or cancels | Confirm, Cancel, Tab nav | Overlay owns focus; options commit |
| Save/Load | Open from main or in-game | Confirm/close; ESC cancels | Confirm, Cancel, Search | Overlay owns focus; file ops guarded |

## 7. Rendering and Input Integration: Overlay Layering and Event Interception

The overlay draws above both ASCII/curses and tiles compositions without modifying base renderer internals. Two integration variants are viable:

- Variant A: final-frame compositing at a present-stage hook.
- Variant B: managed layering via ui_manager.

Both preserve the existing rendering path and keep overlays opt-in. Input events are intercepted only when overlays are active; hit-testing determines consumption, and the remainder passes through to underlying screens. High DPI scaling, focus handling, and minimize-pause are supported, and drawing pauses when the window is minimized to avoid wasted work.[^4][^6][^5][^2]

To clarify event routing, Table 10 details the event flow and consumption rules.

Table 10 — Event flow matrix

| Source | Dispatcher | Interception point | Handler | Consumption rule |
|---|---|---|---|---|
| SDL events | input_manager | Pre-routing (overlay active) | Overlay input adapter | Events inside overlay bounds are consumed |
| SDL events | input_manager | Normal routing (overlay inactive) | Existing handlers | No change; pass-through |
| Overlay widgets | ui_manager | Focus/visibility | Overlay UI | Filtered by focus and visibility |
| Global hotkeys | options/action | On-press (overlay active) | Overlay hotkey | Consumed if active; otherwise ignored |

Accessibility and input remapping are first-class. Overlay hotkeys are configurable; bindings avoid collisions with gameplay actions unless explicitly mapped by the user. Direction handling and multi-key sequences remain the domain of input_context.[^5]

## 8. Backward Compatibility: Ensuring Menus Work with Existing Save/Load Systems

Backward compatibility is preserved by isolating GUI preferences from core save JSON. Overlay settings use a dedicated preferences JSON file, separate from world and character saves. Versioning is respected: on load, the UI reads metadata and uses migration logic already present in savegame routines. If configuration is invalid, the UI reverts to defaults and reports the issue. When the overlay is disabled, the game behaves exactly as before, with no dependencies on GUI features.[^7][^6]

Table 11 lists preferences keys and their validation and fallback rules.

Table 11 — Preferences keys, defaults, and fallbacks

| Key | Type | Default | Validation | Fallback behavior |
|---|---|---|---|---|
| overlay.enabled | bool | false | Boolean | Revert to false |
| overlay.theme | string | “default” | Known theme IDs | Revert to “default” |
| overlay.position | string | “bottom-right” | Allowed positions | Revert to “bottom-right” |
| overlay.opacity | float | 0.8 | Clamp [0.0, 1.0] | Clamp and apply |
| overlay.hotkeys | array | [] | Action IDs or key tokens | Clear array |

Table 12 clarifies backward-compatibility test cases.

Table 12 — Backward compatibility test matrix

| Scenario | Expected behavior | Pass criteria |
|---|---|---|
| Load legacy save (pre-version) | Use migration; display metadata; no schema changes | Successful load; clear messaging if migration occurred |
| Preferences missing or corrupt | Revert to defaults; write a clean file | Game runs; no crashes; clear user notice |
| Overlay disabled | No overlay rendering or input interception | Identical behavior to baseline |
| Tiles vs ASCII builds | Fallbacks for previews; textual summary in ASCII | Graceful degradation; consistent navigation |
| Options conflicts | Detect at edit time; prevent save | User warned; binding unchanged |

## 9. UI Mockups and Widget Strategy

The menu system reuses established widgets: uilist for navigable lists and grids, string_input_popup for text entry, and modal dialogs for confirmations. Theming follows the project’s visual style and is realized through theme files that control colors, spacing, and typography. High DPI scaling is supported; minimize-pause behavior prevents wasted rendering. For developer tools and debug overlays, an immediate-mode GUI (IMGUI) such as Dear ImGui is suitable, while retained-mode SDL2-native libraries (e.g., libsdl2gui or SDL_gui) serve long-lived in-game menus. This dual approach balances rapid iteration for tools with a predictable, componentized UI for core menus.[^6][^2][^8][^12][^13]

To connect the mockup strategy to concrete widgets and integration steps, Table 13 maps screens to widgets and event bindings.

Table 13 — Screen-to-widget mapping and integration

| Screen | Widgets | Data source | Event bindings | Integration steps |
|---|---|---|---|---|
| Save/Load | uilist grid, details panel, preview area | Save metadata JSON | Confirm load, delete, rename; filter/search | Build list from directory scan; bind actions; handle file ops |
| Settings | Tabs for key bindings, display, GUI toggles | Options JSON + preferences | Apply/revert; conflict detection; redraw on change | Register contexts; wire options; ui_adaptor for resize |
| Main menu | Button grid, list of worlds | World factory data | New/load/mods/options | Register actions; push overlays; redraw |
| Pause | Modal with actions | Game state | Resume, save/load, settings | Pause underlay; focus overlay; handle escape |
| Dialogs | Confirm/alert popups | UI state | Confirm/cancel | Modal behavior; redraw; pass-through rules |

Mockup descriptions:

- Main menu: a clean grid of buttons (New, Load, Mods, Options) with a list of worlds. Focused item is highlighted; keyboard and gamepad navigation follow action bindings.
- Save/Load: a list/grid of saves with a details panel showing world name, character, playtime, and timestamp. A preview area shows a thumbnail (if available) or a textual summary. Search and filtering live at the top; file operations are guarded by confirm dialogs.
- Settings: tabs for key bindings (searchable list, conflict detection), display (backend-specific options), and GUI toggles (overlay enablement, theme, position, opacity). Apply and revert are explicit; changes trigger redraws and validation.
- Pause: minimal modal overlay with Resume, Save/Load, and Settings. Escape returns to gameplay; focus remains on the overlay until dismissed.

## 10. Implementation Plan: Phased Delivery and Milestones

Delivery proceeds in phases to mitigate risk and ensure incremental validation. Each phase has clear outputs, entry and exit criteria, and test coverage.

Phase 1: Foundations. Introduce the overlay module skeleton with PIMPL-backed state. Integrate with ui_manager for layering and focus; add present-stage hooks (Variant A) or registration (Variant B). Keep overlays inactive until enabled via preferences. Entry: baseline build. Exit: hooks present and tested; overlays do not draw unless enabled.

Phase 2: Rendering and input. Implement final-frame compositing or ui_manager registration, overlay input adapters, and z-order handling. Validate across ASCII and tiles builds, with High DPI scaling and minimize-pause behavior. Entry: Phase 1 complete. Exit: visual and input tests pass; no flicker or event contention.

Phase 3: Menu/state lifecycle. Implement push/pop behavior, deterministic restoration, and cancel/confirm bindings. Ensure no side effects on gameplay state. Entry: Phase 2 complete. Exit: state tests pass; focus restored correctly; redraw coherent.

Phase 4: Save/Load UI. Build the visual file browser: metadata panels, previews, search and sort, robust file operations with error handling and atomic writes. Entry: Phase 3 complete. Exit: load/save/delete/rename/duplicate work; invalid JSON handled gracefully; version checks confirmed.

Phase 5: Settings. Deliver key bindings editor integration, display options partitioning, and GUI toggles. Persist preferences in a dedicated JSON file with validation and rollback. Entry: Phase 4 complete. Exit: options round-trips succeed; conflicts detected; fallbacks verified.

Phase 6: Testing. Unit, integration, and performance tests. Add CI coverage, regression thresholds, and cross-backend validation. Entry: Phases 1–5 complete. Exit: all acceptance criteria met; no performance regressions.

Table 14 summarizes the phased plan.

Table 14 — Phases, deliverables, and acceptance criteria

| Phase | Tasks | Outputs | Entry criteria | Exit criteria |
|---|---|---|---|---|
| 1 | Skeleton, PIMPL, hooks | Overlay module, integration points | Baseline build | Hooks present; overlays inactive until enabled |
| 2 | Rendering, input, z-order | Overlay draws; input interception | Phase 1 | Visual + input tests pass |
| 3 | Modal lifecycle, transitions | Overlay push/pop; focus rules | Phase 2 | Deterministic restoration; no side effects |
| 4 | Save/Load UI | File browser with metadata, previews | Phase 3 | Robust file ops; error handling; version checks |
| 5 | Settings | Key bindings, display, GUI toggles | Phase 4 | Preferences persisted; validation and rollback |
| 6 | Testing | Unit, integration, CI | Phase 5 | Acceptance criteria matrix met; no perf regressions |

## 11. Risks, Constraints, and Open Questions

The plan flags specific information gaps and constraints to be addressed in early phases:

- Event contention: overlays consuming needed events. Mitigation: strict consumption rules, configurable hotkeys, and pass-through when appropriate.[^5][^6]
- Z-order conflicts: overlay drawn under or above unintended layers. Mitigation: explicit z-order via ui_manager and deterministic compositing.[^6]
- JSON schema risks: changes affecting saves. Mitigation: isolate preferences in a separate file; validate before apply; rollback on error.[^7]
- Performance regressions: overlay rendering altering frame pacing. Mitigation: measure per-frame cost; pause drawing when minimized; leverage efficient IMGUI techniques for tools where appropriate.[^2][^6][^8][^13]
- Information gaps: exact draw order and frame lifecycle in the main loop; canonical input interception patterns; UI modal stacking details; JSON preferences naming conventions; platform-specific input quirks; performance telemetry; licensing constraints. These must be verified before or during Phase 1–2.[^1][^2][^6][^7]

Table 15 enumerates the risk register.

Table 15 — Risk register

| Risk | Likelihood | Impact | Mitigation | Owner | Status |
|---|---|---|---|---|---|
| Event contention | Medium | Medium | Consumption rules; pass-through; configurable hotkeys | UI lead | Open |
| Z-order misdraw | Low–Medium | High | ui_manager layering; deterministic compositing | Rendering lead | Open |
| JSON schema changes | Low | High | Separate preferences; validation; rollback | Persistence lead | Open |
| Perf regression | Medium | Medium–High | Measure; optimize; minimize-pause | Engineering lead | Open |
| Input conflicts | Medium | Medium | Options validation; context-local bindings | Input lead | Open |
| DPI scaling issues | Medium | Medium | Scale factor handling; test matrices | QA | Open |
| Licensing constraints | Unknown | Medium | Legal review of third-party libraries | Project maintainers | Open |

## 12. Appendices: Data Models, JSON Schemas, and File Structures

This appendix outlines the proposed data models for the menu state stack, GUI preferences, and UI action maps, with example JSON and default values. The aim is to make the persistence explicit and testable.

Table 16 defines menu state stack entries and lifecycle hooks.

Table 16 — Menu state stack schema

| Field | Type | Default | Lifecycle hooks |
|---|---|---|---|
| id | string | “” | on_init, on_open, on_close |
| type | enum | “overlay” | on_enter, on_exit |
| z_order | int | 100 | on_draw ordering |
| focus_policy | enum | “exclusive” | on_input routing |
| input_context | string | “” | register_action set |
| visibility | bool | true | on_show, on_hide |
| parent | string or null | null | stack push/pop |

GUI preferences JSON proposal (defaults and validation rules):

- overlay.enabled: false (bool)
- overlay.theme: “default” (string; must match known theme IDs)
- overlay.position: “bottom-right” (string; one of allowed positions)
- overlay.opacity: 0.8 (float; clamp [0.0, 1.0])
- overlay.hotkeys: [] (array; validated against action IDs or key tokens)
- panel.layout: “sidebar” (string; allowed layouts)

Example snippet:

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

UI action map skeleton for contexts:

- Main menu context: CONFIRM, CANCEL, NAV_UP, NAV_DOWN, NAV_LEFT, NAV_RIGHT
- Gameplay context: MOVE_North, MOVE_South, MOVE_East, MOVE_West, LOOK, PAUSE, OPEN_MENU
- Overlay context: CONFIRM, CANCEL, FILTER, APPLY, REVERT, FOCUS_NEXT, FOCUS_PREV

Each context registers actions via input_context; bindings are loaded from JSON and validated by the options system. Conflicts are detected and surfaced in the editor; fallbacks restore previous bindings on error.[^5][^6][^7]

---

## Information Gaps

A small set of specifics require confirmation during implementation:

- Canonical draw order and frame lifecycle insertion points for final-frame compositing.
- Input interception patterns and focus rules currently used by the project beyond event polling.
- UI modal stacking and focus management details in ui_manager for complex overlays.
- Existing JSON preferences files and naming conventions for overlay settings.
- Platform-specific input quirks across SDL2 targets that may impact overlays.
- Performance budgets and telemetry for overlays to avoid regressions.
- Licensing constraints for any third-party GUI libraries adopted beyond SDL2-native code.

These are non-blocking for the overall plan and will be resolved during Phase 1–2 as indicated.

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