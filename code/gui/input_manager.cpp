#include "input_manager.h"
#include <algorithm>
#include <cassert>
#include <unordered_set>
#include "debug.h"

namespace BN {
namespace GUI {

// Forward declarations for internal classes
class InputManager::Impl {
public:
    Impl(InputManager* manager) : manager_(manager) {}
    
    struct HandlerInfo {
        int id;
        EventType type;
        EventHandler handler;
        Priority priority;
        std::string context;
        bool enabled;
    };
    
    // Member variables
    InputManager* manager_;
    std::unordered_map<int, HandlerInfo> handlers_;
    std::unordered_map<std::string, std::unique_ptr<InputContext>> contexts_;
    std::string current_context_name_;
    std::vector<FocusListener> focus_listeners_;
    
    // Mouse state
    int mouse_x_ = 0;
    int mouse_y_ = 0;
    int prev_mouse_x_ = 0;
    int prev_mouse_y_ = 0;
    
    // GUI area bounds
    int gui_area_x_ = 0;
    int gui_area_y_ = 0;
    int gui_area_width_ = 0;
    int gui_area_height_ = 0;
    bool gui_area_defined_ = false;
    
    // Event queue for thread safety
    std::vector<GUIEvent> event_queue_;
    std::mutex event_queue_mutex_;
    std::condition_variable event_queue_cv_;
    std::atomic<bool> processing_events_{false};
    
    // Statistics
    Statistics stats_;
    std::mutex stats_mutex_;
    
    // Methods
    bool ProcessEventInternal(const SDL_Event& event);
    bool ProcessKeyboardEventInternal(const SDL_Event& event);
    bool ProcessMouseEventInternal(const SDL_Event& event);
    void RouteEventToHandlers(const GUIEvent& event);
    void UpdateMouseState(const SDL_Event& event);
    bool IsMouseInGUIArea(int x, int y) const;
    void AddEventToQueue(const GUIEvent& event);
};

InputManager::InputManager(const InputSettings& settings)
    : settings_(settings), impl_(std::make_unique<Impl>(this)) {
    debuglog(DebugLevel::Info, "InputManager: Constructor called");
}

InputManager::~InputManager() {
    if (initialized_.load()) {
        Shutdown();
    }
    debuglog(DebugLevel::Info, "InputManager: Destructor called");
}

bool InputManager::Initialize() {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
    
    if (initialized_.load()) {
        debuglog(DebugLevel::Warning, "InputManager: Already initialized");
        return true;
    }
    
    debuglog(DebugLevel::Info, "InputManager: Initializing...");
    
    // Initialize mouse position
    SDL_GetMouseState(&impl_->mouse_x_, &impl_->mouse_y_);
    impl_->prev_mouse_x_ = impl_->mouse_x_;
    impl_->prev_mouse_y_ = impl_->mouse_y_;
    
    // Set default focus state
    current_focus_state_.store(FocusState::GAME);
    
    initialized_.store(true);
    debuglog(DebugLevel::Info, "InputManager: Initialization complete");
    
    return true;
}

void InputManager::Shutdown() {
    if (!initialized_.load()) {
        return;
    }
    
    debuglog(DebugLevel::Info, "InputManager: Shutting down...");
    
    // Clear event queue
    {
        std::lock_guard<std::mutex> lock(impl_->event_queue_mutex_);
        impl_->event_queue_.clear();
    }
    
    // Clear handlers and contexts
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        impl_->handlers_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(context_mutex_);
        impl_->contexts_.clear();
        impl_->current_context_name_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        impl_->focus_listeners_.clear();
    }
    
    initialized_.store(false);
    debuglog(DebugLevel::Info, "InputManager: Shutdown complete");
}

bool InputManager::ProcessEvent(const SDL_Event& event) {
    if (!initialized_.load() || !enabled_.load()) {
        return false;
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
        impl_->stats_.events_processed++;
    }
    
    // Process event based on type
    bool consumed = false;
    
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        consumed = ProcessKeyboardEvent(event);
    } else if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || 
               event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL) {
        consumed = ProcessMouseEvent(event);
    } else if (event.type == SDL_TEXTINPUT) {
        // Handle text input
        GUIEvent text_event(EventType::TEXT_INPUT, event, Priority::NORMAL);
        consumed = RouteEventToHandlers(text_event);
    }
    
    // Update statistics
    if (consumed) {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
        impl_->stats_.events_consumed++;
    } else if (settings_.pass_through_enabled && current_focus_state_.load() != FocusState::GUI) {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
        impl_->stats_.events_passed_through++;
    }
    
    return consumed;
}

bool InputManager::ProcessKeyboardEvent(const SDL_Event& event) {
    EventType type = (event.type == SDL_KEYDOWN) ? EventType::KEYBOARD_PRESS : EventType::KEYBOARD_RELEASE;
    GUIEvent gui_event(type, event, DetermineEventPriority(event));
    
    return RouteEventToHandlers(gui_event);
}

bool InputManager::ProcessMouseEvent(const SDL_Event& event) {
    // Update mouse state
    impl_->UpdateMouseState(event);
    
    EventType type;
    switch (event.type) {
        case SDL_MOUSEMOTION:
            type = EventType::MOUSE_MOVE;
            break;
        case SDL_MOUSEBUTTONDOWN:
            type = EventType::MOUSE_BUTTON_PRESS;
            break;
        case SDL_MOUSEBUTTONUP:
            type = EventType::MOUSE_BUTTON_RELEASE;
            break;
        case SDL_MOUSEWHEEL:
            type = EventType::MOUSE_WHEEL;
            break;
        default:
            return false;
    }
    
    GUIEvent gui_event(type, event, DetermineEventPriority(event));
    return RouteEventToHandlers(gui_event);
}

bool InputManager::RouteEventToHandlers(const GUIEvent& event) {
    if (event.consumed) {
        return true; // Already consumed
    }

    if (!HasEnabledHandlersForEvent(event)) {
        return false;
    }

    FocusState current_focus = current_focus_state_.load();

    switch (current_focus) {
        case FocusState::GUI: {
            if (!settings_.prevent_game_input_when_gui_focused && !IsEventConsumedByGUI(event)) {
                return false; // GUI allows game input and no matching handler wants the event
            }
            return RouteToHandlers(event);
        }
        case FocusState::GAME: {
            // Game has focus, pass through unless event is in GUI area and should be handled
            if (event.type == EventType::MOUSE_MOVE || event.type == EventType::MOUSE_BUTTON_PRESS ||
                event.type == EventType::MOUSE_BUTTON_RELEASE || event.type == EventType::MOUSE_WHEEL) {
                if (impl_->IsMouseInGUIArea(impl_->mouse_x_, impl_->mouse_y_) && IsEventConsumedByGUI(event)) {
                    return RouteToHandlers(event);
                }
            } else if (!settings_.pass_through_enabled && IsEventConsumedByGUI(event)) {
                // Pass-through disabled allows GUI handlers to run even when game has focus
                return RouteToHandlers(event);
            }
            return false; // Pass to game
        }
        case FocusState::SHARED: {
            bool consumed = RouteToHandlers(event);
            if (!consumed && settings_.pass_through_enabled) {
                // Pass unconsumed events to game
                return false;
            }
            return consumed;
        }
        case FocusState::NONE:
        default:
            break;
    }

    return false;
}

bool InputManager::RouteToHandlers(const GUIEvent& event) {
    std::vector<HandlerInfo*> handlers_to_call;

    // Collect handlers in priority order
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        for (auto& [id, handler_info] : impl_->handlers_) {
            if (handler_info.enabled && handler_info.type == event.type && handler_info.priority >= event.priority) {
                // Check context filter
                if (!event.context.empty() && !handler_info.context.empty() &&
                    event.context != handler_info.context) {
                    continue;
                }
                handlers_to_call.push_back(&handler_info);
            }
        }
    }
    
    // Sort by priority (higher first)
    std::sort(handlers_to_call.begin(), handlers_to_call.end(),
              [](const HandlerInfo* a, const HandlerInfo* b) {
                  return a->priority > b->priority;
              });
    
    // Call handlers
    for (HandlerInfo* handler_info : handlers_to_call) {
        try {
            {
                std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
                impl_->stats_.handlers_called++;
            }
            
            if (handler_info->handler(event)) {
                return true; // Event consumed
            }
        } catch (const std::exception& e) {
            debuglog(DebugLevel::Error, "InputManager: Exception in event handler: ", e.what());
        }
    }
    
    return false; // Not consumed
}

bool InputManager::HasEnabledHandlersForEvent(const GUIEvent& event) const {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    for (const auto& [id, handler_info] : impl_->handlers_) {
        if (!handler_info.enabled) {
            continue;
        }

        if (handler_info.type != event.type) {
            continue;
        }

        if (handler_info.priority < event.priority) {
            continue;
        }

        if (!event.context.empty() && !handler_info.context.empty() && event.context != handler_info.context) {
            continue;
        }

        return true;
    }

    return false;
}

int InputManager::RegisterHandler(EventType type, EventHandler handler, Priority priority, const std::string& context) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    int id = next_handler_id_.fetch_add(1);

    HandlerInfo info;
    info.id = id;
    info.type = type;
    info.handler = handler;
    info.priority = priority;
    info.context = context;
    info.enabled = true;

    impl_->handlers_[id] = std::move(info);
    
    {
        std::lock_guard<std::mutex> stats_lock(impl_->stats_mutex_);
        auto enabled_count = std::count_if(
            impl_->handlers_.begin(),
            impl_->handlers_.end(),
            [](const auto& entry) {
                return entry.second.enabled;
            }
        );
        impl_->stats_.active_handlers = static_cast<uint32_t>(enabled_count);
    }

    debuglog(DebugLevel::Info, "InputManager: Registered handler ", id, " for type ", static_cast<int>(type));
    return id;
}

void InputManager::UnregisterHandler(int handler_id) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    
    if (impl_->handlers_.erase(handler_id) > 0) {
        {
            std::lock_guard<std::mutex> stats_lock(impl_->stats_mutex_);
            auto enabled_count = std::count_if(
                impl_->handlers_.begin(),
                impl_->handlers_.end(),
                [](const auto& entry) {
                    return entry.second.enabled;
                }
            );
            impl_->stats_.active_handlers = static_cast<uint32_t>(enabled_count);
        }
        debuglog(DebugLevel::Info, "InputManager: Unregistered handler ", handler_id);
    }
}

void InputManager::SetInputContext(const std::string& context_name, std::unique_ptr<InputContext> context) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    
    impl_->contexts_[context_name] = std::move(context);
    debuglog(DebugLevel::Info, "InputManager: Set input context '", context_name, "'");
}

void InputManager::RemoveInputContext(const std::string& context_name) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    
    if (impl_->contexts_.erase(context_name) > 0) {
        if (impl_->current_context_name_ == context_name) {
            impl_->current_context_name_.clear();
        }
        debuglog(DebugLevel::Info, "InputManager: Removed input context '", context_name, "'");
    }
}

InputContext* InputManager::GetCurrentContext() const {
    std::lock_guard<std::mutex> lock(context_mutex_);
    
    if (impl_->current_context_name_.empty()) {
        return nullptr;
    }
    
    auto it = impl_->contexts_.find(impl_->current_context_name_);
    if (it != impl_->contexts_.end()) {
        return it->second.get();
    }
    
    return nullptr;
}

void InputManager::SetFocusState(FocusState focus, const std::string& reason) {
    FocusState previous = current_focus_state_.exchange(focus);
    
    if (previous != focus) {
        {
            std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
            impl_->stats_.focus_changes++;
        }
        
        debuglog(DebugLevel::Info, "InputManager: Focus changed from ", static_cast<int>(previous), 
                " to ", static_cast<int>(focus), " (", reason, ")");
        
        NotifyFocusListeners(previous, focus);
    }
}

void InputManager::NotifyFocusListeners(FocusState previous, FocusState current) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    
    for (const auto& listener : impl_->focus_listeners_) {
        try {
            listener(previous, current);
        } catch (const std::exception& e) {
            debuglog(DebugLevel::Error, "InputManager: Exception in focus listener: ", e.what());
        }
    }
}

int InputManager::AddFocusListener(FocusListener listener) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    
    int id = next_listener_id_.fetch_add(1);
    impl_->focus_listeners_.push_back(listener);
    
    debuglog(DebugLevel::Info, "InputManager: Added focus listener ", id);
    return id;
}

void InputManager::RemoveFocusListener(int listener_id) {
    std::lock_guard<std::mutex> lock(listeners_mutex_);
    
    // Note: In a production implementation, you'd want a more sophisticated
    // listener management system. For now, we just log that removal was requested.
    debuglog(DebugLevel::Info, "InputManager: Remove focus listener ", listener_id, 
            " (implementation note: listeners are cleared on shutdown)");
}

void InputManager::UpdateSettings(const InputSettings& settings) {
    settings_ = settings;
    debuglog(DebugLevel::Info, "InputManager: Settings updated");
}

bool InputManager::ShouldConsumeEvent(const SDL_Event& event) const {
    if (!initialized_.load() || !enabled_.load()) {
        return false;
    }

    GUIEvent gui_event(SDLToEventType(event), event, DetermineEventPriority(event));

    // If GUI does not currently have focus and pass-through is enabled, we
    // allow the event to continue to the game systems.
    FocusState focus = current_focus_state_.load();
    if (focus == FocusState::GAME && settings_.pass_through_enabled) {
        return false;
    }

    // Shared focus respects pass-through behaviour but still allows the GUI
    // layer to consume mouse/keyboard when it overlaps GUI regions.
    if (focus == FocusState::SHARED && settings_.pass_through_enabled) {
        return IsEventConsumedByGUI(gui_event);
    }

    return IsEventConsumedByGUI(gui_event);
}

InputManager::Statistics InputManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
    return impl_->stats_;
}

void InputManager::ResetStatistics() {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex_);
    impl_->stats_ = Statistics{};
    debuglog(DebugLevel::Info, "InputManager: Statistics reset");
}

void InputManager::GetMousePosition(int& x, int& y) const {
    x = impl_->mouse_x_;
    y = impl_->mouse_y_;
}

void InputManager::GetMouseDelta(int& x, int& y) const {
    x = impl_->mouse_x_ - impl_->prev_mouse_x_;
    y = impl_->mouse_y_ - impl_->prev_mouse_y_;
}

bool InputManager::IsMouseOverGUI() const {
    return impl_->IsMouseInGUIArea(impl_->mouse_x_, impl_->mouse_y_);
}

void InputManager::SetGUIAreaBounds(int x, int y, int width, int height) {
    impl_->gui_area_x_ = x;
    impl_->gui_area_y_ = y;
    impl_->gui_area_width_ = width;
    impl_->gui_area_height_ = height;
    impl_->gui_area_defined_ = true;
    
    debuglog(DebugLevel::Info, "InputManager: Set GUI area bounds: ", x, ",", y, " ", width, "x", height);
}

InputManager::EventType InputManager::SDLToEventType(const SDL_Event& event) const {
    switch (event.type) {
        case SDL_KEYDOWN: return EventType::KEYBOARD_PRESS;
        case SDL_KEYUP: return EventType::KEYBOARD_RELEASE;
        case SDL_MOUSEMOTION: return EventType::MOUSE_MOVE;
        case SDL_MOUSEBUTTONDOWN: return EventType::MOUSE_BUTTON_PRESS;
        case SDL_MOUSEBUTTONUP: return EventType::MOUSE_BUTTON_RELEASE;
        case SDL_MOUSEWHEEL: return EventType::MOUSE_WHEEL;
        case SDL_TEXTINPUT: return EventType::TEXT_INPUT;
        default: return EventType::KEYBOARD_PRESS;
    }
}

InputManager::Priority InputManager::DetermineEventPriority(const SDL_Event& event) const {
    // High priority events
    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        return Priority::HIGH;
    }
    
    // Medium priority for mouse movement (can be throttled)
    if (event.type == SDL_MOUSEMOTION) {
        return Priority::NORMAL;
    }
    
    // Text input has high priority for immediate response
    if (event.type == SDL_TEXTINPUT) {
        return Priority::HIGH;
    }
    
    return Priority::NORMAL;
}

bool InputManager::IsEventConsumedByGUI(const GUIEvent& event) const {
    if (!HasEnabledHandlersForEvent(event)) {
        return false;
    }

    // For mouse events, check if mouse is in GUI area
    if (event.type == EventType::MOUSE_MOVE || event.type == EventType::MOUSE_BUTTON_PRESS ||
        event.type == EventType::MOUSE_BUTTON_RELEASE || event.type == EventType::MOUSE_WHEEL) {
        int mouse_x = impl_->mouse_x_;
        int mouse_y = impl_->mouse_y_;

        switch (event.sdl_event.type) {
            case SDL_MOUSEMOTION:
                mouse_x = event.sdl_event.motion.x;
                mouse_y = event.sdl_event.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                mouse_x = event.sdl_event.button.x;
                mouse_y = event.sdl_event.button.y;
                break;
            case SDL_MOUSEWHEEL: {
                int wheel_x = 0;
                int wheel_y = 0;
                SDL_GetMouseState(&wheel_x, &wheel_y);
                mouse_x = wheel_x;
                mouse_y = wheel_y;
                break;
            }
            default:
                break;
        }

        return impl_->IsMouseInGUIArea(mouse_x, mouse_y);
    }

    // For non-mouse events, we have matching handlers
    return true;
}

// Implementation helper methods
bool InputManager::Impl::IsMouseInGUIArea(int x, int y) const {
    if (!gui_area_defined_) {
        return false; // No GUI area defined, can't be over GUI
    }
    
    return (x >= gui_area_x_ && x < gui_area_x_ + gui_area_width_ &&
            y >= gui_area_y_ && y < gui_area_y_ + gui_area_height_);
}

void InputManager::Impl::UpdateMouseState(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        prev_mouse_x_ = mouse_x_;
        prev_mouse_y_ = mouse_y_;
        mouse_x_ = event.motion.x;
        mouse_y_ = event.motion.y;
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        mouse_x_ = event.button.x;
        mouse_y_ = event.button.y;
    } else if (event.type == SDL_MOUSEWHEEL) {
        // Mouse wheel events use the current mouse position
        int temp_x, temp_y;
        SDL_GetMouseState(&temp_x, &temp_y);
        mouse_x_ = temp_x;
        mouse_y_ = temp_y;
    }
}

} // namespace GUI
} // namespace BN