#ifndef GUI_INPUT_MANAGER_H
#define GUI_INPUT_MANAGER_H

#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <SDL.h>
#include "gui_settings.h"

namespace BN {
namespace GUI {

// Forward declarations
class InputContext;
class GUIEvent;

/**
 * @brief Manages dual input handling (keyboard and mouse) for the GUI system
 * 
 * Coordinates between existing keyboard input and new mouse input, routing events
 * to appropriate handlers (GUI vs game) while maintaining input priority and
 * focus management. Integrates with Cataclysm-BN's existing input.cpp system.
 */
class InputManager {
public:
    // Input event types
    enum class EventType {
        KEYBOARD_PRESS,
        KEYBOARD_RELEASE,
        MOUSE_MOVE,
        MOUSE_BUTTON_PRESS,
        MOUSE_BUTTON_RELEASE,
        MOUSE_WHEEL,
        TEXT_INPUT,
        FOCUS_GAINED,
        FOCUS_LOST
    };

    // Input priority levels
    enum class Priority {
        LOWEST = 0,
        LOW = 1,
        NORMAL = 2,
        HIGH = 3,
        HIGHEST = 4
    };

    // Focus state
    enum class FocusState {
        NONE,
        GUI,
        GAME,
        SHARED
    };

    // Event handler signature
    using EventHandler = std::function<bool(const GUIEvent& event)>;
    
    // Event listener for focus changes
    using FocusListener = std::function<void(FocusState previous, FocusState current)>;

    struct InputSettings {
        bool enable_mouse = true;
        bool enable_keyboard = true;
        bool pass_through_enabled = true;
        bool prevent_game_input_when_gui_focused = true;
        Priority default_priority = Priority::NORMAL;
        int max_mouse_sensitivity = 100;
        int mouse_sensitivity = 50;
        bool mouse_relative_mode = false;
        bool focus_indicator_enabled = true;
    };

    /**
     * @brief Constructor
     * @param settings Initial input settings
     */
    explicit InputManager(const InputSettings& settings = InputSettings());

    /**
     * @brief Destructor
     */
    ~InputManager();

    // Non-copyable, non-movable
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    /**
     * @brief Initialize the input manager
     * @return true if initialization successful
     */
    bool Initialize();

    /**
     * @brief Shutdown the input manager and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Process an SDL event and route it appropriately
     * @param event SDL event to process
     * @return true if event was consumed by GUI
     */
    bool ProcessEvent(const SDL_Event& event);

    /**
     * @brief Register an event handler for a specific input type
     * @param type Event type to handle
     * @param handler Event handler function
     * @param priority Priority level (higher = processed first)
     * @param context Optional context identifier
     * @return Handler ID for later removal
     */
    int RegisterHandler(EventType type, EventHandler handler, Priority priority = Priority::NORMAL, const std::string& context = "");

    /**
     * @brief Remove an event handler
     * @param handler_id ID returned from RegisterHandler
     */
    void UnregisterHandler(int handler_id);

    /**
     * @brief Set the current input context
     * @param context_name Name of the context
     * @param context Input context object
     */
    void SetInputContext(const std::string& context_name, std::unique_ptr<InputContext> context);

    /**
     * @brief Remove an input context
     * @param context_name Name of the context to remove
     */
    void RemoveInputContext(const std::string& context_name);

    /**
     * @brief Get the current input context
     * @return Current input context or nullptr
     */
    InputContext* GetCurrentContext() const;

    /**
     * @brief Set focus state
     * @param focus New focus state
     * @param reason Reason for focus change
     */
    void SetFocusState(FocusState focus, const std::string& reason = "");

    /**
     * @brief Get current focus state
     * @return Current focus state
     */
    FocusState GetFocusState() const { return current_focus_state_.load(); }

    /**
     * @brief Add a focus change listener
     * @param listener Listener function
     * @return Listener ID for later removal
     */
    int AddFocusListener(FocusListener listener);

    /**
     * @brief Remove a focus change listener
     * @param listener_id ID returned from AddFocusListener
     */
    void RemoveFocusListener(int listener_id);

    /**
     * @brief Update input settings
     * @param settings New settings
     */
    void UpdateSettings(const InputSettings& settings);

    /**
     * @brief Get current input settings
     * @return Current input settings
     */
    const InputSettings& GetSettings() const { return settings_; }

    /**
     * @brief Check if GUI should consume this event
     * @param event SDL event to check
     * @return true if GUI should consume the event
     */
    bool ShouldConsumeEvent(const SDL_Event& event) const;

    /**
     * @brief Get mouse position (screen coordinates)
     * @param x Output X coordinate
     * @param y Output Y coordinate
     */
    void GetMousePosition(int& x, int& y) const;

    /**
     * @brief Get mouse position relative to last position
     * @param x Output X delta
     * @param y Output Y delta
     */
    void GetMouseDelta(int& x, int& y) const;

    /**
     * @brief Check if mouse is over GUI area
     * @return true if mouse is over GUI
     */
    bool IsMouseOverGUI() const;

    /**
     * @brief Set GUI area bounds for focus detection
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Width
     * @param height Height
     */
    void SetGUIAreaBounds(int x, int y, int width, int height);

    /**
     * @brief Enable or disable input processing
     * @param enabled New enabled state
     */
    void SetEnabled(bool enabled) { enabled_.store(enabled); }

    /**
     * @brief Check if input processing is enabled
     * @return true if enabled
     */
    bool IsEnabled() const { return enabled_.load(); }

    /**
     * @brief Get statistics for debugging/performance monitoring
     */
    struct Statistics {
        std::atomic<uint64_t> events_processed{0};
        std::atomic<uint64_t> events_consumed{0};
        std::atomic<uint64_t> events_passed_through{0};
        std::atomic<uint64_t> handlers_called{0};
        std::atomic<uint32_t> active_handlers{0};
        std::atomic<uint32_t> focus_changes{0};
    };

    /**
     * @brief Get current statistics
     * @return Statistics structure
     */
    Statistics GetStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void ResetStatistics();

private:
    // Internal implementation
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Settings and state
    InputSettings settings_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> enabled_{true};
    std::atomic<FocusState> current_focus_state_{FocusState::NONE};
    std::atomic<int> next_handler_id_{1};
    std::atomic<int> next_listener_id_{1};

    // Thread safety
    mutable std::mutex handlers_mutex_;
    mutable std::mutex context_mutex_;
    mutable std::mutex listeners_mutex_;
    mutable std::mutex statistics_mutex_;

    // Internal event routing
    bool ProcessKeyboardEvent(const SDL_Event& event);
    bool ProcessMouseEvent(const SDL_Event& event);
    bool RouteEventToHandlers(const GUIEvent& event);
    bool RouteToHandlers(const GUIEvent& event);
    bool HasEnabledHandlersForEvent(const GUIEvent& event) const;
    
    // Focus management
    void NotifyFocusListeners(FocusState previous, FocusState current);
    
    // Helper methods
    EventType SDLToEventType(const SDL_Event& event) const;
    Priority DetermineEventPriority(const SDL_Event& event) const;
    bool IsEventConsumedByGUI(const GUIEvent& event) const;
};

/**
 * @brief GUI Event wrapper for internal use
 */
struct GUIEvent {
    InputManager::EventType type;
    SDL_Event sdl_event;
    InputManager::Priority priority;
    std::string context;
    int64_t timestamp_ms;
    bool consumed = false;

    GUIEvent(InputManager::EventType t, const SDL_Event& sdl, InputManager::Priority p = InputManager::Priority::NORMAL)
        : type(t), sdl_event(sdl), priority(p), timestamp_ms(SDL_GetTicks64()) {}
};

/**
 * @brief Input context interface
 * 
 * Represents a context for input handling, similar to BN's input_context system
 */
class InputContext {
public:
    virtual ~InputContext() = default;
    
    /**
     * @brief Handle an input event
     * @param event Event to handle
     * @return true if event was consumed
     */
    virtual bool HandleEvent(const GUIEvent& event) = 0;
    
    /**
     * @brief Get context name
     * @return Context name
     */
    virtual const std::string& GetName() const = 0;
    
    /**
     * @brief Get context priority
     * @return Priority level
     */
    virtual InputManager::Priority GetPriority() const = 0;
    
    /**
     * @brief Check if this context should receive the event
     * @param event Event to check
     * @return true if this context should handle the event
     */
    virtual bool ShouldReceiveEvent(const GUIEvent& event) const = 0;
};

} // namespace GUI
} // namespace BN

#endif // GUI_INPUT_MANAGER_H