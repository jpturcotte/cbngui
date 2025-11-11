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

    const std::type_index type_index(typeid(*event));
    std::vector<std::shared_ptr<EventSubscription>> subscriptions_copy;

    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(type_index);
        if (it == subscriptions_.end()) {
            return;
        }
        subscriptions_copy = it->second;
    }

    for (const auto& subscription : subscriptions_copy) {
        if (subscription && subscription->isActive()) {
            try {
                // Dynamic dispatch not implemented; prefer typed publish.
            } catch (const std::exception& e) {
                std::cerr << "Error publishing dynamic event: " << e.what() << std::endl;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        auto it = subscriptions_.find(type_index);
        if (it == subscriptions_.end()) {
            return;
        }

        auto& stored = it->second;
        stored.erase(
            std::remove_if(stored.begin(), stored.end(),
                           [](const auto& sub) { return !sub || !sub->isActive(); }),
            stored.end());

        if (stored.empty()) {
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

void EventBus::unsubscribeInternal(const std::type_index& event_type, size_t subscription_id) {
    auto it = subscriptions_.find(event_type);
    if (it == subscriptions_.end()) {
        return;
    }

    auto& subscriptions = it->second;
    subscriptions.erase(std::remove_if(subscriptions.begin(), subscriptions.end(),
                                       [subscription_id](const auto& subscription) {
                                           if (!subscription) {
                                               return true;
                                           }
                                           if (subscription->getId() == subscription_id) {
                                               subscription->deactivate();
                                               return true;
                                           }
                                           if (!subscription->isActive()) {
                                               return true;
                                           }
                                           return false;
                                       }),
                        subscriptions.end());

    if (subscriptions.empty()) {
        subscriptions_.erase(it);
    }
}

void EventBus::unsubscribeById(const std::type_index& event_type, size_t subscription_id) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    unsubscribeInternal(event_type, subscription_id);
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
