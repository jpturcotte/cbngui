#include "event_bus_adapter.h"
#include <iostream>
#include <algorithm>

namespace cataclysm {
namespace gui {

// EventBusAdapter implementation
EventBusAdapter::EventBusAdapter()
    : event_bus_(EventBusManager::getGlobalEventBus()), initialized_(false), shutdown_requested_(false) {
}

EventBusAdapter::EventBusAdapter(EventBus& event_bus)
    : event_bus_(event_bus), initialized_(false), shutdown_requested_(false) {
}

EventBusAdapter::~EventBusAdapter() {
    if (initialized_.load()) {
        shutdown();
    }
}

void EventBusAdapter::initialize() {
    if (initialized_.load()) {
        return;
    }
    
    initialized_.store(true);
    setupDefaultSubscriptions();
    
    std::cout << "EventBusAdapter initialized with " << getSubscriptionCount() 
              << " active subscriptions" << std::endl;
}

void EventBusAdapter::shutdown() {
    if (!initialized_.load() || shutdown_requested_.load()) {
        return;
    }
    
    shutdown_requested_.store(true);
    
    cleanupSubscriptions();
    managed_subscriptions_.clear();
    
    initialized_.store(false);
    shutdown_requested_.store(false);
    
    std::cout << "EventBusAdapter shutdown completed" << std::endl;
}

void EventBusAdapter::publishOverlayOpen(const std::string& overlay_id, bool is_modal) {
    UiOverlayOpenEvent event(overlay_id);
    event.setModal(is_modal);
    
    event_bus_.publish(event);
    events_published_.fetch_add(1);
    
    std::cout << "Published overlay open event: " << overlay_id 
              << " (modal: " << (is_modal ? "yes" : "no") << ")" << std::endl;
}

void EventBusAdapter::publishOverlayClose(const std::string& overlay_id, bool was_cancelled) {
    UiOverlayCloseEvent event(overlay_id);
    event.setCancelled(was_cancelled);
    
    event_bus_.publish(event);
    events_published_.fetch_add(1);
    
    std::cout << "Published overlay close event: " << overlay_id 
              << " (cancelled: " << (was_cancelled ? "yes" : "no") << ")" << std::endl;
}

void EventBusAdapter::publishFilterApplied(const std::string& filter_text, 
                                          const std::string& target_component,
                                          bool case_sensitive) {
    UiFilterAppliedEvent event(filter_text, target_component);
    event.setCaseSensitive(case_sensitive);
    
    event_bus_.publish(event);
    events_published_.fetch_add(1);
    
    std::cout << "Published filter applied event: '" << filter_text 
              << "' for component: " << target_component << std::endl;
}

void EventBusAdapter::publishItemSelected(const std::string& item_id,
                                         const std::string& source_component,
                                         bool is_double_click,
                                         int item_count) {
    UiItemSelectedEvent event(item_id, source_component);
    event.setDoubleClick(is_double_click);
    event.setItemCount(item_count);
    
    event_bus_.publish(event);
    events_published_.fetch_add(1);
    
    std::cout << "Published item selected event: " << item_id 
              << " from: " << source_component 
              << " (double-click: " << (is_double_click ? "yes" : "no") 
              << ", count: " << item_count << ")" << std::endl;
}

void EventBusAdapter::publishDataBindingUpdate(const std::string& binding_id,
                                              const std::string& data_source,
                                              bool forced) {
    UiDataBindingUpdateEvent event(binding_id, data_source);
    event.setForced(forced);
    
    event_bus_.publish(event);
    events_published_.fetch_add(1);
    
    std::cout << "Published data binding update event: " << binding_id 
              << " from: " << data_source 
              << " (forced: " << (forced ? "yes" : "no") << ")" << std::endl;
}

std::shared_ptr<EventSubscription> EventBusAdapter::subscribeToStatusChange(
    std::function<void(const GameplayStatusChangeEvent&)> callback) {
    
    auto subscription = event_bus_.subscribe<GameplayStatusChangeEvent>(
        [this, callback = std::move(callback)](const GameplayStatusChangeEvent& event) {
            callback(event);
            events_received_.fetch_add(1);
        }
    );
    
    if (subscription) {
        managed_subscriptions_.push_back(subscription);
    }
    
    return subscription;
}

std::shared_ptr<EventSubscription> EventBusAdapter::subscribeToInventoryChange(
    std::function<void(const GameplayInventoryChangeEvent&)> callback) {
    
    auto subscription = event_bus_.subscribe<GameplayInventoryChangeEvent>(
        [this, callback = std::move(callback)](const GameplayInventoryChangeEvent& event) {
            callback(event);
            events_received_.fetch_add(1);
        }
    );
    
    if (subscription) {
        managed_subscriptions_.push_back(subscription);
    }
    
    return subscription;
}

std::shared_ptr<EventSubscription> EventBusAdapter::subscribeToGameplayNotice(
    std::function<void(const GameplayNoticeEvent&)> callback) {
    
    auto subscription = event_bus_.subscribe<GameplayNoticeEvent>(
        [this, callback = std::move(callback)](const GameplayNoticeEvent& event) {
            callback(event);
            events_received_.fetch_add(1);
        }
    );
    
    if (subscription) {
        managed_subscriptions_.push_back(subscription);
    }
    
    return subscription;
}

size_t EventBusAdapter::getSubscriptionCount() const {
    size_t count = 0;
    for (const auto& subscription : managed_subscriptions_) {
        if (subscription && subscription->isActive()) {
            count++;
        }
    }
    return count;
}

bool EventBusAdapter::isEmpty() const {
    return getSubscriptionCount() == 0;
}

void EventBusAdapter::clearAllSubscriptions() {
    for (auto& subscription : managed_subscriptions_) {
        if (subscription) {
            subscription->unsubscribe();
        }
    }
    managed_subscriptions_.clear();
}

std::unordered_map<std::string, int> EventBusAdapter::getStatistics() const {
    std::unordered_map<std::string, int> stats;
    
    stats["total_subscriptions"] = static_cast<int>(getSubscriptionCount());
    stats["events_published"] = static_cast<int>(events_published_.load());
    stats["events_received"] = static_cast<int>(events_received_.load());
    stats["managed_subscriptions"] = static_cast<int>(managed_subscriptions_.size());
    
    // Count subscriptions by type
    stats["status_change_subscriptions"] = static_cast<int>(getSubscriptionCount<GameplayStatusChangeEvent>());
    stats["inventory_change_subscriptions"] = static_cast<int>(getSubscriptionCount<GameplayInventoryChangeEvent>());
    stats["notice_subscriptions"] = static_cast<int>(getSubscriptionCount<GameplayNoticeEvent>());
    stats["overlay_open_subscriptions"] = static_cast<int>(getSubscriptionCount<UiOverlayOpenEvent>());
    stats["overlay_close_subscriptions"] = static_cast<int>(getSubscriptionCount<UiOverlayCloseEvent>());
    
    return stats;
}

void EventBusAdapter::setupDefaultSubscriptions() {
    // Set up default gameplay event subscriptions
    // These will be overridden by specific GUI components as needed
    
    // Subscribe to status changes (default: no-op)
    subscribeToStatusChange([](const GameplayStatusChangeEvent& event) {
        std::cout << "Status change: " << event.getStatusType() 
                  << " -> " << event.getNewValue() << std::endl;
    });
    
    // Subscribe to inventory changes (default: no-op)
    subscribeToInventoryChange([](const GameplayInventoryChangeEvent& event) {
        std::cout << "Inventory change: " << event.getChangeType() 
                  << " - " << event.getItemName() << std::endl;
    });
    
    // Subscribe to gameplay notices (default: log to console)
    subscribeToGameplayNotice([](const GameplayNoticeEvent& event) {
        std::cout << "Gameplay notice [" << event.getNoticeType() << "]: " 
                  << event.getMessage() << std::endl;
    });
}

void EventBusAdapter::cleanupSubscriptions() {
    // Clean up all managed subscriptions
    for (auto& subscription : managed_subscriptions_) {
        if (subscription) {
            subscription->unsubscribe();
        }
    }
}

// ThreadLocalEventBusAdapter implementation
thread_local std::unique_ptr<EventBusAdapter> ThreadLocalEventBusAdapter::adapter_;
thread_local bool ThreadLocalEventBusAdapter::initialized_ = false;

EventBusAdapter& ThreadLocalEventBusAdapter::getInstance() {
    if (!initialized_) {
        initialize(EventBusManager::getGlobalEventBus());
    }
    return *adapter_;
}

void ThreadLocalEventBusAdapter::initialize(EventBus& event_bus) {
    if (initialized_) {
        return;
    }
    
    adapter_ = std::make_unique<EventBusAdapter>(event_bus);
    adapter_->initialize();
    initialized_ = true;
}

void ThreadLocalEventBusAdapter::shutdown() {
    if (initialized_ && adapter_) {
        adapter_->shutdown();
        adapter_.reset();
        initialized_ = false;
    }
}

} // namespace gui
} // namespace cataclysm
