/**
 * @file input_manager_example.cpp
 * @brief Example implementation showing how to integrate the dual input system
 * 
 * This file demonstrates various use cases and integration patterns for the
 * InputManager class, including basic setup, event handling, and integration
 * with the game loop.
 */

#include "input_manager.h"
#include <iostream>
#include <SDL.h>
#include "debug.h"

namespace BN {
namespace GUI {
namespace Examples {

// Example 1: Basic Input Manager Setup and Usage
class BasicInputExample {
public:
    void Run() {
        std::cout << "=== Basic Input Manager Example ===" << std::endl;
        
        // 1. Create input manager with custom settings
        InputManager::InputSettings settings;
        settings.enable_mouse = true;
        settings.enable_keyboard = true;
        settings.pass_through_enabled = true;
        settings.mouse_sensitivity = 75;
        
        InputManager input_manager(settings);
        
        // 2. Initialize the manager
        if (!input_manager.Initialize()) {
            std::cerr << "Failed to initialize input manager" << std::endl;
            return;
        }
        
        // 3. Register event handlers
        RegisterHandlers(input_manager);
        
        // 4. Set up focus management
        SetupFocusManagement(input_manager);
        
        // 5. Define GUI area
        input_manager.SetGUIAreaBounds(100, 100, 800, 600);
        
        // 6. Process some test events
        ProcessTestEvents(input_manager);
        
        // 7. Show statistics
        ShowStatistics(input_manager);
        
        // 8. Shutdown
        input_manager.Shutdown();
        
        std::cout << "=== Basic Example Complete ===" << std::endl;
    }
    
private:
    void RegisterHandlers(InputManager& manager) {
        // Mouse click handler
        int mouse_handler = manager.RegisterHandler(
            InputManager::EventType::MOUSE_BUTTON_PRESS,
            [](const GUIEvent& event) {
                int x = event.sdl_event.button.x;
                int y = event.sdl_event.button.y;
                std::cout << "Mouse click at (" << x << ", " << y << ")" << std::endl;
                
                if (event.sdl_event.button.button == SDL_BUTTON_LEFT) {
                    std::cout << "  Left button clicked" << std::endl;
                    return true; // Consume event
                }
                return false;
            },
            InputManager::Priority::HIGH,
            "gui_mouse"
        );
        
        // Keyboard handler
        int key_handler = manager.RegisterHandler(
            InputManager::EventType::KEYBOARD_PRESS,
            [](const GUIEvent& event) {
                SDL_Keycode key = event.sdl_event.key.keysym.sym;
                std::cout << "Key pressed: " << SDL_GetKeyName(key) << std::endl;
                
                if (key == SDLK_ESCAPE) {
                    std::cout << "  Escape pressed - exiting" << std::endl;
                    return true; // Consume escape key
                }
                return false; // Pass other keys through
            },
            InputManager::Priority::NORMAL,
            "gui_keyboard"
        );
        
        // Mouse movement handler (lower priority)
        int move_handler = manager.RegisterHandler(
            InputManager::EventType::MOUSE_MOVE,
            [](const GUIEvent& event) {
                // Only log every few moves to avoid spam
                static int move_count = 0;
                if (++move_count % 10 == 0) {
                    int x = event.sdl_event.motion.x;
                    int y = event.sdl_event.motion.y;
                    std::cout << "Mouse moved to (" << x << ", " << y << ")" << std::endl;
                }
                return false; // Don't consume movement events
            },
            InputManager::Priority::LOW,
            "gui_mouse"
        );
        
        std::cout << "Registered " << 3 << " event handlers" << std::endl;
    }
    
    void SetupFocusManagement(InputManager& manager) {
        // Add focus change listener
        int focus_listener = manager.AddFocusListener(
            [](InputManager::FocusState previous, InputManager::FocusState current) {
                std::cout << "Focus changed from " 
                         << static_cast<int>(previous) << " to " 
                         << static_cast<int>(current) << std::endl;
            }
        );
        
        std::cout << "Added focus change listener" << std::endl;
    }
    
    void ProcessTestEvents(InputManager& manager) {
        std::cout << "\nProcessing test events..." << std::endl;
        
        // Simulate some events
        SDL_Event test_event;
        
        // Test 1: Mouse move
        test_event.type = SDL_MOUSEMOTION;
        test_event.motion.x = 150;
        test_event.motion.y = 200;
        manager.ProcessEvent(test_event);
        
        // Test 2: Mouse click
        test_event.type = SDL_MOUSEBUTTONDOWN;
        test_event.button.x = 150;
        test_event.button.y = 200;
        test_event.button.button = SDL_BUTTON_LEFT;
        manager.ProcessEvent(test_event);
        
        // Test 3: Key press
        test_event.type = SDL_KEYDOWN;
        test_event.key.keysym.sym = SDLK_SPACE;
        test_event.key.keysym.scancode = SDL_SCANCODE_SPACE;
        manager.ProcessEvent(test_event);
        
        // Test 4: Escape key
        test_event.key.keysym.sym = SDLK_ESCAPE;
        manager.ProcessEvent(test_event);
    }
    
    void ShowStatistics(const InputManager& manager) {
        auto stats = manager.GetStatistics();
        std::cout << "\nInput Manager Statistics:" << std::endl;
        std::cout << "  Events processed: " << stats.events_processed << std::endl;
        std::cout << "  Events consumed: " << stats.events_consumed << std::endl;
        std::cout << "  Events passed through: " << stats.events_passed_through << std::endl;
        std::cout << "  Handlers called: " << stats.handlers_called << std::endl;
        std::cout << "  Active handlers: " << stats.active_handlers << std::endl;
        std::cout << "  Focus changes: " << stats.focus_changes << std::endl;
    }
};

// Example 2: Input Context Implementation
class MenuInputContext : public InputContext {
public:
    explicit MenuInputContext(const std::string& name) : name_(name) {}
    
    bool HandleEvent(const GUIEvent& event) override {
        std::cout << "MenuContext '" << name_ << "' handling event" << std::endl;
        
        // Handle menu-specific events
        if (event.type == InputManager::EventType::MOUSE_BUTTON_PRESS) {
            // Handle menu navigation
            return true;
        }
        
        return false; // Not consumed
    }
    
    const std::string& GetName() const override {
        return name_;
    }
    
    InputManager::Priority GetPriority() const override {
        return InputManager::Priority::HIGH;
    }
    
    bool ShouldReceiveEvent(const GUIEvent& event) const override {
        // Only receive events when menu is active
        return event.context == "menu";
    }
    
private:
    std::string name_;
};

class GameInputContext : public InputContext {
public:
    explicit GameInputContext(const std::string& name) : name_(name) {}
    
    bool HandleEvent(const GUIEvent& event) override {
        // Handle game-specific input
        if (event.type == InputManager::EventType::KEYBOARD_PRESS) {
            SDL_Keycode key = event.sdl_event.key.keysym.sym;
            
            switch (key) {
                case SDLK_w: std::cout << "Game: Move forward" << std::endl; break;
                case SDLK_s: std::cout << "Game: Move backward" << std::endl; break;
                case SDLK_a: std::cout << "Game: Move left" << std::endl; break;
                case SDLK_d: std::cout << "Game: Move right" << std::endl; break;
                default: return false; // Not consumed
            }
            return true;
        }
        
        return false; // Not consumed
    }
    
    const std::string& GetName() const override {
        return name_;
    }
    
    InputManager::Priority GetPriority() const override {
        return InputManager::Priority::NORMAL;
    }
    
    bool ShouldReceiveEvent(const GUIEvent& event) const override {
        // Only receive events when game is active
        return event.context == "game";
    }
    
private:
    std::string name_;
};

class ContextExample {
public:
    void Run() {
        std::cout << "\n=== Input Context Example ===" << std::endl;
        
        InputManager manager;
        manager.Initialize();
        
        // Create input contexts
        auto menu_context = std::make_unique<MenuInputContext>("main_menu");
        auto game_context = std::make_unique<GameInputContext>("gameplay");
        
        // Register contexts
        manager.SetInputContext("menu", std::move(menu_context));
        manager.SetInputContext("game", std::move(game_context));
        
        // Register context-specific handlers
        RegisterContextHandlers(manager);
        
        // Simulate context switching
        SimulateContextSwitching(manager);
        
        manager.Shutdown();
        std::cout << "=== Context Example Complete ===" << std::endl;
    }
    
private:
    void RegisterContextHandlers(InputManager& manager) {
        // Menu context handler
        manager.RegisterHandler(
            InputManager::EventType::MOUSE_BUTTON_PRESS,
            [](const GUIEvent& event) {
                if (event.context == "menu") {
                    std::cout << "Menu context: Processing mouse event" << std::endl;
                    return true;
                }
                return false;
            },
            InputManager::Priority::HIGH,
            "menu"
        );
        
        // Game context handler
        manager.RegisterHandler(
            InputManager::EventType::KEYBOARD_PRESS,
            [](const GUIEvent& event) {
                if (event.context == "game") {
                    std::cout << "Game context: Processing key event" << std::endl;
                    return true;
                }
                return false;
            },
            InputManager::Priority::NORMAL,
            "game"
        );
    }
    
    void SimulateContextSwitching(InputManager& manager) {
        SDL_Event test_event;
        
        std::cout << "\nSimulating context switching..." << std::endl;
        
        // Switch to menu context
        std::cout << "Switching to menu context" << std::endl;
        manager.SetFocusState(InputManager::FocusState::GUI, "Menu opened");
        
        // Test menu input
        test_event.type = SDL_MOUSEBUTTONDOWN;
        test_event.button.x = 200;
        test_event.button.y = 300;
        test_event.button.button = SDL_BUTTON_LEFT;
        manager.ProcessEvent(test_event);
        
        // Switch to game context
        std::cout << "\nSwitching to game context" << std::endl;
        manager.SetFocusState(InputManager::FocusState::GAME, "Game started");
        
        // Test game input
        test_event.type = SDL_KEYDOWN;
        test_event.key.keysym.sym = SDLK_w;
        manager.ProcessEvent(test_event);
    }
};

// Example 3: Integration with Game Loop
class GameLoopIntegration {
public:
    void Run() {
        std::cout << "\n=== Game Loop Integration Example ===" << std::endl;
        
        InputManager manager;
        manager.Initialize();
        SetupGameLoopHandlers(manager);
        
        // Simulate game loop
        SimulateGameLoop(manager);
        
        manager.Shutdown();
        std::cout << "=== Game Loop Example Complete ===" << std::endl;
    }
    
private:
    void SetupGameLoopHandlers(InputManager& manager) {
        // GUI overlay handler
        manager.RegisterHandler(
            InputManager::EventType::MOUSE_BUTTON_PRESS,
            [](const GUIEvent& event) {
                if (manager.IsMouseOverGUI()) {
                    std::cout << "GUI overlay: Mouse click in GUI area" << std::endl;
                    return true; // Consume GUI events
                }
                return false; // Let game handle it
            },
            InputManager::Priority::HIGHEST,
            "gui_overlay"
        );
        
        // Global hotkey handler
        manager.RegisterHandler(
            InputManager::EventType::KEYBOARD_PRESS,
            [](const GUIEvent& event) {
                SDL_Keycode key = event.sdl_event.key.keysym.sym;
                
                // F1 for help
                if (key == SDLK_F1) {
                    std::cout << "Global hotkey: F1 - Help" << std::endl;
                    return true;
                }
                
                // F2 for settings
                if (key == SDLK_F2) {
                    std::cout << "Global hotkey: F2 - Settings" << std::endl;
                    return true;
                }
                
                return false; // Not a global hotkey
            },
            InputManager::Priority::HIGHEST,
            "global_hotkeys"
        );
    }
    
    void SimulateGameLoop(InputManager& manager) {
        std::cout << "\nSimulating game loop..." << std::endl;
        
        // Set up initial state
        manager.SetFocusState(InputManager::FocusState::GAME, "Game started");
        manager.SetGUIAreaBounds(50, 50, 200, 150); // GUI overlay area
        
        SDL_Event events[] = {
            // Game input
            { SDL_KEYDOWN, {{0}}, {{SDLK_w}}, 0, 0 },
            { SDL_MOUSEMOTION, {{0}}, {{0}}, 100, 100 },
            
            // GUI input (mouse in GUI area)
            { SDL_MOUSEBUTTONDOWN, {{0}}, {{SDL_BUTTON_LEFT}}, 75, 75 },
            
            // Global hotkey
            { SDL_KEYDOWN, {{0}}, {{SDLK_F1}}, 0, 0 },
            
            // More game input
            { SDL_KEYDOWN, {{0}}, {{SDLK_a}}, 0, 0 }
        };
        
        for (const auto& event : events) {
            bool consumed = manager.ProcessEvent(event);
            std::cout << "Event " << static_cast<int>(event.type) 
                     << " consumed: " << (consumed ? "Yes" : "No") << std::endl;
        }
    }
};

} // namespace Examples
} // namespace GUI
} // namespace BN

// Main function to run examples
int main(int argc, char* argv[]) {
    std::cout << "InputManager Integration Examples" << std::endl;
    std::cout << "=================================" << std::endl;
    
    try {
        // Run basic example
        BN::GUI::Examples::BasicInputExample basic_example;
        basic_example.Run();
        
        // Run context example
        BN::GUI::Examples::ContextExample context_example;
        context_example.Run();
        
        // Run game loop example
        BN::GUI::Examples::GameLoopIntegration game_loop_example;
        game_loop_example.Run();
        
        std::cout << "\nAll examples completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Example failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}