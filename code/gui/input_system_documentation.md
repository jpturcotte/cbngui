# Dual Input System Documentation

## Overview

The Dual Input System provides coordinated handling of both keyboard and mouse input for the Cataclysm-BN GUI system. It routes events to appropriate handlers (GUI vs game) while maintaining input priority and focus management.

## Key Features

### 1. Event Coordination
- **Dual Input Support**: Handles both keyboard and mouse events seamlessly
- **Priority-Based Routing**: Events are processed based on priority levels
- **Context-Aware Processing**: Events can be filtered by input context
- **Thread-Safe Operations**: All operations are thread-safe with proper mutex protection

### 2. Focus Management
- **Focus States**: NONE, GUI, GAME, SHARED
- **Focus Change Notifications**: Listeners can be notified of focus changes
- **Automatic Focus Detection**: Mouse position determines focus when appropriate
- **Configurable Focus Behavior**: Settings control how focus changes

### 3. Event Routing
- **Handler Registration**: Register handlers for specific event types
- **Priority Levels**: LOWEST(0) to HIGHEST(4) priority processing
- **Context Filtering**: Handlers can be scoped to specific contexts
- **Pass-Through Control**: Configure when events pass through to game input

### 4. Integration Points
- **SDL2 Events**: Direct integration with SDL2 event system
- **Game Input Compatibility**: Preserves existing game input behavior
- **GUI Integration**: Designed to work with Dear ImGui overlay system
- **Extensible Architecture**: Easy to add new event types and handlers

## Usage Examples

### Basic Initialization

```cpp
#include "gui/input_manager.h"

using namespace BN::GUI;

// Create input manager with default settings
InputManager input_manager;
input_manager.Initialize();

// Or with custom settings
InputManager::InputSettings settings;
settings.enable_mouse = true;
settings.enable_keyboard = true;
settings.pass_through_enabled = true;
settings.mouse_sensitivity = 75;

InputManager custom_manager(settings);
custom_manager.Initialize();
```

### Registering Event Handlers

```cpp
// Register a mouse click handler
int mouse_handler_id = input_manager.RegisterHandler(
    InputManager::EventType::MOUSE_BUTTON_PRESS,
    [](const GUIEvent& event) {
        // Handle mouse click
        if (event.sdl_event.button.button == SDL_BUTTON_LEFT) {
            // Process left click
            return true; // Consume event
        }
        return false; // Pass through
    },
    InputManager::Priority::HIGH
);

// Register a keyboard handler
int key_handler_id = input_manager.RegisterHandler(
    InputManager::EventType::KEYBOARD_PRESS,
    [](const GUIEvent& event) {
        SDL_Keycode key = event.sdl_event.key.keysym.sym;
        if (key == SDLK_ESCAPE) {
            // Handle escape key
            return true; // Consume event
        }
        return false; // Pass to game
    },
    InputManager::Priority::NORMAL,
    "main_menu" // Context-specific
);
```

### Focus Management

```cpp
// Add focus change listener
int focus_listener_id = input_manager.AddFocusListener(
    [](InputManager::FocusState previous, InputManager::FocusState current) {
        if (previous != current) {
            if (current == InputManager::FocusState::GUI) {
                // GUI gained focus - pause game input
            } else if (current == InputManager::FocusState::GAME) {
                // Game gained focus - resume game input
            }
        }
    }
);

// Set focus state
input_manager.SetFocusState(InputManager::FocusState::GUI, "Menu opened");
```

### Mouse Area Management

```cpp
// Define GUI area for focus detection
input_manager.SetGUIAreaBounds(100, 100, 800, 600);

// Check if mouse is over GUI
if (input_manager.IsMouseOverGUI()) {
    // Handle GUI mouse interaction
}

// Get mouse position and delta
int mouse_x, mouse_y, delta_x, delta_y;
input_manager.GetMousePosition(mouse_x, mouse_y);
input_manager.GetMouseDelta(delta_x, delta_y);
```

### Input Context Management

```cpp
// Create and register input context
class MenuContext : public InputContext {
public:
    bool HandleEvent(const GUIEvent& event) override {
        // Handle menu-specific events
        return false; // Not consumed
    }
    
    const std::string& GetName() const override {
        static const std::string name = "menu_context";
        return name;
    }
    
    InputManager::Priority GetPriority() const override {
        return InputManager::Priority::HIGH;
    }
    
    bool ShouldReceiveEvent(const GUIEvent& event) const override {
        return true; // Receive all events in this context
    }
};

// Register context
auto menu_context = std::make_unique<MenuContext>();
input_manager.SetInputContext("main_menu", std::move(menu_context));
```

## Event Flow

### Normal Operation
1. SDL event is received
2. InputManager processes event based on type (keyboard/mouse)
3. Focus state is checked
4. Event is routed to appropriate handlers based on:
   - Focus state (GUI/Game/Shared)
   - Mouse position (for GUI area detection)
   - Handler priorities
5. Handlers are called in priority order
6. Consumed events stop propagation
7. Unconsumed events may pass through to game

### Focus States
- **NONE**: No focus, events are ignored
- **GUI**: GUI has exclusive focus, events go to GUI handlers only
- **GAME**: Game has focus, events go to game (with GUI area exception)
- **SHARED**: Both GUI and game receive events (GUI first)

### Priority Levels
- **LOWEST (0)**: Background processing, rarely used
- **LOW (1)**: Non-critical events
- **NORMAL (2)**: Standard events (default)
- **HIGH (3)**: Important user interactions
- **HIGHEST (4)**: Critical system events

## Thread Safety

The InputManager is designed to be thread-safe:
- **Mutex Protection**: All shared data is protected by mutexes
- **Atomic Operations**: State flags use atomic operations
- **Lock Ordering**: Consistent lock ordering to prevent deadlocks
- **Exception Safety**: All critical sections are exception-safe

## Integration with Game Input

### Backward Compatibility
The system maintains full compatibility with existing game input:
- **Pass-Through Mode**: When enabled, unconsumed events go to game
- **Focus Management**: Game input is automatically disabled when GUI has focus
- **No Breaking Changes**: Existing game input code remains unchanged

### Event Consumption Rules
1. **GUI Area Mouse Events**: Consumed by GUI when mouse is in defined GUI area
2. **High-Priority Events**: Always processed by GUI first
3. **Keyboard Events**: Processed based on focus state and context
4. **Pass-Through Events**: Unconsumed events passed to game when configured

## Performance Considerations

### Efficiency Features
- **Priority Sorting**: Handlers are sorted once per event type
- **Context Filtering**: Efficient filtering by context
- **Minimal Locking**: Locking is minimized to reduce contention
- **Statistics Tracking**: Built-in performance monitoring

### Monitoring
```cpp
// Get performance statistics
auto stats = input_manager.GetStatistics();

std::cout << "Events processed: " << stats.events_processed << std::endl;
std::cout << "Events consumed: " << stats.events_consumed << std::endl;
std::cout << "Events passed through: " << stats.events_passed_through << std::endl;
std::cout << "Handlers called: " << stats.handlers_called << std::endl;
std::cout << "Focus changes: " << stats.focus_changes << std::endl;

// Reset statistics
input_manager.ResetStatistics();
```

## Configuration Options

### InputSettings Structure
```cpp
struct InputSettings {
    bool enable_mouse = true;              // Enable mouse input
    bool enable_keyboard = true;           // Enable keyboard input
    bool pass_through_enabled = true;      // Allow pass-through to game
    bool prevent_game_input_when_gui_focused = true;  // Auto-prevent game input
    Priority default_priority = Priority::NORMAL;  // Default event priority
    int max_mouse_sensitivity = 100;       // Maximum sensitivity
    int mouse_sensitivity = 50;            // Current sensitivity
    bool mouse_relative_mode = false;      // Relative mouse mode
    bool focus_indicator_enabled = true;   // Show focus indicators
};
```

### Runtime Configuration
```cpp
// Update settings at runtime
InputManager::InputSettings new_settings = input_manager.GetSettings();
new_settings.mouse_sensitivity = 75;
new_settings.pass_through_enabled = false;
input_manager.UpdateSettings(new_settings);
```

## Error Handling

### Robustness Features
- **Exception Safety**: All operations are exception-safe
- **Graceful Degradation**: System continues working even if handlers fail
- **Debug Logging**: Comprehensive debug logging for troubleshooting
- **Resource Management**: Automatic cleanup of resources

### Debug Support
- **Debug Levels**: Configurable debug output levels
- **Event Tracing**: Detailed event flow tracing
- **Statistics**: Performance and usage statistics
- **Error Recovery**: Automatic error recovery mechanisms

## Best Practices

### Handler Design
1. **Keep Handlers Lightweight**: Handlers should be fast and non-blocking
2. **Check Event Type**: Always check event.type before processing
3. **Return Consumption Status**: Always return true if event consumed
4. **Handle Exceptions**: Wrap handler code in try-catch blocks
5. **Use Appropriate Priorities**: Match priority to event importance

### Focus Management
1. **Set GUI Area Early**: Define GUI area before processing events
2. **Update Focus Promptly**: Change focus state when UI state changes
3. **Notify Users**: Provide visual feedback for focus changes
4. **Test Scenarios**: Test all focus change scenarios

### Performance
1. **Minimize Handler Count**: Register only necessary handlers
2. **Use Context Filtering**: Filter events by context when possible
3. **Monitor Statistics**: Regularly check performance statistics
4. **Optimize Priorities**: Use appropriate priority levels

## Troubleshooting

### Common Issues
1. **Events Not Being Consumed**: Check focus state and GUI area definition
2. **Performance Issues**: Check statistics and reduce handler count
3. **Focus Problems**: Verify focus change notifications are working
4. **Thread Safety Issues**: Ensure proper mutex usage in custom handlers

### Debug Tools
1. **Statistics Monitoring**: Use GetStatistics() to monitor performance
2. **Debug Logging**: Enable debug logging for detailed information
3. **Focus State Checking**: Use GetFocusState() to verify focus
4. **Mouse Position Tracking**: Use GetMousePosition() for debugging

This dual input system provides a robust, flexible, and efficient foundation for handling both mouse and keyboard input in the Cataclysm-BN GUI system while maintaining full compatibility with the existing game input mechanisms.