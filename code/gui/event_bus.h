#ifndef GUI_EVENT_BUS_H
#define GUI_EVENT_BUS_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace cataclysm {
namespace gui {

/**
 * Base class for all events in the GUI system.
 * All specific event types should inherit from this class.
 */
class Event {
public:
    virtual ~Event() = default;
    virtual std::string getEventType() const = 0;
    virtual std::unique_ptr<Event> clone() const = 0;
};

/**
 * Event subscription handle for unsubscribing.
 * Created when subscribing to an event and can be used to remove the subscription.
 */
class EventSubscription {
public:
    EventSubscription() : active_(false), id_(0) {}
    explicit EventSubscription(std::function<void()> unsubscribe_func) 
        : active_(true), id_(0), unsubscribe_func_(std::move(unsubscribe_func)) {}
    
    void unsubscribe() {
        std::function<void()> unsubscribe_copy;
        if (active_ && unsubscribe_func_) {
            active_ = false;
            unsubscribe_copy = std::move(unsubscribe_func_);
        }
        if (unsubscribe_copy) {
            unsubscribe_copy();
        }
    }

    bool isActive() const { return active_; }
    void setId(size_t id) { id_ = id; }
    size_t getId() const { return id_; }

    void deactivate() {
        active_ = false;
        unsubscribe_func_ = nullptr;
    }

private:
    std::atomic<bool> active_;
    size_t id_;
    std::function<void()> unsubscribe_func_;
};

/**
 * Template for event subscriptions with type safety.
 * Manages the lifetime of event subscriptions and provides easy unsubscription.
 */
template<typename EventType>
class TypedEventSubscription : public EventSubscription {
public:
    TypedEventSubscription(std::function<void(const EventType&)> callback, std::function<void()> unsubscribe_func)
        : EventSubscription(std::move(unsubscribe_func)), callback_(std::move(callback)) {}
    
    void invoke(const EventType& event) {
        if (isActive() && callback_) {
            callback_(event);
        }
    }
    
private:
    std::function<void(const EventType&)> callback_;
};

/**
 * Core event bus implementation using observer pattern.
 * Provides thread-safe publish/subscribe mechanism for loose coupling between components.
 */
class EventBus {
public:
    EventBus();
    ~EventBus();
    
    // Delete copy and move operations for safety
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;
    
    /**
     * Subscribe to events of type EventType.
     * @param callback Function to call when event is published
     * @return EventSubscription handle for unsubscribing
     */
    template<typename EventType, typename Callable>
    std::shared_ptr<EventSubscription> subscribe(Callable&& callback) {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        auto subscription = std::make_shared<TypedEventSubscription<EventType>>(
            std::forward<Callable>(callback),
            [this, type_index = std::type_index(typeid(EventType))]() {
                unsubscribeInternal(type_index);
            }
        );
        
        size_t subscription_id = next_subscription_id_++;
        subscription->setId(subscription_id);
        
        auto& event_subscriptions = subscriptions_[std::type_index(typeid(EventType))];
        event_subscriptions.push_back(subscription);
        
        return subscription;
    }
    
    /**
     * Publish an event to all subscribers.
     * @param event Event to publish
     */
    template<typename EventType>
    void publish(const EventType& event) {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        
        auto it = subscriptions_.find(std::type_index(typeid(EventType)));
        if (it != subscriptions_.end()) {
            // Create a copy of subscriptions to avoid issues with unsubscription during iteration
            auto subscriptions_copy = it->second;
            
            for (const auto& subscription : subscriptions_copy) {
                if (subscription->isActive()) {
                    auto typed_subscription = std::static_pointer_cast<TypedEventSubscription<EventType>>(subscription);
                    typed_subscription->invoke(event);
                }
            }
            
            // Clean up inactive subscriptions
            it->second.erase(
                std::remove_if(it->second.begin(), it->second.end(), 
                              [](const auto& sub) { return !sub->isActive(); }),
                it->second.end()
            );
            
            // Remove empty subscription lists
            if (it->second.empty()) {
                subscriptions_.erase(it);
            }
        }
    }
    
    /**
     * Publish a dynamically typed event.
     * @param event Event to publish
     */
    void publishDynamic(const std::unique_ptr<Event>& event);
    
    /**
     * Unsubscribe all subscriptions of a specific type.
     * @tparam EventType Type of events to unsubscribe
     */
    template<typename EventType>
    void unsubscribe() {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        unsubscribeInternal(std::type_index(typeid(EventType)));
    }
    
    /**
     * Clear all subscriptions.
     */
    void clearAll();
    
    /**
     * Get the number of active subscriptions.
     * @return Total number of active subscriptions
     */
    size_t getSubscriptionCount() const;
    
    /**
     * Get the number of subscriptions for a specific event type.
     * @tparam EventType Type of events to count
     * @return Number of subscriptions for the event type
     */
    template<typename EventType>
    size_t getSubscriptionCount() const {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(std::type_index(typeid(EventType)));
        if (it != subscriptions_.end()) {
            return std::count_if(it->second.begin(), it->second.end(),
                               [](const auto& sub) { return sub->isActive(); });
        }
        return 0;
    }
    
    /**
     * Check if the event bus is empty (no active subscriptions).
     * @return true if no active subscriptions exist
     */
    bool isEmpty() const;

private:
    void unsubscribeInternal(const std::type_index& event_type);
    
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<EventSubscription>>> subscriptions_;
    mutable std::mutex subscriptions_mutex_;
    std::atomic<size_t> next_subscription_id_{0};
};

/**
 * Global event bus instance.
 * Provides a centralized event communication system for the entire GUI system.
 */
class EventBusManager {
public:
    /**
     * Get the global event bus instance.
     * @return Reference to the global event bus
     */
    static EventBus& getGlobalEventBus();
    
    /**
     * Initialize the global event bus (called at startup).
     */
    static void initialize();
    
    /**
     * Shutdown the global event bus (called at shutdown).
     */
    static void shutdown();

private:
    static std::unique_ptr<EventBus> global_event_bus_;
    static std::atomic<bool> initialized_;
};

} // namespace gui
} // namespace cataclysm

// Macro helpers for common event bus operations
#define GUI_EVENT_BUS_SUBSCRIBE(bus, EventType, callback) \
    (bus).subscribe<EventType>(callback)

#define GUI_EVENT_BUS_PUBLISH(bus, event) \
    (bus).publish(event)

#endif // GUI_EVENT_BUS_H