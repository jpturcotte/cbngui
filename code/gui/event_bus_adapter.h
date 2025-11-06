#ifndef GUI_EVENT_BUS_ADAPTER_H
#define GUI_EVENT_BUS_ADAPTER_H

#include "event_bus.h"
#include "events.h"
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace cataclysm {
namespace gui {

/**
 * EventBusAdapter provides a clean interface for GUI components to interact
 * with the game event system while maintaining loose coupling.
 * 
 * This class serves as a bridge between overlay UI components and gameplay systems,
 * handling the publication of UI events and subscription to gameplay state changes.
 */
class EventBusAdapter {
public:
    EventBusAdapter();
    explicit EventBusAdapter(EventBus& event_bus);
    ~EventBusAdapter();
    
    // Delete copy and move operations
    EventBusAdapter(const EventBusAdapter&) = delete;
    EventBusAdapter& operator=(const EventBusAdapter&) = delete;
    EventBusAdapter(EventBusAdapter&&) = delete;
    EventBusAdapter& operator=(EventBusAdapter&&) = delete;
    
    /**
     * Initialize the event bus adapter.
     * Should be called when the overlay system starts up.
     */
    void initialize();
    
    /**
     * Shutdown the event bus adapter.
     * Should be called when the overlay system shuts down.
     */
    void shutdown();
    
    // =============================================================================
    // UI Event Publishing Methods
    // =============================================================================
    
    /**
     * Publish an overlay open event.
     * Called when a GUI overlay becomes active.
     * @param overlay_id Unique identifier for the overlay
     * @param is_modal Whether the overlay is modal
     */
    void publishOverlayOpen(const std::string& overlay_id, bool is_modal = false);
    
    /**
     * Publish an overlay close event.
     * Called when a GUI overlay becomes inactive.
     * @param overlay_id Unique identifier for the overlay
     * @param was_cancelled Whether the overlay was cancelled
     */
    void publishOverlayClose(const std::string& overlay_id, bool was_cancelled = false);
    
    /**
     * Publish a filter applied event.
     * Called when a filter is applied to a UI component.
     * @param filter_text The filter text
     * @param target_component The component that should receive the filter
     * @param case_sensitive Whether the filter is case sensitive
     */
    void publishFilterApplied(const std::string& filter_text, 
                            const std::string& target_component,
                            bool case_sensitive = false);
    
    /**
     * Publish an item selected event.
     * Called when an item is selected in a GUI component.
     * @param item_id Unique identifier for the selected item
     * @param source_component The component where the item was selected
     * @param is_double_click Whether this was a double-click selection
     * @param item_count Number of items selected
     */
    void publishItemSelected(const std::string& item_id,
                           const std::string& source_component,
                           bool is_double_click = false,
                           int item_count = 1);
    
    /**
     * Publish a data binding update event.
     * Called when UI data bindings need to be refreshed.
     * @param binding_id Unique identifier for the data binding
     * @param data_source The data source that changed
     * @param forced Whether this is a forced update
     */
    void publishDataBindingUpdate(const std::string& binding_id,
                                const std::string& data_source,
                                bool forced = false);
    
    // =============================================================================
    // Gameplay Event Subscription Methods
    // =============================================================================
    
    /**
     * Subscribe to gameplay status change events.
     * @param callback Function to call when status changes occur
     * @return Subscription handle for unsubscribing
     */
    std::shared_ptr<EventSubscription> subscribeToStatusChange(
        std::function<void(const GameplayStatusChangeEvent&)> callback);
    
    /**
     * Subscribe to gameplay inventory change events.
     * @param callback Function to call when inventory changes occur
     * @return Subscription handle for unsubscribing
     */
    std::shared_ptr<EventSubscription> subscribeToInventoryChange(
        std::function<void(const GameplayInventoryChangeEvent&)> callback);
    
    /**
     * Subscribe to gameplay notice events.
     * @param callback Function to call when notices are published
     * @return Subscription handle for unsubscribing
     */
    std::shared_ptr<EventSubscription> subscribeToGameplayNotice(
        std::function<void(const GameplayNoticeEvent&)> callback);
    
    // =============================================================================
    // Generic Event Handling Methods
    // =============================================================================
    
    /**
     * Subscribe to any event type.
     * @tparam EventType The type of event to subscribe to
     * @param callback Function to call when the event occurs
     * @return Subscription handle for unsubscribing
     */
    template<typename EventType>
    std::shared_ptr<EventSubscription> subscribe(
        std::function<void(const EventType&)> callback) {
        return event_bus_.subscribe<EventType>(std::move(callback));
    }
    
    /**
     * Publish any event type.
     * @tparam EventType The type of event to publish
     * @param event The event to publish
     */
    template<typename EventType>
    void publish(const EventType& event) {
        event_bus_.publish(event);
    }
    
    // =============================================================================
    // Utility Methods
    // =============================================================================
    
    /**
     * Get the number of active subscriptions.
     * @return Total number of active event subscriptions
     */
    size_t getSubscriptionCount() const;
    
    /**
     * Get the number of subscriptions for a specific event type.
     * @tparam EventType The type of event to count
     * @return Number of subscriptions for the event type
     */
    template<typename EventType>
    size_t getSubscriptionCount() const {
        return event_bus_.getSubscriptionCount<EventType>();
    }
    
    /**
     * Check if the event bus adapter has any active subscriptions.
     * @return true if no active subscriptions exist
     */
    bool isEmpty() const;
    
    /**
     * Clear all subscriptions managed by this adapter.
     * This is useful during shutdown or when the GUI overlay is being closed.
     */
    void clearAllSubscriptions();
    
    /**
     * Get statistics about event system performance.
     * @return Map of statistics about the event system
     */
    std::unordered_map<std::string, int> getStatistics() const;

private:
    void setupDefaultSubscriptions();
    void cleanupSubscriptions();
    
    EventBus& event_bus_;
    std::vector<std::shared_ptr<EventSubscription>> managed_subscriptions_;
    std::atomic<bool> initialized_;
    std::atomic<bool> shutdown_requested_;
    
    // Performance tracking
    mutable std::atomic<uint64_t> events_published_{0};
    mutable std::atomic<uint64_t> events_received_{0};
};

/**
 * Thread-local event bus adapter for GUI components.
 * Provides convenient access to event bus functionality from any GUI component.
 */
class ThreadLocalEventBusAdapter {
public:
    /**
     * Get the thread-local event bus adapter instance.
     * Creates one if it doesn't exist.
     * @return Reference to the thread-local event bus adapter
     */
    static EventBusAdapter& getInstance();
    
    /**
     * Initialize the thread-local event bus adapter.
     * @param event_bus Reference to the event bus to use
     */
    static void initialize(EventBus& event_bus);
    
    /**
     * Shutdown the thread-local event bus adapter.
     */
    static void shutdown();

private:
    static thread_local std::unique_ptr<EventBusAdapter> adapter_;
    static thread_local bool initialized_;
};

} // namespace gui
} // namespace cataclysm

// Macro helpers for common event bus adapter operations
#define GUI_EVENT_ADAPTER_PUBLISH_OVERLAY_OPEN(adapter, overlay_id) \
    (adapter).publishOverlayOpen(overlay_id)

#define GUI_EVENT_ADAPTER_PUBLISH_OVERLAY_CLOSE(adapter, overlay_id) \
    (adapter).publishOverlayClose(overlay_id)

#define GUI_EVENT_ADAPTER_PUBLISH_FILTER_APPLIED(adapter, filter, component) \
    (adapter).publishFilterApplied(filter, component)

#define GUI_EVENT_ADAPTER_PUBLISH_ITEM_SELECTED(adapter, item_id, source) \
    (adapter).publishItemSelected(item_id, source)

#define GUI_EVENT_ADAPTER_SUBSCRIBE_TO_STATUS_CHANGE(adapter, callback) \
    (adapter).subscribeToStatusChange(callback)

#define GUI_EVENT_ADAPTER_SUBSCRIBE_TO_INVENTORY_CHANGE(adapter, callback) \
    (adapter).subscribeToInventoryChange(callback)

#define GUI_EVENT_ADAPTER_SUBSCRIBE_TO_NOTICE(adapter, callback) \
    (adapter).subscribeToGameplayNotice(callback)

#endif // GUI_EVENT_BUS_ADAPTER_H