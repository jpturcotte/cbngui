# GUI Components Architecture for the Cataclysm-BN MVP

## Executive Summary and Design Goals

This design specifies a cohesive, production-ready graphical user interface (GUI) for the Cataclysm: Bright Nights (Cataclysm-BN) minimum viable product (MVP). It defines four core components—the Visual Map Overlay, Inventory Panel, Character Status Bar, and Message Log Panel—along with the interaction model that binds them to the game’s world state and input system. The design prioritizes the existing ASCII/curses rendering surface and the established UI manager (ui_manager) and panel constructs. It also outlines a safe path to extend into a tiles-backed retained-mode GUI if a richer widget set and theming become advantageous in later phases.

Three design principles guide the work. First, preserve the game’s rendering and UI boundaries: ASCII/curses remains authoritative for the base scene; overlay UI layers draw above it without re-architecting the frame pipeline. Second, use the existing input and event systems; GUI hotkeys and mouse events are routed through input_context and handle_action with deterministic consumption rules. Third, rely on const-safe data bindings and subscription to change events to keep UI state in sync without introducing write-through coupling to gameplay systems.

Key deliverables are:

- Visual Map Overlay: ASCII cell rendering with color coding, selection highlights, and deterministic cursor behavior, aligned to the pseudo-curses buffer model and composited above the base frame.
- Inventory Panel: A clickable, scrollable, filterable list that supports drag-and-drop to ground, equipment, or other containers, with per-item tooltips and contextual actions.
- Character Status Bar: A compact, glanceable panel for health, stats, status effects, and condition bars with high/low thresholds and change animations.
- Message Log Panel: A timestamped, scrollable, color-typed message stream with auto-scroll, filter controls, and stable integration with the game’s message subsystems.

Each component ships with concrete layout specifications, data bindings, update mechanisms, performance guidelines, and testing plans. Non-functional outcomes include low latency, minimal per-frame cost, deterministic focus handling, and stable behavior across ASCII and tiles configurations. The design explicitly avoids breaking gameplay or save semantics and, where applicable, isolates preferences in separate JSON. It therefore aligns with Cataclysm-BN’s documented architecture and development practices while providing a clear roadmap for MVP scope and future enhancements.[^1][^9]

## Architectural Baseline and Constraints

Cataclysm-BN’s rendering stack separates a curses-like buffer from the final display compositor. Cursesport provides WINDOW and associated print/attribute APIs, stores lines and cells (cursecell/curseline), supports wide characters, and uses buffered refresh (wnoutrefresh/doupdate). The same ASCII/curses output can be presented directly or routed through an SDL-based tiles compositor (sdltiles), depending on build configuration.[^4]

UI construction leverages reusable components—uilist for menus and lists, string_input_popup for text fields, and ui_manager/ui_adaptor for redraw/resize coordination. Windows (catacurses::window) are the canonical rendering surface. Input routing is handled by input_manager and per-mode input_context, with action IDs, mouse coordinates, and support for gamepad. A central game instance owns world state and orchestrates the update cycle, ensuring that UI and rendering remain decoupled from gameplay logic.[^6][^5][^3]

Constraints are deliberate. The overlay UI must be compatible with the ASCII/curses renderer and sdltiles. Input handling must not hijack gameplay hotkeys nor introduce contention. UI state must avoid write-through to core gameplay structures, favoring read-only bindings and change subscriptions. Save semantics remain unchanged; preferences are isolated from world state. These constraints reflect the codebase’s stable boundaries and known integration points.[^4][^6][^5][^3]

To align terminology, the following table maps key source constructs to how the MVP UI will refer to them.

Table 1. Terminology mapping: BN constructs to MVP UI terminology

| BN Construct/Module | Role in BN | MVP UI Term | Notes |
|---|---|---|---|
| WINDOW (cursesport) | Curses-like window buffer | UI Surface | Base for ASCII text and attributes; wide char invariants apply[^4] |
| curseline/cursecell | Text storage per line/cell | ASCII Cell Model | Each cell holds UTF‑8 char, FG/BG color[^4] |
| ui_manager | Redraw coordination | UI Manager | Orchestrates redraws, focus, and layering[^6] |
| ui_adaptor | Resize/redraw hooks | UI Hooks | on_redraw/on_screen_resize callbacks[^6] |
| uilist | Reusable list UI | List Widget | Used for inventory and message views[^6] |
| string_input_popup | Text input | Text Field Widget | For search/filter input where needed[^6] |
| input_context | Mode-specific input | Input Router | Routes events to actions; supports mouse and gamepad[^5] |
| game | Core world state | World State | Owns map, calendar, event bus, etc.[^3] |

## Component Architecture Overview

The MVP UI comprises four components with distinct responsibilities but shared update and event routing policies. Each component owns a UI Surface (a catacurses::window or retained-mode widget container), exposes read-only data bindings to game state, and registers an input_context for hotkeys and mouse actions when it has focus. Redraws and resizes are coordinated by ui_manager/ui_adaptor to ensure deterministic draw order and to avoid redundant work. The frame lifecycle remains anchored to the main loop and the game’s orchestration, with UI subscriptions for state changes and periodic refresh for time-based effects.

The focus model is priority-based. When the player opens the Inventory Panel, the panel receives focus and consumes relevant input; other components continue to receive hover or passive updates but do not block events intended for the active panel. Hotkeys are configurable and validated to avoid conflicts with global gameplay bindings.

Table 2 catalogs the component responsibilities, input contexts, and update triggers.

Table 2. Component responsibility matrix

| Component | Primary UI Surface | Data Sources | Input Context (examples) | Redraw Triggers |
|---|---|---|---|---|
| Visual Map Overlay | ASCII overlay region (catacurses::window), above base scene | map visibility/lighting, critter_tracker, player position | MOVE, LOOK, TOGGLE_OVERLAY, SELECT; mouse: hover, click | On MOVE/LOOK; on map or visibility change; periodic for time-based effects[^4][^3] |
| Inventory Panel | Scrolling list (uilist) + details pane | item stacks (character inventory and ground), equipment slots | SELECT_ITEM, DROP, EXAMINE, FILTER, DRAG_SOURCE/DRAG_TARGET | On inventory/equipment change; on filter input; scroll events[^6][^3] |
| Character Status Bar | Compact bar with icons/progress | player health/stats, status effects | TOGGLE_STATUS_DETAILS, SCROLL_CONDITIONS | On health/stat/effect change; periodic tick for timers[^3] |
| Message Log Panel | Scrollable list (uilist) with timestamps | message bus / in-game message store | FILTER_MSG, TOGGLE_AUTO_SCROLL, SCROLL | On new message; on filter toggle; user scroll[^6][^3] |

### Update and Redraw Coordination

Updates follow a publish/subscribe pattern layered on existing constructs. When gameplay state changes—e.g., inventory mutated, health adjusted, a message published—interested UI components receive a change notification (via the game’s event bus or explicit subscriptions) and schedule a redraw. If a component is resized (terminal size change or panel layout update), ui_adaptor invokes on_screen_resize hooks so the component can recompute layout and repaint. ui_manager drives the final redraw orchestration to avoid interleaving artifacts and redundant paints.

Time-based UI updates (e.g., blinking highlights, status effect timers) are throttled to a sensible cadence (e.g., 200–250 ms) so that the ASCII renderer and overlay widgets update smoothly without monopolizing frame time. The pattern adheres to the codebase’s UI manager and adaptor conventions and complements the buffered window refresh model.[^6][^4]

## Visual Map Overlay: ASCII Tile Rendering with Color Coding and Highlights

The Visual Map Overlay renders a selected region of the game map as ASCII tiles using the cursesport’s cell model. It leverages the existing WINDOW buffer, cursecell/curseline storage, color pairs, and wide-character invariants to ensure the overlay is both performant and consistent with the base scene. Overlay highlights (selection rectangles, path previews, hover indicators) are drawn in a deterministic z-order above the base map but within the ASCII buffer semantics. The design assumes the overlay region may be a full view or a subregion around the player; in either case, it must respect scrolling (via view offsets), lighting/visibility, and the event bus to refresh only the parts that change.

### Layout and Render Spec

Each ASCII cell displays a character with explicit foreground and background colors. The overlay uses the same color/attribute mechanisms as cursesport, mapping semantic states (terrain, item, creature, selection) to color pairs and attributes (e.g., bold for hover). Wide characters are handled as per cursesport invariants: when a two-cell character is placed, the next cell is cleared to preserve alignment. The overlay composes highlights on top of the base tile using a deterministic rule: base tile first, then highlight overlay (border, underline, or background tint), then cursor/selection marker if applicable. If the overlay is dimmed or paused, it still respects window-level attributes to avoid visual artifacts.

To make the rendering mappings explicit, the following table defines default color and attribute assignments. These defaults can be themed later; the key is to keep the mapping consistent and legible.

Table 3. Color/attribute mapping by tile type

| Tile Type | Character (default) | Foreground | Background | Attribute | Notes |
|---|---|---|---|---|---|
| Terrain (walkable) | '.' or ',' | Neutral (e.g., light gray) | Transparent | None | Follows map visibility; dim when not in LoS |
| Terrain (wall) | '#' | Medium gray | Transparent | None | Wall glyphs consistent with tileset |
| Item (ground) | '!' or other stack indicator | Yellow | Transparent | Bold | Highlight on hover; stack count via suffix |
| Creature (NPC/monster) | '@' or sprite code | Red (hostile), Green (friendly), Cyan (neutral) | Transparent | None | Color encodes faction/attitude |
| Player | '@' | White | Transparent | Bold | Cursor underline indicates focus |
| Selection/hover | '_' underline or background tint | High-contrast (e.g., white or cyan) | Semi-transparent tint | Bold | Deterministic layering over base |
| Path preview | '.' with background tint | Cyan | Semi-transparent cyan | None | Blink/throttle to avoid strobe |
| Out-of-LoS | '.' or terrain | Gray | Transparent | None | Dimming applied for clarity |

These assignments align with the cursesport buffer model and attribute handling while keeping the overlay readable in both ASCII and tiles builds.[^4]

### Data Bindings and Update Mechanisms

The overlay binds to map data (local map visibility, lighting, items, vehicles, creatures), the player’s position, and view offsets. It subscribes to:

- Movement and look actions (MOVE_n, LOOK), which adjust the view and cursor.
- Visibility/lighting changes, which alter tile brightness or dimming.
- Creature_tracker updates (spawn/despawn/move), which change occupant glyphs or colors.
- Event bus pulses for time-based effects (e.g., periodic weather or light changes).

It uses a dirty-region strategy to minimize repaint cost. When a tile changes, only its cell is marked dirty; wnoutrefresh flushes buffered changes efficiently, and sdltiles composites the final frame. Throttled redraws are used for time-varying highlights (e.g., 200–250 ms cadence) to avoid excessive work on static frames.[^3][^4]

### Interaction Model

The overlay accepts keyboard and mouse inputs. Directional keys adjust the view or cursor; look action freezes the scene for inspection; toggle overlay action shows or hides the overlay region. Mouse hover moves a cursor highlight; click commits a selection. Focus handling is explicit: when the overlay is active, it consumes movement and look events; when inactive, events pass through to the underlying screen.

Table 4. Overlay input map and consumption rules

| Action/Event | Description | Focus Requirement | Consumption Rule |
|---|---|---|---|
| MOVE_<dir> | Move cursor/view | Overlay active | Consumed to update overlay region |
| LOOK | Enter look mode | Overlay active or passive | Consumed to freeze and inspect |
| TOGGLE_OVERLAY | Show/hide overlay | None | Not consumed by passive overlay |
| MOUSE_HOVER | Move cursor highlight | Overlay visible | Not consumed; hover effect only |
| MOUSE_CLICK | Select tile | Overlay visible | Consumed if selection is made |
| SCROLL | Pan view | Overlay visible | Consumed for paging the view |

The model follows input_context conventions and respects the event pipeline, ensuring deterministic behavior across screens and modes.[^5]

### Integration Points

The overlay composes above the ASCII base by queuing its window updates in the same buffered refresh cycle. If the tiles renderer is active, it composites ASCII cells to the final surface in sdltiles; the overlay is drawn immediately after the base scene, preserving z-order. Input interception is limited to when the overlay is visible and focused; otherwise, it is transparent to events. This approach adheres to the documented rendering and event pipeline and avoids invasive changes.[^4][^5]

### Performance and Testing

Performance goals are conservative: the overlay must not add visible stutter or increase frame time measurably. Unit tests verify correct cell layout, wide-character invariants, attribute application, and dirty-region redraws. Visual tests confirm that overlays draw above the base scene, with deterministic highlight layering and no flicker.

Table 5. Acceptance criteria for Visual Map Overlay

| Criterion | Test Type | Expected Outcome | Pass/Fail |
|---|---|---|---|
| Wide-char invariants | Unit | Two-cell characters leave next cell empty; no misalignment | No misalignment across cases |
| Attribute correctness | Unit | FG/BG and bold applied per mapping; no bleed to neighbors | Correct colors/attributes |
| Highlight layering | Visual | Selection/path drawn above base with consistent z-order | Deterministic layering |
| Dirty-region redraw | Visual + Perf | Only changed cells repainted; minimal CPU time | < 1 ms avg per frame |
| Input routing | Integration | Overlay consumes events when focused; passes otherwise | No contention or lost events |

References: cursesport buffer and refresh; input routing; game event subscriptions.[^4][^5][^3]

## Inventory Panel: Clickable Item Display with Drag-and-Drop

The Inventory Panel presents item stacks from the player’s inventory and optionally from the ground or linked containers. It uses uilist for navigation and selection, augmented by a details pane for examination. The panel supports search/filter, sort options, and contextual actions (use, drop, examine). Drag-and-drop enables moving items between inventory slots, dropping to ground, or stacking into other containers. Feedback is provided via highlights and messages back to the Message Log to confirm actions.

### Layout and Panel Spec

The panel divides into three regions: a scrollable item list (uilist) on the left, a details pane on the right, and a filter input field at the top. The list shows each item’s name, quantity, and a compact status icon (e.g., durability, ammo type). The details pane shows extended description, stats, and available actions. The filter field supports incremental search and quick key toggles (e.g., filter by category).

Table 6. Inventory layout matrix

| Region | Contents | Notes |
|---|---|---|
| Filter bar | Search input (string_input_popup), quick-filter toggles | Anchored; persists across sessions |
| Item list | uilist entries with name, quantity, status icon | Supports scroll, selection, and hotkeys |
| Details pane | Item description, stats, actions | Contextual; updates on selection |
| Footer hints | Action hints (Drop, Use, Examine) | Hotkey mapping visible |

This layout reuse leverages established UI components and the windowing model, reducing custom code and ensuring consistency.[^6]

### Data Bindings

Bindings are read-only with respect to gameplay systems. The panel reads item stacks from the character’s inventory and ground items within a sensible radius, updates on change events (inventory mutations, equipment changes), and uses lazy refresh during hover to avoid expensive repaint cycles. Equipment slots and容器 are treated as separate logical groups, with list sections indicating container provenance.

Table 7. Data binding map: Inventory Panel

| Field | Source | Update Trigger | Read/Write |
|---|---|---|---|
| Item name/quantity | character inventory | On inventory change; periodic lazy hover | Read |
| Item details | item structures via character | On selection | Read |
| Equipment slots | character equipment | On equip/unequip; tick | Read |
| Ground items | map/items near player | On drop/pickup; movement | Read |
| Message confirmations | message subsystem | On action performed | Read (for display) |

The bindings emphasize const-safe accessors and subscriptions to change events, avoiding write-through coupling.[^3]

### Drag-and-Drop Model

Drag sources and targets include inventory slots, equipment slots, ground position (as a logical target), and other containers. The panel supports clone-on-drag for stack splitting and move semantics for non-stackable items. Validation prevents invalid drops (e.g., incompatible items to equipment slots). Visual feedback includes a drag ghost and highlight on the target. After a successful drop, the panel triggers a message to the log and schedules a list refresh.

Table 8. Drag-and-drop state machine

| State | Description | Events | Transition |
|---|---|---|---|
| Idle | No drag in progress | Drag start (mouse or keyboard) | → Dragging |
| Dragging | Ghost item follows cursor | Drag move; target hover highlight | → Dropping (on mouse release) |
| Dropping | Validate target and action | Drop accept/reject | → Idle (on completion) |
| Error feedback | Invalid target | Drop reject | → Idle; show message |

This model is intentionally simple and deterministic to minimize edge cases and to integrate cleanly with the input system’s focus and event consumption rules.[^5]

### Performance and Testing

The panel must handle large inventories smoothly. Virtualization renders only visible rows; sorting and filtering avoid O(n²) behavior. The update strategy is event-driven with periodic lazy refresh when hovered.

Table 9. Performance budget for Inventory Panel

| Operation | Target Metric | Acceptable Threshold | Notes |
|---|---|---|---|
| List render (500 items) | < 1 ms | < 2 ms | Virtualize visible rows |
| Filter (incremental) | < 10 ms | < 20 ms | Debounce input; cache results |
| Drag redraw | < 0.5 ms | < 1 ms | Ghost only; avoid full repaint |
| Message confirm | < 0.2 ms | < 0.5 ms | Append-only log update |

Table 10. Acceptance criteria for Inventory Panel

| Criterion | Test Type | Expected Outcome | Pass/Fail |
|---|---|---|---|
| Correct item binding | Unit | Names/quantities match game state | Accurate binding |
| Drag validity | Integration | Invalid drops rejected; valid accepted | Deterministic validation |
| Scroll performance | Perf | Smooth scroll; no stutter at 500 items | Smooth within thresholds |
| Message integration | Integration | Confirmations appear in log | No dropped confirmations |
| Filter correctness | Unit | Matches items by query and category | Correct filtering |

References: UI components and panel conventions; event bus and state changes.[^6][^3]

## Character Status Bar: Health, Stats, and Condition Visualization

The Character Status Bar provides a compact, glanceable view of the player’s vital signs, core stats, and active conditions. It displays health with thresholds (critical/low/normal), stat values with deltas, and a simple icon set for status effects. It is designed for low visual noise yet sufficient detail to inform decisions. The bar supports a details toggle for a modal or expanded view.

### Layout and Visual Spec

The bar is a single line (or two lines on small screens) divided into segments: health, stats, and conditions. Health includes a progress bar and numeric percentage; stats show primary attributes with arrows indicating recent deltas; conditions list active effects with time remaining. Colors encode severity: green for stable, yellow for caution, red for critical. On high-DPI or tiles builds, icons complement text; in ASCII builds, compact labels and symbols maintain legibility.

Table 11. Status mapping: condition to icon/color/attribute

| Condition | Icon (tiles) | ASCII Symbol | Color | Attribute | Tooltip |
|---|---|---|---|---|---|
| Hungry | Utensils | '=' | Yellow | None | “Hunger rising” |
| Thirsty | Droplet | '~' | Cyan | None | “Dehydration risk” |
| Pain | Skull | '!' | Red | Bold | “High pain level” |
| Bleeding | Drop | '%' | Red | None | “Wound bleeding” |
| Stunned | Zigzag | 'Z' | Magenta | None | “Balance impaired” |
| Infected | Cross | '+' | Red | None | “Infection present” |
| Encumbrance | Weight | 'kg' | Yellow | None | “Movement penalty” |

This mapping balances legibility across rendering backends and provides a stable reference for the UI layer.[^6]

### Data Bindings and Update Triggers

Bindings read player health, stamina, and stats; status effects are bound via character state. Updates occur on change events (e.g., damage, recovery) and periodically for timers. To prevent desync, the bar uses subscriptions to the event bus and invalidation hooks for screen resize or theme changes.

Table 12. Character Status binding map

| Field | Source | Update Strategy | Read/Write |
|---|---|---|---|
| Health (current/max) | player | On damage/heal; on tick | Read |
| Stamina | player | On exertion; on rest | Read |
| Core stats (STR, DEX, etc.) | player | On change; per tick | Read |
| Status effects | character effects | On add/remove; per tick | Read |
| Condition timers | character effects | Periodic (throttled) | Read |

### Animation and Thresholds

Threshold changes use color transitions and brief indicators (arrows, blinks) that are throttled to avoid visual strobe. Blink cadence is limited to the same 200–250 ms used elsewhere, keeping the interface stable and unobtrusive.

### Testing and Acceptance

Unit tests verify mapping correctness; integration tests ensure timely updates on change events; visual regression tests guard against layout or palette drift.

Table 13. Acceptance criteria for Character Status Bar

| Criterion | Test Type | Expected Outcome | Pass/Fail |
|---|---|---|---|
| Threshold colors | Unit | Correct color per health ranges | Accurate |
| Timer accuracy | Integration | Status timers update within tolerance | ±1 turn |
| Blink throttling | Unit | Blinks cadence within limits | No strobe |
| Resize behavior | Visual | Layout adapts; text clipped minimally | Clean resize |
| Event latency | Integration | UI updates promptly on changes | < 1 frame latency |

References: UI manager redraw and adaptor hooks; bindings to character state; event bus usage.[^6][^3]

## Message Log Panel: Scrollable Chat-like Interface with Timestamps

The Message Log Panel presents timestamped messages with color-coded types (gameplay, system, combat, NPC). It behaves like a chat interface: the latest message appears at the bottom; auto-scroll can be toggled; users can scroll up to review history, with filter controls to reduce noise. It aligns with the game’s message subsystem and preserves log integrity across sessions.

### Layout and Interaction

The panel comprises a list view, a timestamp column, type tags, and filter controls. Keyboard navigation includes up/down scroll, page up/down, and a “jump to bottom” action. Mouse wheel and scrollbar interactions are supported. Filter toggles allow the player to show or hide message categories without altering the underlying log.

Table 14. Message types and color mapping

| Type | Color | Example Payload | Filter Default |
|---|---|---|---|
| Gameplay | White | “You pick up the item.” | On |
| System | Cyan | “Auto-save complete.” | On |
| Combat | Red | “You hit the zombie for 5 damage.” | On |
| NPC | Green | “Merchant says: ‘Prices just improved.’” | On |
| Debug | Yellow | “Path recalculated.” | Off (in release builds) |

The mapping and defaults can be themed; the key is stability and readability across builds.[^6]

### Data Binding and Update Strategy

Messages are appended to the log store with a timestamp and type. The panel uses windowed virtualization to display only visible rows efficiently. When a new message arrives, the panel checks auto-scroll policy; if enabled, it jumps to the bottom. If the player scrolls up, auto-scroll is temporarily disabled.

Table 15. Message log binding map

| Field | Source | Update Trigger | Notes |
|---|---|---|---|
| Message text | message store | On new message | Append-only |
| Timestamp | message store | On new message | Stored separately |
| Type | message store | On new message | Fixed at creation |
| Filter flags | UI state | On toggle | Non-destructive |

### Performance and Testing

The log uses ring buffers or capped collections to prevent unbounded growth. Virtualization reduces draw cost; appends are O(1) amortized. Tests verify scroll position, filter correctness, and timestamps.

Table 16. Acceptance criteria for Message Log Panel

| Criterion | Test Type | Expected Outcome | Pass/Fail |
|---|---|---|---|
| Append performance | Perf | 1000 messages: no stutter | Smooth append |
| Auto-scroll toggle | Integration | Scrolls to bottom when enabled | Correct behavior |
| Filter correctness | Unit | Shows/hides types per setting | Accurate filtering |
| Timestamp consistency | Unit | Timestamps monotonic and correct | No drift |
| Virtualization | Visual | Only visible rows rendered | No off-screen draws |

References: UI list rendering and scrolling patterns; event distribution for messages.[^6][^3]

## Component Interaction: Cross-Component Communication and World Binding

The MVP GUI’s four components share a consistent interaction model. Input events are routed through the active input_context; state changes are published via the event bus or explicit subscriptions; UI state is anchored by ui_manager/ui_adaptor; rendering uses buffered window updates and final compositing in sdltiles when applicable. Focus and modal behavior are deterministic: when a panel like Inventory opens, it receives focus; when closed, focus is restored to the prior context.

Message log integration is canonical for user feedback: Inventory actions and status changes publish confirmations and alerts to the log. Status bar changes may also trigger log entries for significant transitions (e.g., critical health). Cross-component constraints ensure that when Inventory is active, movement keys are not hijacked by the Visual Map Overlay, and vice versa.

Table 17. Event bus topics and subscribers

| Topic | Publisher | Subscribers | Update Strategy |
|---|---|---|---|
| inventory_changed | character/map | Inventory Panel, Message Log | Event-driven; refresh and confirm |
| equipment_changed | character | Inventory Panel, Status Bar | Event-driven; refresh |
| health_changed | player | Status Bar, Message Log | Event-driven; threshold check |
| messagePublished | gameplay/systems | Message Log Panel | Append-only; auto-scroll policy |
| map_visibility_changed | map/weather | Visual Map Overlay | Dirty-region redraw |
| turn_tick | calendar/timed events | Status Bar, Overlay highlights | Periodic throttled update |

### Data Flow per Frame

The end-to-end frame flow is:

1. Poll input via input_manager/handle_action and the active input_context to get an action ID.
2. Dispatch the action to gameplay or UI, mutating state (e.g., move, drop, examine).
3. Publish state changes to interested components via the event bus; schedule redraws.
4. UI components paint to their catacurses::window surfaces; ui_manager orchestrates redraws.
5. Cursesport buffers changes (wnoutrefresh); sdltiles composes the final frame (doupdate).
6. On certain turns/time thresholds, autosave runs without involvement of UI components.[^3][^5][^6][^4]

This pipeline is stable and traceable across the codebase, and it allows the MVP UI to integrate without structural changes.

## Layout Specifications and Responsive Behavior

Responsive layout is defined for the ASCII/curses build using a character grid. Each component has min/max sizes and a resize strategy. The Visual Map Overlay can be the full scene or a panel; the Inventory Panel and Message Log Panel prefer sidebars; the Status Bar is a top or bottom strip. On resize events (terminal size change or theme switch), ui_adaptor triggers layout recomputation; fonts and DPI are handled through the tiles renderer’s font utilities for hybrid builds.

Table 18. Panel layout by resolution class

| Resolution Class | Map | Inventory | Status Bar | Message Log | Notes |
|---|---|---|---|---|---|
| Small (e.g., 80×25) | Full width, reduced height | Right sidebar, 30–40% width | Top strip (1–2 lines) | Bottom strip, 20% height | Prioritize core info |
| Medium (e.g., 120×40) | Full width, 60–70% height | Right sidebar, 25–35% width | Top strip (1–2 lines) | Bottom strip, 25% height | Balanced layout |
| Large (e.g., 160×60) | Full width, 50–60% height | Right sidebar, 20–30% width | Top strip (1–2 lines) | Bottom strip, 20% height | More details visible |

On resize, components recompute scroll ranges, visible rows, and highlight placements. In tiles builds with HiDPI, the font scaling ensures legibility without altering logic.[^6][^4]

## Data Binding Strategies and Subscription Model

Data bindings are read-only with respect to gameplay state. Components subscribe to change events to receive timely updates, and they maintain a per-frame poll fallback for time-based UI elements. The update strategy balances responsiveness with performance by preferring event-driven updates and throttling periodic refreshes.

Table 19. Unified binding map across components

| Component Field | Source Module | Read/Write | Update Trigger | Fallback |
|---|---|---|---|---|
| Map tile (overlay) | map/visibility | Read | On MOVE/LOOK; visibility change | Per-frame poll (throttled) |
| Inventory entries | character/map items | Read | On inventory change | Lazy hover refresh |
| Equipment slots | character | Read | On equip/unequip | Periodic tick |
| Health/stat | player | Read | On change; tick | Per-frame poll (throttled) |
| Status effects | character | Read | On add/remove; tick | Per-frame poll (throttled) |
| Messages | message store | Read | On new message | Append check per frame |

The model leverages existing game state access and avoids new write paths to gameplay logic, preserving encapsulation.[^3][^6]

## Input Handling, Focus, and Hotkey Strategy

Hotkeys are registered per component via input_context, and global gameplay bindings are preserved. Conflict resolution follows project conventions: overlay hotkeys are configurable; conflicts are detected and reported by the binding editor. Focus rules specify consumption and pass-through. Mouse capture and gamepad support follow existing code paths, ensuring consistent behavior across devices.

Table 20. Hotkey registry (initial)

| Action | Context | Key(s) (configurable) | Description | Conflict Policy |
|---|---|---|---|---|
| TOGGLE_OVERLAY | Game/Overlay | O (example) | Show/hide Visual Map Overlay | Avoid global conflicts; validate in options |
| LOOK | Overlay | L (example) | Enter look mode | Consumed when overlay active |
| SELECT_ITEM | Inventory | Enter/Space | Examine/use selected item | Consumed in Inventory |
| DROP | Inventory | D | Drop selected item | Consumed in Inventory |
| FILTER | Inventory | F | Focus filter field | Consumed in Inventory |
| TOGGLE_STATUS_DETAILS | Status Bar | H (example) | Expand/collapse details | Avoid global conflicts |
| TOGGLE_AUTO_SCROLL | Message Log | A | Toggle auto-scroll | Consumed in Message Log |
| SCROLL_UP/DOWN | Inventory/Message | PageUp/PageDown | Scroll list | Consumed in focused panel |

The registry is a starting point; keys are bound via options and validated to avoid collisions. Mouse and gamepad events are routed as per the existing input pipeline.[^5][^1]

## Update Mechanisms, Performance, and Rendering Coordination

Update cadence is designed to be responsive yet conservative. Per-frame updates occur for the active panel and time-based UI changes; passive panels update on change events. Buffered window refresh (wnoutrefresh) and sdltiles compositing ensure efficient redraws; UI drawing pauses when minimized to avoid wasted work. Dirty-region repainting minimizes work on unchanged content.

Table 21. Update cadence matrix

| Component | Per-frame | Throttled (200–250 ms) | On-change | Notes |
|---|---|---|---|---|
| Visual Map Overlay | Active view | Highlights and path preview | Map/visibility change | Dirty-region optimization |
| Inventory Panel | Active panel | Hover details | Inventory/equipment change | Virtualization |
| Character Status Bar | Passive | Blink/threshold transitions | Health/stat/effect change | Timers throttled |
| Message Log Panel | Append check | Auto-scroll recompute | New messages | Virtualized list |

Table 22. Performance budget and thresholds

| Operation | Budget | Threshold | Test Reference |
|---|---|---|---|
| Overlay draw (1k tiles) | < 1 ms | < 2 ms | Visual + Perf |
| Inventory render (500 items) | < 1 ms | < 2 ms | Perf |
| Status bar update | < 0.2 ms | < 0.5 ms | Unit + Perf |
| Message append + draw | < 0.5 ms | < 1 ms | Perf |

These budgets are achievable with buffered updates and careful event-driven design, and they align with the documented rendering and UI coordination layers.[^4][^6][^24]

## Testing and Validation Plan

Unit, integration, and end-to-end tests validate correctness, focus routing, and performance. Compatibility tests run across ASCII/curses and tiles configurations. Acceptance criteria per component are enforced through CI with visual regression for UI layout and palette, and input event capture to ensure no contention or lost events.

Table 23. Acceptance criteria matrix (consolidated)

| Component | Test Type | Expected Outcome | Pass/Fail |
|---|---|---|---|
| Visual Map Overlay | Visual + Perf | Correct layering and highlights; no artifacts; perf within budget | Meets targets |
| Inventory Panel | Integration | Correct bindings; drag-and-drop validity; smooth scroll | No contention |
| Character Status Bar | Unit + Integration | Threshold colors and timers correct; timely updates | Accurate |
| Message Log Panel | Unit + Integration | Append performance; filters; timestamps | Smooth, correct |
| Cross-component | Integration | Focus routing deterministic; no event loss | Deterministic focus |
| Preferences (if any) | Unit | JSON isolation; rollback on error | No save impact |

References: frame lifecycle; input handling; UI manager redraw; windowed rendering.[^2][^5][^6][^4]

## Extensibility Path: Optional Retained-Mode GUI (Future)

If retained-mode requirements emerge (rich theming, complex layouts, docking), the MVP’s decoupled UI surfaces allow adoption of a retained-mode SDL2-native GUI. A practical option is libsdl2gui, which defines layouts in XML, renders via SDL2Renderer, and offers a broad widget set. Event bridging remains straightforward: input_context and ui_manager can interface with a retained-mode widget tree. Preferences, including themes and panel positioning, would be stored in separate JSON and validated on load.

Table 24. Retained-mode component mapping (future)

| MVP Component | libsdl2gui Element | Notes |
|---|---|---|
| Inventory Panel | List/Table + Details Pane | XML layout for list and detail; dynamic updates via API |
| Character Status Bar | Progress bars + Labels + Icons | Bind to player state; throttled updates |
| Message Log Panel | List with custom item model | Timestamp column; virtualization |
| Visual Map Overlay | Custom widget or hybrid | Retained shell; ASCII cells via custom draw |

This path leverages the same SDL2Renderer, avoids a second graphics API, and aligns with Cataclysm-BN’s existing pipeline and conventions.[^8][^1]

## Implementation Roadmap, Deliverables, and Milestones

The roadmap is phased to mitigate risk and ensure incremental validation. Each phase has clear outputs and exit criteria.

Table 25. Phases and deliverables (abbreviated)

| Phase | Tasks | Outputs | Entry Criteria | Exit Criteria |
|---|---|---|---|---|
| 0 — Spike | Minimal overlay list; input routing test; ASCII buffer draw | Prototype components; event hooks | Codebase baseline | Visual + input tests pass |
| 1 — Foundations | Component skeletons; ui_adaptor hooks; event subscriptions | Core UI surfaces; data bindings | Phase 0 | Stable bindings; no desync |
| 2 — Map Overlay | ASCII overlay with highlights; cursor; dirty-region | Overlay draw and input | Phase 1 | Layering + perf tests pass |
| 3 — Inventory | uilist; details pane; drag-and-drop | Inventory Panel | Phase 1 | Valid drag/drop; smooth scroll |
| 4 — Status Bar | Health/stat/conditions; thresholds | Status Bar | Phase 1 | Accurate thresholds; timers |
| 5 — Message Log | Timestamped log; filters; virtualization | Message Log Panel | Phase 1 | Append/perf targets met |
| 6 — Cross-component | Focus, hotkeys, event routing; testing | Integrated MVP | Phase 2–5 | Acceptance criteria met |

Entry and exit criteria focus on correctness, performance, and integration stability, consistent with Cataclysm-BN’s main loop and UI management practices.[^3][^6]

## Appendices: API and Data Structure Mapping

The appendices provide a quick reference for key BN classes and modules relevant to the MVP UI, and they summarize JSON preferences isolation. This helps developers keep bindings const-safe and avoid coupling to gameplay state.

Table 26. Key classes and roles (quick reference)

| Class/Struct | File | Responsibility | Notable Members/Methods |
|---|---|---|---|
| game | game.cpp | Central game state and loop | do_turn; map; overmap_buffer; calendar; weather; event_bus; uquit; safe_mode; autosave heuristics[^3] |
| ui_manager | ui.cpp | Global UI redraw | Redraw orchestration and callback scheduling[^6] |
| ui_adaptor | ui.cpp | Redraw/resize hooks | on_redraw; on_screen_resize; invalidation[^6] |
| input_manager | input.cpp | Global input management | Initialize keycodes; load/save bindings; timeouts[^5] |
| input_context | input.cpp | Mode-specific input | register_action; handle_input; get_desc; binding editor[^5] |
| WINDOW | cursesport.cpp | Curses-like window buffer | width, height, cursor, FG/BG, lines[^4] |
| curseline/cursecell | cursesport.cpp | Text storage | cursecell: FG/BG and UTF‑8 char; wide-char handling[^4] |
| uilist | ui.cpp | Menu UI | entries; filter; hotkeys; callbacks; scrollbar[^6] |
| string_input_popup | ui.cpp | Text input | positioning; editing; confirm/cancel[^6] |
| overmap, overmap_buffer | savegame.cpp | Overworld data | RLE terrain; view layers; notes/extras[^7] |
| map | game.cpp | Local map data | visibility, items, vehicles, terrain[^3] |
| calendar | game.cpp | In-game time | turn; start times; seasons[^3] |
| weather_manager | game.cpp | Weather state | Conditions and transitions[^3] |
| event_bus | game.cpp | Event distribution | Publish/subscribe for inter-component events[^3] |

Table 27. Preferences JSON keys (if any GUI preferences introduced)

| Key | Type | Default | Location | Validation Rule |
|---|---|---|---|---|
| overlay.enabled | bool | false | GUI preferences JSON | Must be boolean; revert on invalid |
| overlay.theme | string | “default” | GUI preferences JSON | Known theme only; fallback otherwise |
| overlay.position | string | “bottom-right” | GUI preferences JSON | Allowed positions only; fallback |
| overlay.opacity | number | 0.8 | GUI preferences JSON | Clamp [0.0, 1.0] |
| overlay.hotkeys | array | [] | GUI preferences JSON | Validate against options bindings |

Preferences are isolated from world saves to avoid impacting save integrity. Backup/rollback strategies are applied: write to a temporary file, validate, then replace the active preferences file.[^7]

## Information Gaps and Assumptions

The design acknowledges specific gaps that must be addressed during implementation or early phases:

- Authoritative list and current layout of panels managed by panel_manager (for consistent sidebar integration).
- Exact draw order and frame lifecycle hooks for final-frame compositing in sdltiles and the precise sdltiles compositing sequence.
- Canonical input interception pattern beyond basic event polling, including focus and priority rules.
- Current UI modal stacking and focus management details within ui_manager for deterministic state transitions.
- JSON preferences file naming conventions in user data directories for GUI overlay settings.
- Platform-specific input quirks across SDL2 targets (e.g., high-DPI handling, mouse capture).
- Performance budgets and telemetry available to measure per-frame cost of overlays and UI updates.
- Licensing or policy constraints on adding new dependencies for a retained-mode GUI.

These gaps are non-blocking but require clarification to ensure precise integration and to avoid regressions.[^1][^2][^6][^5][^3]

## References

[^1]: Cataclysm: Bright Nights — GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN  
[^2]: Cataclysm-BN src/main.cpp (entry points, initialization, loop scaffolding). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/main.cpp  
[^3]: Cataclysm-BN src/game.cpp (game state, UI integration, main loop). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/game.cpp  
[^4]: Cataclysm-BN src/cursesport.cpp (pseudo-curses implementation and ASCII rendering). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/cursesport.cpp  
[^5]: Cataclysm-BN src/input.cpp (input manager, contexts, keybindings). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/input.cpp  
[^6]: Cataclysm-BN src/ui.cpp (uilist, UI manager, adaptor, windowing). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/ui.cpp  
[^7]: Cataclysm-BN src/savegame.cpp (JSON serialization, versioning, RLE, legacy support). https://github.com/cataclysmbnteam/Cataclysm-BN/blob/main/src/savegame.cpp  
[^8]: libsdl2gui GitHub Repository. https://github.com/adamajammary/libsdl2gui  
[^9]: Cataclysm: Bright Nights — Official Documentation. https://docs.cataclysmbn.org  
[^24]: Martin Fieber, “GUI Development with C++, SDL2, and Dear ImGui.” https://martin-fieber.de/blog/gui-development-with-cpp-sdl2-and-dear-imgui/