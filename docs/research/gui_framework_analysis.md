# GUI Frameworks for Cataclysm-BN Integration: SDL2, Raylib, CEGUI, and Immediate-Mode Options

## Executive Summary

Cataclysm: Bright Nights (Cataclysm-BN) is an open-source, SDL2-based roguelike with a C++ codebase. Its graphics, font rendering, and audio all rely on standard Simple DirectMedia Layer 2 (SDL2) modules (image, ttf, mixer), and the project builds via CMake and common package managers (e.g., vcpkg, MSYS2) on mainstream desktop platforms[^1]. Against this foundation, the question is how best to introduce a modern, performant, and maintainable graphical user interface (GUI) layer that can serve overlays, in-game panels, and developer tools without compromising frame time, memory budgets, or build simplicity.

This report examines four integration patterns:

- SDL2-native GUI (retained-mode): event-driven, SDL2-centric libraries that draw via SDL2Renderer and can define layouts via XML.
- Raylib-based GUI: either raygui (Raylib’s immediate-mode GUI) or rlImGui (Dear ImGui bindings for Raylib), useful when Raylib already drives the render loop.
- CEGUI (Crazy Eddie’s GUI): a mature retained-mode, component-oriented GUI with a large widget set and strong theming.
- Immediate-mode GUI alternatives: Dear ImGui (C++) and Nuklear (ANSI C), both well-suited to overlays and tools.

Top-line recommendation for Cataclysm-BN:

- Preferred: SDL2-native retained-mode GUI (e.g., libsdl2gui) for in-game UI overlays and HUD-like panels. This path minimizes new rendering dependencies, keeps the focus on SDL2’s proven renderer, and tends to simplify event integration. 
- Complementary: Dear ImGui for debug overlays, tools, and rapid iteration. It is widely used, easy to embed, and demonstrably efficient in real-time contexts[^2][^6][^9][^13][^14][^15].
- Optional: Raylib-based approach (raygui or rlImGui) selectively, if/when the project wants to leverage Raylib’s multimedia convenience or a dedicated tools app built alongside the game[^4][^5].
- Deferred: CEGUI for large, complex in-game UIs or configuration-driven screens, but only if a retained-mode architecture and its workflow are a clear fit for the team’s goals[^3].

Key trade-offs at a glance:

- Retained-mode (SDL2-native, CEGUI) favors stateful, componentized UI with predictable updates and theming. 
- Immediate-mode (Dear ImGui, Nuklear) favors tools, overlays, and rapid iteration with small integration footprints and high runtime efficiency[^6][^13][^15].
- Raylib-based options provide convenience and speed for small tools or prototypes, but introduce a secondary framework if Cataclysm-BN’s core remains SDL2-only[^4][^5][^16].

Evidence note: we draw on the project’s repository to anchor Cataclysm-BN’s architecture and build practices[^1], an integration guide demonstrating Dear ImGui with SDL2’s renderer backends[^2], primary repositories for CEGUI[^3], Raylib/raygui[^4][^5], and Nuklear[^7], and third-party analyses comparing immediate-mode and retained-mode GUI efficiency[^6].


## Cataclysm-BN Baseline and Requirements

Cataclysm-BN inherits a C++ codebase and an SDL2-based multimedia stack. The graphical build uses SDL2 for windowing, images, TrueType fonts, and audio. The project is cross-platform, with builds orchestrated through CMake and common package managers. Community packaging (e.g., Debian) further confirms the SDL2 dependency footprint for the tiles-based, graphical interface[^1][^10].

GUI needs for a roguelike with overlays, inventory screens, and developer tools are typical:

- Low-latency input and crisp text; 
- Predictable updates and clean re-draw behavior;
- Thematic and font handling consistent with the game’s art direction;
- A build system that coexists with existing CMake/vcpkg/MSYS2 pipelines.

Non-functional priorities include:

- Keeping additional dependencies modest;
- Maintaining short build times and small binaries;
- Ensuring that overlays and tools do not disturb frame pacing;
- Preserving maintainability and contributor accessibility.

These constraints shape the evaluation criteria used in the rest of this report: integration complexity, compatibility, performance, memory footprint, cross-platform coverage, theming/styling flexibility, and community maturity.

To ground the SDL2 baseline, Table 1 summarizes the primary modules and their roles.

Table 1 — Cataclysm-BN SDL2 stack and roles

| Component         | Role in Cataclysm-BN                         | Notes                                               |
|-------------------|----------------------------------------------|-----------------------------------------------------|
| SDL2              | Core windowing, events, main loop            | Cross-platform foundation for graphics/input[^1]    |
| SDL2_image        | Image loading for textures/assets            | Used by the tiles-based graphical interface[^1]     |
| SDL2_ttf          | TrueType font rendering                      | Critical for in-game text and UI[^1]                |
| SDL2_mixer        | Audio playback                                | Integrated with the game’s audio subsystem[^1]      |

The distribution metadata for Cataclysm: Dark Days Ahead’s SDL2 build provides a useful proxy for a broadly used SDL2-based C++ roguelike’s packaging and dependency footprint[^10]. The similarity of technical needs across forks and variants supports the relevance of the baseline and the GUI requirements identified above.


## Evaluation Framework and Methodology

We assess candidate GUI libraries across seven dimensions:

- Integration complexity: how easily they slot into the existing SDL2-based loop and CMake build;
- Compatibility: how well they respect the project’s current dependencies and rendering path;
- Performance overhead: qualitative expectations for CPU/GPU draw and frame time;
- Memory footprint: expected heap and VRAM usage patterns, especially for overlays;
- Cross-platform coverage: Windows, Linux, macOS, and mobile if relevant;
- Theming/styling and text handling: ability to meet visual requirements for a roguelike;
- Maturity: documentation quality, community support, and maintenance cadence.

Methodologically, we emphasize:

- Primary repositories and documentation (CEGUI, Dear ImGui, Raylib/raygui, Nuklear, libsdl2gui)[^1][^2][^3][^4][^5][^7][^8];
- An SDL2+Dear ImGui integration guide that demonstrates backends, event handling, and high-DPI support[^2];
- A community-maintained list of SDL GUI toolkits for discoverability and ecosystem context[^11];
- An empirical analysis contrasting immediate-mode and retained-mode GUI efficiency[^6];
- A talk clarifying that Dear ImGui does not perform immediate-mode rendering but produces optimized vertex buffers, a detail with practical implications for batching and multi-backend rendering[^9].

Limitations: We did not run bespoke benchmarks of memory and frame time for each library within the Cataclysm-BN renderer. Quantitative comparisons specific to Cataclysm-BN’s pipeline remain an information gap (see “Information gaps” near the end).


## Option A — SDL2-Native GUI Libraries (Retained Mode)

SDL2-native GUI libraries draw via SDL2’s renderer, reuse Cataclysm-BN’s existing event loop, and avoid introducing a second graphics API. Two notable options are libsdl2gui and SDL_gui. They align with the project’s dependency model and CMake orientation, easing integration and packaging.

### A1. libsdl2gui (XML-driven retained-mode UI, SDL2 Renderer)

libsdl2gui is a modern C++20 library that defines UI layouts in XML and drives rendering through SDL2Renderer. It offers a broad component set and a style/theme mechanism, plus utilities for file dialogs and image metadata. Integration revolves around initializing the UI, running the event-driven frame loop, and optionally intervening for dynamic elements[^8].

Core integration steps:

- Initialization: LSG_Start creates the window, loads the XML layout, and returns an SDL_Renderer for the frame.
- Run/Present: LSG_Run clears, renders, and processes UI events; LSG_Present swaps buffers. 
- Dynamic updates: add/remove items, navigate lists/tables, and update control attributes via a small API surface (e.g., LSG_SetText, LSG_SetEnabled, LSG_ScrollVertical).
- Platform dialogs and utilities: open/save dialogs and image helpers are included, which may reduce custom platform code.

Table 2 maps the libsdl2gui API to Cataclysm-BN’s needs.

Table 2 — libsdl2gui API mapping to Cataclysm-BN needs

| Function              | Purpose                                           | Relevance to Cataclysm-BN                                     |
|-----------------------|---------------------------------------------------|----------------------------------------------------------------|
| LSG_Start             | Initialize UI, create window/SDL_Renderer         | Minimizes new renderer deps; fits existing SDL2 loop[^8]       |
| LSG_Run               | Process events, render UI                         | Event-driven alignment with existing input handling[^8]        |
| LSG_Present           | Swap buffers                                      | Fits standard present/pacing logic[^8]                         |
| LSG_Layout            | Recalculate/redraw layout                         | Useful on resolution/DPI changes[^8]                           |
| LSG_Add/Remove*       | Dynamic element add/remove                        | Supports dynamic inventories, tables, lists[^8]                |
| LSG_SetText/Value     | Update labels, inputs, sliders                    | Tight binding to game state without separate message bus[^8]   |
| LSG_SetEnabled/Visible| State control                                     | Conditional panels (e.g., context menus)[^8]                   |
| LSG_Set*Size/Position | Sizing/anchoring                                  | Responsive layouts for overlays[^8]                            |
| LSG_Sort*             | Sorting lists/tables                              | Inventory and crafting sort UI[^8]                             |
| Dialogs/Save/Open     | Platform dialogs                                  | Reduces bespoke code for file IO[^8]                           |
| Theme files           | Color themes                                      | Art direction alignment and mod support[^8]                    |

Pros:
- Native fit to SDL2; no extra rendering layer required.
- XML-defined layouts encourage designer-friendly workflows.
- Rich widget set, platform dialogs, and utilities reduce bespoke code.

Cons:
- C++20 requirement may constrain compiler support on older platforms[^8].
- Active ecosystem but smaller than Dear ImGui or CEGUI.
- Some alignment work may be needed to harmonize with Cataclysm-BN’s event model and UI style guide.

Suitability: high for retained-mode in-game UIs/HUDs, tool windows, and configuration dialogs where XML layouts and a clear event flow are desired.

### A2. SDL_gui (SDL2+SDL_image+SDL_ttf, HarfBuzz, icons)

SDL_gui is an SDL2-centric library that builds on SDL2_image and SDL2_ttf, adds HarfBuzz for glyph shaping, supports Unicode, and includes icon fonts (e.g., FontAwesome). This positions it as a text-forward, SDL-native UI with internationalization and iconography built-in[^12]. It can complement or substitute libsdl2gui when:

- Complex, internationalized text and icons are prominent;
- Developers prefer a library that stays entirely within the SDL ecosystem.

Table 3 provides an overview.

Table 3 — SDL_gui components and features

| Area         | Highlights                                                                 |
|--------------|-----------------------------------------------------------------------------|
| Text         | SDL2_ttf for fonts; HarfBuzz for shaping; Unicode support                   |
| Icons        | FontAwesome integration for glyph-based iconography                         |
| Graphics     | SDL2_image and SDL2_ttf for assets and text rendering                       |
| Cross-platform| Built on SDL2 modules                                                       |
| Use cases    | Text-heavy interfaces, mod UIs requiring icons, multilingual screens        |

Suitability: strong for text-centric interfaces and iconography; comparable to libsdl2gui for SDL2-native retained-mode needs, with a different emphasis (text shaping and icons) and a distinct API surface[^12].


## Option B — Raylib-Based GUI (raygui or rlImGui)

Raylib is a lightweight multimedia library focused on games. Cataclysm-BN does not currently depend on Raylib; therefore, adopting a Raylib-based GUI either means layering it under a tools-only app or temporarily driving the game’s render loop with Raylib. Two paths exist:

- raygui: immediate-mode GUI for Raylib, with a distinctive visual style and a large, editable control set.
- rlImGui: Dear ImGui bindings for Raylib, letting developers use ImGui’s widgets inside a Raylib-driven loop[^4][^5][^16].

Practical considerations:

- If Cataclysm-BN remains SDL2-centric, introducing Raylib solely for GUI may add unnecessary complexity.
- If the team expects to ship a tools app (e.g., a map editor) alongside the game, Raylib can offer a very productive, cross-platform baseline for that tool, with either raygui or rlImGui providing the UI layer[^16].

Table 4 contrasts the two options.

Table 4 — raygui vs rlImGui under Raylib

| Dimension                | raygui (Raylib immediate-mode GUI)                               | rlImGui (Dear ImGui bindings for Raylib)                            |
|-------------------------|-------------------------------------------------------------------|---------------------------------------------------------------------|
| API paradigm            | Immediate-mode (no retained state)                                | Immediate-mode via Dear ImGui                                       |
| Visual style            | Raylib aesthetic; highly configurable via styles                   | Default ImGui look; skinnable via style system                      |
| Control coverage        | 25+ controls; layout tools; icons                                  | Broad ImGui widget set; tables, docks, plots via ImGui ecosystem    |
| Ecosystem               | Raylib-centric auxiliary module                                    | Bridges ImGui’s large community and add-ons                         |
| Tooling                 | rGuiStyler, rGuiIcons, rGuiLayout                                  | Use ImGui’s style editors and ImPlot, etc., via RL backends         |
| Fit for Cataclysm-BN    | Best if using Raylib for the app; not ideal if staying on SDL2     | Good for a Raylib-based tool app; extra deps if core stays on SDL2  |

### B1. raygui (Immediate-mode GUI for Raylib)

raygui is a single-header, immediate-mode library aligned with Raylib. It provides over 25 controls, a strong styling system (including a style editor), and an icon pack. It is “stand-alone capable” in the sense that it can be adapted beyond Raylib, but its version alignment follows Raylib master, which implies coordination when upgrading either library[^4].

Integration pattern with Raylib:
- BeginDrawing → Gui controls → EndDrawing;
- Use the style system to align visuals with the target game.

Suitability: high for Raylib-based tools and prototypes; consider only if Cataclysm-BN adopts Raylib or a secondary app benefits from Raylib’s speed of iteration.

### B2. rlImGui (Dear ImGui bindings for Raylib)

rlImGui wires Dear ImGui into a Raylib render loop. A common pattern is rlImGuiSetup(true), then sandwich ImGui code between rlImGuiBegin and rlImGuiEnd, all within BeginDrawing/EndDrawing. The integration is straightforward via CMake and small source sets; it is often used when developers want ImGui’s richer widget set and ecosystem within a Raylib context[^5][^16].

Suitability: strong for a Raylib-based tool UI, especially if ImGui’s breadth (tables, docking, plots) is desirable. For Cataclysm-BN’s core, it introduces an extra dependency unless the game also uses Raylib for rendering[^16].


## Option C — CEGUI (Retained Mode, Modular Architecture)

CEGUI is a mature, retained-mode GUI library designed for games and rendering applications. It has a modular architecture, supports a wide set of rendering backends and image/XML parsers, and currently requires only GLM as a mandatory dependency (with many optional integrations). The project maintains stable API/ABI branches and provides a CMake-based build system[^3].

Architecture and integration:

- Rendering: modular; supports multiple backends and engines;
- Dependencies: GLM required; many optional dependencies for codecs and parsers;
- Build: CMake across Windows and *NIX; dynamic linking recommended for compatibility;
- Theming and layout: powerful and flexible retained-mode model suited to complex, long-lived UI screens[^3].

CEGUI’s community-facing materials emphasize performance and maturity. Developers report running hundreds of windows in samples at high frame rates in Release builds, and the library is used in notable commercial titles. However, CEGUI cautions that poor performance often stems from anti-patterns such as reloading layout resources every frame or updating the UI multiple times per frame; performance-sensitive code should be measured in Release mode[^3].

Suitability: strong for complex, retained-mode in-game UIs and screen systems. The modular architecture and optional dependencies are attractive when teams value feature depth and explicit control over build complexity[^3].

Table 5 summarizes modularity and optional integrations.

Table 5 — CEGUI modularity overview

| Area                | Notes                                                                 |
|---------------------|-----------------------------------------------------------------------|
| Mandatory deps      | GLM                                                                     |
| Rendering backends  | Multiple supported; integrates with engines/renderers                  |
| Image loaders       | Multiple codecs supported via optional dependencies                    |
| XML parsers         | Optional, configurable                                                 |
| Linking             | Dynamic linking recommended for compatibility                          |
| Stability           | Stable branches maintain API/ABI compatibility                         |


## Option D — Immediate-Mode GUI Alternatives (Dear ImGui, Nuklear)

Immediate-mode GUIs (IMGUIs) describe UI per frame rather than maintaining a retained widget tree. They are known for easy integration into real-time render loops, small footprints, and fast iteration. Importantly, IMGUIs typically do not perform “immediate-mode rendering”; many, including Dear ImGui, output optimized vertex buffers that can be batched and rendered efficiently by the host renderer[^9][^15].

Immediate vs retained paradigms: a concise comparison appears in Table 6.

Table 6 — Immediate vs retained GUI paradigms

| Aspect                | Immediate-mode (e.g., Dear ImGui, Nuklear)                     | Retained-mode (e.g., CEGUI, SDL2-native retained)              |
|-----------------------|-----------------------------------------------------------------|-----------------------------------------------------------------|
| State management      | Per-frame description; minimal retained state                   | Persistent widget tree/state                                    |
| Integration           | Simple embedding in game loops                                  | Typically requires framework lifecycle integration              |
| Performance profile   | Efficient in practice; batching via vertex buffers[^9][^6]      | Efficient; performance depends on usage and update discipline   |
| Complexity handling   | Excellent for tools/overlays; complex screens require structure | Suited to complex, long-lived UI screens                        |
| Theming               | Supported but may be simpler                                    | Rich theming and layout systems common                          |

### D1. Dear ImGui (C++ IMGUI with rich ecosystem)

Dear ImGui is widely used in games and tools. It offers SDL2 backends that integrate windowing, events, and rendering through SDL2. A practical integration guide shows how to use SDL’s accelerated renderer with vsync, enable High DPI handling, and activate docking/viewports as needed. In a game, developers can pause drawing when minimized and map input succinctly, minimizing unwanted CPU work[^2].

Key traits:
- Mature, with many extensions and third-party packages;
- Supports docking and viewports (optional) for flexible layouts;
- Simple data passing (pointer-based) accelerates iteration;
- Produces optimized vertex buffers, not immediate-mode rendering[^9][^2].

Suitability: ideal for debug overlays, in-game consoles, and tools; suitable for selected in-game panels if a retained-mode model is not mandatory.

### D2. Nuklear (ANSI C, single-header, minimal-state IMGUI)

Nuklear is a single-header ANSI C library with no external dependencies, no default backend, and a focus on portability and minimal state. It outputs drawing commands for primitives, leaving backend integration to the host application. Developers can bake fonts, control memory explicitly, and compile only the modules they need[^7].

Suitability: good for ultra-light overlays, strictly license-friendly codebases, and environments demanding minimal dependencies. Integration is straightforward if Cataclysm-BN provides a small adapter to convert Nuklear’s primitives to SDL2Renderer draw calls[^7].


## Performance and Memory Efficiency

The central performance question for overlays is whether an immediate-mode GUI imposes meaningful overhead compared to retained-mode alternatives. An empirical study measured “power at the wall” across multiple IMGUI and non-IMGUI applications on two laptops. Even under worst-case test conditions (120 Hz, idle optimizations disabled), immediate-mode GUIs showed power draw comparable to mainstream retained-mode applications, indicating that IMGUIs do not inherently penalize battery life or frame time relative to retained-mode UIs[^6].

A frequently misunderstood point is that Dear ImGui does not render via immediate-mode rendering. Instead, it generates optimized vertex buffers that can be batched and rendered by the host backend (e.g., SDL2Renderer), making it compatible with Cataclysm-BN’s existing rendering approach[^9]. This nuance matters: IMGUIs can be both easy to integrate and efficient on the GPU.

Table 7 summarizes the reported power consumption (Watts) for the laptops and software measured.

Table 7 — Power consumption comparison (Watts)

| Software Category          | MacBook Pro (M1 Pro, 2021) | Razer Blade (2015) |
|---------------------------|-----------------------------|--------------------|
| Immediate-mode GUIs       |                             |                    |
| Dear ImGui                | 7.5                         | 27.8               |
| ImPlot                    | 8.9                         | 29.7               |
| egui                      | 8.2                         | 38.6               |
| rerun                     | 11.1                        | 63.2               |
| Retained-mode/Apps        |                             |                    |
| Spotify                   | 5.8                         | 25.6               |
| VS Code                   | 7.0                         | 27.3               |
| YouTube                   | 11.5                        | 30.9               |
| Facebook                  | 8.7                         | 34.8               |

As the table shows, Dear ImGui’s power draw is within the range of common retained-mode applications. The author notes that enabling idle optimizations further reduces IMGUI cost to near-zero when there is no input or state change, which is representative of many HUD/overlay scenarios[^6].

For Dear ImGui + SDL2 specifically, best practices from field guides include:
- Use SDL’s accelerated renderer with vsync to cap CPU/GPU contention;
- Handle High DPI by scaling so widgets render at comfortable sizes;
- Pause UI drawing when minimized to avoid wasted work;
- Control event routing and input focus explicitly to avoid spurious updates[^2].


## Cross-Platform and Build System Considerations

CEGUI provides a modular architecture with optional dependencies and CMake build scripts across Windows and *NIX. Dynamic linking is recommended for compatibility, and stable branches preserve API/ABI compatibility across compiler versions[^3]. libsdl2gui explicitly lists C++20 minimums and recent compiler versions (Clang 14, GCC 13, MSVC 2019), and integrates with SDL2 components that are already familiar to Cataclysm-BN packagers[^8]. 

Raylib-based solutions are cross-platform, but adopting Raylib solely for GUI may complicate Cataclysm-BN’s dependency graph and CI matrix. Conversely, for a standalone tool, Raylib combined with rlImGui or raygui is a pragmatic and productive choice that keeps the tool decoupled from the game’s core[^4][^5][^16].

A community-maintained list of SDL GUI toolkits is useful for broad discovery, particularly if the project later wants to compare alternative SDL-native libraries without adding a second graphics API[^11].


## Recommendations and Phased Implementation Plan

Shortlist per use case:

- In-game overlays, HUDs, and panels: SDL2-native retained-mode GUI (libsdl2gui or SDL_gui). Rationale: native SDL2Renderer integration, no secondary graphics API, and event-model alignment.
- Debug/overlay tools: Dear ImGui. Rationale: minimal integration, broad ecosystem, demonstrably efficient for overlays and tools[^2][^6][^9][^14][^15].
- Raylib-based tool app: rlImGui or raygui. Rationale: when building a separate tool with Raylib, these options maximize developer productivity[^4][^5][^16].
- Complex retained-mode UI: CEGUI. Rationale: mature retained-mode, extensive features and theming, suitable for large, long-lived UI screens if the team embraces its model[^3].

Primary recommendation for Cataclysm-BN:

- Adopt a retained-mode SDL2-native GUI (e.g., libsdl2gui) for core in-game UI overlays. This leverages Cataclysm-BN’s existing renderer and event handling, keeps dependencies focused, and yields predictable updates. Use Dear ImGui for debug overlays, profiling panels, and internal tools where immediate-mode’s rapid iteration is valuable.

Phased plan:

1) Phase 0 — Integration spike:
- Select a small set of widgets (label, button, toggle, inventory list) in libsdl2gui and wire them into the existing SDL2 loop; 
- Add a minimal Dear ImGui overlay (FPS counter, basic console) via the SDL2Renderer backends; 
- Validate High DPI scaling, focus handling, and minimize-pause behavior[^2][^8].

2) Phase 1 — Core overlay adoption:
- Migrate a small, non-critical HUD panel to libsdl2gui; 
- Define theme files and font/size guidelines; 
- Add event bridging patterns for the new GUI layer[^8].

3) Phase 2 — Tooling expansion:
- Introduce Dear ImGui for debugging, telemetry, and in-game console; 
- Optionally, prototype a Raylib-based side tool using rlImGui or raygui to validate product fit for a dedicated editor/app[^5][^16].

4) Phase 3 — Optional retained-mode expansion:
- If requirements demand, evaluate CEGUI for complex menus/layouts; 
- Establish performance testing procedures in Release builds to avoid anti-patterns noted by the maintainers[^3].

To visualize the trade-offs, Table 8 summarizes the options.

Table 8 — Comparison matrix across primary criteria

| Criterion                         | SDL2-native retained (libsdl2gui/SDL_gui) | CEGUI (retained)                              | Dear ImGui (immediate)                       | Raylib-based (raygui/rlImGui)                |
|----------------------------------|-------------------------------------------|-----------------------------------------------|----------------------------------------------|----------------------------------------------|
| Integration complexity           | Low (SDL2Renderer)                        | Moderate (modular deps, event wiring)         | Low (SDL2 backends available)                | Moderate (adds Raylib dependency)            |
| Fit to Cataclysm-BN architecture | High                                       | Medium                                        | High (tools/overlays)                        | Low–Medium (tools or dual-framework)         |
| Performance expectations         | High (SDL2Renderer aligned)               | High (mature, release-mode critical)          | High (empirical efficiency, vertex buffers)  | High (Raylib loop; good for tools)           |
| Memory footprint                 | Moderate                                   | Moderate–High                                 | Low–Moderate                                 | Moderate                                     |
| Theming/styling                  | Theme files/XML; text shaping (SDL_gui)   | Rich, flexible theming                        | Style system; skinnable                      | Style systems (raygui), ImGui styles (rlImGui) |
| Community/maturity               | Moderate                                   | High                                          | High                                         | Moderate–High (Raylib + ImGui ecosystems)    |
| Licensing                        | Zlib/libpng (CEGUI has separate terms)    | Flexible, see project                         | MIT-like (open, permissive)                  | Zlib (Raylib/raygui), MIT-like (ImGui)       |

Licensing note: libsdl2gui references zlib/libpng-licensed SDL components; CEGUI and Raylib/raygui have separate permissive licenses; Dear ImGui is open-source and permissive. Teams should perform a formal license review before adoption[^3][^4][^5][^8].


## Appendix: Integration Notes and Example Patterns

Dear ImGui + SDL2Renderer essentials (C++20, CMake):
- Initialize SDL2 (video, timer, game controller), create a window and accelerated renderer with vsync. Enable High DPI scaling and calculate a scale factor to pass to SDL_RenderSetScale.
- Initialize ImGui backends for SDL2 and SDL2Renderer; enable keyboard navigation, and optionally docking and viewports.
- In the main loop: poll events; if minimized, pause UI drawing; call ImGui::NewFrame, render panels, then render the ImGui draw data via the SDL2Renderer backend. A practical guide provides reference flags and event-handling patterns[^2].
- Integration rationale and architecture: Dear ImGui outputs optimized vertex buffers rather than driving immediate-mode rendering, which is why it fits cleanly with SDL2Renderer and allows batching with the game’s own geometry[^9].

CEGUI build and linking:
- Use CMake to configure; dynamic linking is recommended for compatibility; ensure the needed rendering backend and image/XML parsers are enabled. Leverage stable branches for API/ABI stability. Avoid reloading layouts every frame and minimize redundant updates to achieve expected performance in Release builds[^3].

Nuklear integration:
- Provide a small adapter that converts Nuklear’s primitive draw commands into SDL2Renderer fills and texture draws (or into geometry for batching if using a custom pipeline). Font baking and memory control are first-class features; compile only needed modules. Because Nuklear has no default OS window or input handling, Cataclysm-BN must feed events and manage the windowing surface[^7].

Raylib + rlImGui:
- If adopting Raylib for a tools app, integrate rlImGui by calling rlImGuiSetup(true), then wrap ImGui code between rlImGuiBegin and rlImGuiEnd inside Raylib’s BeginDrawing/EndDrawing. A CMake setup with a small set of source files is straightforward[^5][^16].


## Information Gaps

- No head-to-head, quantitative benchmarks of memory usage and frame time specific to Cataclysm-BN’s rendering path for CEGUI vs Dear ImGui vs libsdl2gui/SDL_gui vs raygui/rlImGui.
- The precise performance characteristics of raygui 4.x under high-frequency overlay workloads on Cataclysm-BN’s platforms are not directly documented.
- The Dear ImGui docking/viewports stability in a pure SDL2Renderer configuration (as opposed to a Raylib or OpenGL context) requires project-specific validation.
- No Cataclysm-BN–specific internal guidance on GUI theming and DPI handling beyond the project’s existing SDL2 usage.
- Licensing nuances and compatibility with Cataclysm-BN’s license for each third-party GUI library should be reviewed by legal counsel.


## References

[^1]: Cataclysm-BN GitHub Repository. https://github.com/cataclysmbnteam/Cataclysm-BN
[^2]: Martin Fieber, “GUI Development with C++, SDL2, and Dear ImGui.” https://martin-fieber.de/blog/gui-development-with-cpp-sdl2-and-dear-imgui/
[^3]: CEGUI GitHub Repository. https://github.com/cegui/cegui
[^4]: raygui GitHub Repository. https://github.com/raysan5/raygui
[^5]: rlImGui: Dear ImGui bindings for Raylib. https://github.com/raylib-extras/rlImGui/
[^6]: Forrest The Woods, “Proving Immediate Mode GUIs are Performant.” https://www.forrestthewoods.com/blog/proving-immediate-mode-guis-are-performant/
[^7]: Nuklear GitHub Repository. https://github.com/Immediate-Mode-UI/Nuklear
[^8]: libsdl2gui GitHub Repository. https://github.com/adamajammary/libsdl2gui
[^9]: Nicolas Guillemot, “Dear ImGui” (CppCon 2016). https://www.youtube.com/watch?v=LSRJ1jZq90k
[^10]: Debian: cataclysm-dda-sdl package details (graphical SDL2-based interface). https://packages.debian.org/sid/games/cataclysm-dda-sdl
[^11]: SDL Discourse: List of GUI toolkits for SDL. https://discourse.libsdl.org/t/list-all-gui-toolkits-for-sdl/21911
[^12]: SDL_gui GitHub Repository. https://github.com/mozeal/SDL_gui
[^13]: Dear ImGui GitHub Repository. https://github.com/ocornut/imgui
[^14]: Stack Overflow: Performance implications of immediate-mode GUI. https://stackoverflow.com/questions/47444189/what-are-the-performance-implications-of-using-an-immediate-mode-gui-compared-to
[^15]: Wikipedia: Immediate mode (computer graphics). https://en.wikipedia.org/wiki/Immediate_mode_(computer_graphics)
[^16]: “Building a Cross-Platform C++ GUI App with CMake, Raylib, and Dear ImGui.” https://keasigmadelta.com/blog/building-a-cross-platform-c-gui-app-with-cmake-raylib-and-dear-imgui/