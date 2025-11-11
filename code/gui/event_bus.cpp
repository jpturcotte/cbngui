#include "event_bus.h"
#include <iostream>

namespace cataclysm {
namespace gui {

// EventBus implementation
EventBus::EventBus() = default;

EventBus::~EventBus() {
    clearAll();
}

void EventBus::publishDynamic(const std::unique_ptr<Event>& event) {
    if (!event) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    auto it = subscriptions_.find(std::type_index(typeid(*event)));
    if (it != subscriptions_.end()) {
        // Create a copy of subscriptions to avoid issues with unsubscription during iteration
        auto subscriptions_copy = it->second;
        
        for (const auto& subscription : subscriptions_copy) {
            if (subscription->isActive()) {
                // For dynamic events, we need to cast back to the specific type
                // This is a bit of a hack, but necessary for dynamic dispatch
                // In practice, use the typed publish method for better performance
                try {
                    // Note: This would require runtime type information and virtual dispatch
                    // For now, we'll skip dynamic event publishing as it's complex
                    // and the typed publish method should be used instead
                } catch (const std::exception& e) {
                    std::cerr << "Error publishing dynamic event: " << e.what() << std::endl;
                }
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

void EventBus::unsubscribeInternal(const std::type_index& event_type) {
    auto it = subscriptions_.find(event_type);
    if (it != subscriptions_.end()) {
        for (auto& subscription : it->second) {
            if (subscription) {
                subscription->deactivate();
            }
        }
        subscriptions_.erase(it);
    }
}

void EventBus::clearAll() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    for (auto& [event_type, subscriptions] : subscriptions_) {
        for (auto& subscription : subscriptions) {
            if (subscription) {
                subscription->deactivate();
            }
        }
    }
    subscriptions_.clear();
}

size_t EventBus::getSubscriptionCount() const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    size_t count = 0;
    for (const auto& [event_type, subscriptions] : subscriptions_) {
        count += std::count_if(subscriptions.begin(), subscriptions.end(),
                             [](const auto& sub) { return sub->isActive(); });
    }
    return count;
}

bool EventBus::isEmpty() const {
    return getSubscriptionCount() == 0;
}

// EventBusManager implementation
std::unique_ptr<EventBus> EventBusManager::global_event_bus_;
std::atomic<bool> EventBusManager::initialized_{false};

EventBus& EventBusManager::getGlobalEventBus() {
    if (!initialized_.load()) {
        initialize();
    }
    return *global_event_bus_;
}

void EventBusManager::initialize() {
    if (!initialized_.load()) {
        global_event_bus_ = std::make_unique<EventBus>();
        initialized_.store(true);
    }
}

void EventBusManager::shutdown() {
    if (initialized_.load()) {
        global_event_bus_->clearAll();
        global_event_bus_.reset();
        initialized_.store(false);
    }
}

} // namespace gui
} // namespace cataclysm