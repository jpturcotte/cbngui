#pragma once

#include <SDL.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "events.h"
#include "InventoryOverlayState.h"

namespace cataclysm {
namespace gui {

class EventBusAdapter;
class EventSubscription;

class OverlayInteractionBridge {
public:
    explicit OverlayInteractionBridge(EventBusAdapter& event_bus_adapter);
    ~OverlayInteractionBridge();

    OverlayInteractionBridge(const OverlayInteractionBridge&) = delete;
    OverlayInteractionBridge& operator=(const OverlayInteractionBridge&) = delete;
    OverlayInteractionBridge(OverlayInteractionBridge&&) = delete;
    OverlayInteractionBridge& operator=(OverlayInteractionBridge&&) = delete;

    void enable_inventory_forwarding();
    void disable_inventory_forwarding();
    bool is_inventory_forwarding_active() const;

    void enable_character_forwarding();
    void disable_character_forwarding();
    bool is_character_forwarding_active() const;

    void set_inventory_click_handler(std::function<void(const inventory_entry&)> handler);
    void set_inventory_key_handler(std::function<void(const SDL_KeyboardEvent&)> handler);
    void set_character_tab_handler(std::function<void(const std::string&)> handler);
    void set_character_row_handler(std::function<void(const std::string&, int)> handler);
    void set_character_command_handler(std::function<void(CharacterCommand)> handler);

private:
    EventBusAdapter& event_bus_adapter_;

    std::shared_ptr<EventSubscription> inventory_click_subscription_;
    std::shared_ptr<EventSubscription> inventory_key_subscription_;
    std::shared_ptr<EventSubscription> character_tab_subscription_;
    std::shared_ptr<EventSubscription> character_row_subscription_;
    std::shared_ptr<EventSubscription> character_command_subscription_;

    bool inventory_forwarding_active_ = false;
    bool character_forwarding_active_ = false;

    std::function<void(const inventory_entry&)> inventory_click_handler_;
    std::function<void(const SDL_KeyboardEvent&)> inventory_key_handler_;
    std::function<void(const std::string&)> character_tab_handler_;
    std::function<void(const std::string&, int)> character_row_handler_;
    std::function<void(CharacterCommand)> character_command_handler_;

    void register_inventory_subscriptions();
    void unregister_inventory_subscriptions();
    void register_character_subscriptions();
    void unregister_character_subscriptions();

    void unsubscribe_and_reset(std::shared_ptr<EventSubscription>& subscription);

    template<typename Handler, typename DefaultHandler>
    void assign_or_default(Handler& target, Handler&& incoming, DefaultHandler&& default_handler) {
        if (incoming) {
            target = std::move(incoming);
        } else {
            target = std::forward<DefaultHandler>(default_handler);
        }
    }
};

} // namespace gui
} // namespace cataclysm

