#include "event_bus.h"
#include "events.h"
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

#define PUBLISH_IF_TYPE(event_type)                           \
    if (auto* derived = dynamic_cast<const event_type*>(event.get())) { \
        publish<event_type>(*derived);                       \
        return;                                              \
    }

    // Shim dynamic events into the strongly-typed publish path so callers that
    // only know about the base Event interface can still notify subscribers.
    PUBLISH_IF_TYPE(UiOverlayOpenEvent);
    PUBLISH_IF_TYPE(UiOverlayCloseEvent);
    PUBLISH_IF_TYPE(UiFilterAppliedEvent);
    PUBLISH_IF_TYPE(UiItemSelectedEvent);
    PUBLISH_IF_TYPE(GameplayStatusChangeEvent);
    PUBLISH_IF_TYPE(GameplayInventoryChangeEvent);
    PUBLISH_IF_TYPE(GameplayNoticeEvent);
    PUBLISH_IF_TYPE(UiDataBindingUpdateEvent);
    PUBLISH_IF_TYPE(PerformanceMetricsUpdateEvent);
    PUBLISH_IF_TYPE(MapTileHoveredEvent);
    PUBLISH_IF_TYPE(MapTileClickedEvent);
    PUBLISH_IF_TYPE(InventoryItemClickedEvent);
    PUBLISH_IF_TYPE(InventoryKeyInputEvent);
    PUBLISH_IF_TYPE(InventoryOverlayForwardedClickEvent);
    PUBLISH_IF_TYPE(InventoryOverlayForwardedKeyEvent);
    PUBLISH_IF_TYPE(CharacterOverlayForwardedTabEvent);
    PUBLISH_IF_TYPE(CharacterOverlayForwardedRowEvent);
    PUBLISH_IF_TYPE(CharacterOverlayForwardedCommandEvent);
    PUBLISH_IF_TYPE(CharacterTabRequestedEvent);
    PUBLISH_IF_TYPE(CharacterRowActivatedEvent);
    PUBLISH_IF_TYPE(CharacterCommandEvent);

    std::cerr << "EventBus::publishDynamic received unknown event type '"
              << event->getEventType() << "'" << std::endl;
}
#undef PUBLISH_IF_TYPE

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
