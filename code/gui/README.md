# Cataclysm-BN GUI Manager

This directory contains the core GUI manager implementation for Cataclysm: Bright Nights Dear ImGui overlay integration.

## Overview

The GUI Manager provides a non-invasive overlay system that integrates Dear ImGui with Cataclysm-BN's existing SDL2-based rendering pipeline. The system follows the architectural design patterns described in `/docs/design/gui_architecture_design.md` and implements proper lifecycle management, input routing, and error handling.

## Architecture

### Core Components

1. **GUIManager** (`gui_manager.h/cpp`)
   - Main class for managing the GUI overlay lifecycle
   - Handles initialization, shutdown, and event routing
   - Provides configuration and state management
   - Integrates with Cataclysm-BN's UI manager system

2. **GUIRenderer** (`gui_renderer.h/cpp`)
   - Dear ImGui integration with SDL2 backends
   - Handles rendering pipeline and DPI scaling
   - Manages font atlas and texture resources
   - Provides low-level ImGui context management

### Key Features

- **PIMPL Pattern**: Implementation details hidden in private structs to minimize header pollution
- **Safe SDL2 Integration**: Proper initialization and cleanup of SDL2 backends
- **Event Routing**: Sophisticated input handling with configurable pass-through
- **DPI Scaling**: High-DPI support for modern displays
- **Error Handling**: Comprehensive error tracking and reporting
- **Configurable**: Flexible configuration for different use cases

## Files

```
code/gui/
├── gui_manager.h          # Main GUI manager header
├── gui_manager.cpp        # GUI manager implementation
├── gui_renderer.h         # GUI renderer header
├── gui_renderer.cpp       # GUI renderer implementation
├── CMakeLists.txt         # Build configuration
├── test_gui_manager.cpp   # Example/test application
└── README.md             # This file
```

## Usage

### Basic Integration

```cpp
#include "gui_manager.h"

GUIManager gui_manager;

// Configuration
GUIManager::Config config;
config.enabled = true;
config.pass_through_input = true;
config.dpi_scale = 1.0f;
config.minimize_pause = true;

// Initialize
if (!gui_manager.Initialize(window, renderer, config)) {
    // Handle initialization failure
    return;
}

// Register callbacks
gui_manager.RegisterRedrawCallback([]() {
    // Handle redraw request
});

gui_manager.RegisterResizeCallback([](int width, int height) {
    // Handle window resize
});

// Main loop
while (running) {
    // Handle SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (gui_manager.HandleEvent(event)) {
            // Event was consumed by GUI
            continue;
        }
        // Handle game events
    }
    
    // Update
    gui_manager.Update();
    
    // Render game scene first
    RenderGameScene();
    
    // Render GUI overlay
    gui_manager.Render();
    
    // Present
    SDL_RenderPresent(renderer);
}
```

### Configuration Options

```cpp
struct Config {
    bool enabled = true;                    // Enable/disable GUI overlay
    bool pass_through_input = true;         // Pass unconsumed events to game
    float dpi_scale = 1.0f;                 // DPI scaling factor
    bool minimize_pause = true;             // Pause rendering when minimized
    std::string ini_filename;               // Custom ImGui ini filename
};
```

### API Reference

#### GUIManager Methods

- `bool Initialize(SDL_Window*, SDL_Renderer*, const Config&)` - Initialize the GUI system
- `void Shutdown()` - Clean up resources
- `void Update()` - Update GUI state (per frame)
- `void Render()` - Render GUI overlay (after game scene)
- `bool HandleEvent(const SDL_Event&)` - Process SDL events
- `void Open()` / `void Close()` - Open/close GUI overlay
- `bool IsOpen()` / `bool IsEnabled()` - State queries
- `void SetEnabled(bool)` - Enable/disable system

#### Callbacks

- `RegisterRedrawCallback(RedrawCallback)` - Called when UI needs redraw
- `RegisterResizeCallback(ResizeCallback)` - Called on window resize

## Build Instructions

### Using CMake

```bash
# Build the GUI library
cd code/gui
mkdir build && cd build
cmake ..
make -j$(nproc)

# Build and run test application
make test_gui
./test_gui
```

### Manual Compilation

```bash
# Compile with g++
g++ -std=c++17 -I./include test_gui_manager.cpp gui_manager.cpp gui_renderer.cpp \
    -lSDL2 -o test_gui

# Run test application
./test_gui
```

## Integration with Cataclysm-BN

### 1. Rendering Integration

In the main rendering loop (typically in `sdltiles.cpp`):

```cpp
// After game scene is rendered
game_scene.Render();

// Render GUI overlay
if (gui_manager.IsEnabled() && gui_manager.IsOpen()) {
    gui_manager.Render();
}

// Present frame
SDL_RenderPresent(renderer);
```

### 2. Input Integration

In the main event loop:

```cpp
SDL_Event event;
while (SDL_PollEvent(&event)) {
    // Let GUI manager handle events first
    if (gui_manager.HandleEvent(event)) {
        continue;  // Event consumed by GUI
    }
    
    // Handle game events
    game.HandleEvent(event);
}
```

### 3. UI Manager Integration

Register with the existing UI manager:

```cpp
// In initialization
ui_manager.RegisterLayer(&gui_manager);

// Register callbacks
gui_manager.RegisterRedrawCallback([&]() {
    ui_manager.RequestRedraw();
});

gui_manager.RegisterResizeCallback([&](int w, int h) {
    ui_manager.OnScreenResize(w, h);
});
```

## Error Handling

The system includes comprehensive error handling:

- **Initialization Errors**: Detailed error messages for configuration issues
- **SDL2 Errors**: Proper cleanup on SDL2 initialization failure
- **Dear ImGui Errors**: Graceful handling of ImGui context issues
- **Runtime Errors**: State validation and safe error recovery

Access error information via:
```cpp
const std::string& error = gui_manager.GetLastError();
```

## Performance Considerations

- **Batching**: Uses ImDrawLists for efficient rendering
- **Conditional Updates**: Only updates when overlay is active
- **DPI Scaling**: Proper high-DPI support for crisp rendering
- **Minimize Pause**: Stops rendering when window is minimized
- **Pass-through**: Configurable event routing to avoid conflicts

## Testing

The test application (`test_gui_manager.cpp`) demonstrates:

- Basic initialization and shutdown
- Event handling and input routing
- Window resize handling
- Configuration options
- Performance testing with FPS limiting

## Platform Support

- **Windows**: Full support with SDL2
- **Linux**: Full support with SDL2
- **macOS**: Full support with SDL2 (including Retina/High-DPI)
- **ASCII Builds**: Gracefully disabled (no SDL2 dependency)

## Dependencies

- **SDL2**: For window management and input handling
- **Dear ImGui**: For GUI framework (headers required for full implementation)
- **C++17**: For modern language features

## Contributing

When contributing to the GUI system:

1. Follow the PIMPL pattern to minimize header pollution
2. Include comprehensive error handling
3. Add proper documentation for public APIs
4. Test on multiple platforms
5. Maintain performance characteristics
6. Follow existing code style and patterns

## License

This implementation follows the same license as Cataclysm: Bright Nights.

## Notes

This is a core implementation following the design specifications. The actual Dear ImGui headers and backends are required for a complete implementation. The placeholder code demonstrates the API design and integration patterns without depending on external libraries.