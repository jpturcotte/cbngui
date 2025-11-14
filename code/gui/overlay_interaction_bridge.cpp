#include "overlay_interaction_bridge.h"

#include "event_bus_adapter.h"

namespace cataclysm {
namespace gui {

namespace {
auto make_inventory_click_default() {
    return std::function<void(const inventory_entry&)>([](const inventory_entry&) {});
}

auto make_inventory_key_default() {
    return std::function<void(const SDL_KeyboardEvent&)>([](const SDL_KeyboardEvent&) {});
}

auto make_character_tab_default() {
    return std::function<void(const std::string&)>([](const std::string&) {});
}

auto make_character_row_default() {
    return std::function<void(const std::string&, int)>([](const std::string&, int) {});
}

auto make_character_command_default() {
    return std::function<void(CharacterCommand)>([](CharacterCommand) {});
}
} // namespace

OverlayInteractionBridge::OverlayInteractionBridge(EventBusAdapter& event_bus_adapter)
    : event_bus_adapter_(event_bus_adapter),
      inventory_click_handler_(make_inventory_click_default()),
      inventory_key_handler_(make_inventory_key_default()),
      character_tab_handler_(make_character_tab_default()),
      character_row_handler_(make_character_row_default()),
      character_command_handler_(make_character_command_default()) {
    register_subscriptions();
}

OverlayInteractionBridge::~OverlayInteractionBridge() {
    unregister_subscriptions();
}

void OverlayInteractionBridge::register_subscriptions() {
    inventory_click_subscription_ = event_bus_adapter_.subscribe<InventoryItemClickedEvent>(
        [this](const InventoryItemClickedEvent& event) {
            inventory_click_handler_(event.getEntry());
        });

    inventory_key_subscription_ = event_bus_adapter_.subscribe<InventoryKeyInputEvent>(
        [this](const InventoryKeyInputEvent& event) {
            inventory_key_handler_(event.getKeyEvent());
        });

    character_tab_subscription_ = event_bus_adapter_.subscribe<CharacterTabRequestedEvent>(
        [this](const CharacterTabRequestedEvent& event) {
            character_tab_handler_(event.getTabId());
        });

    character_row_subscription_ = event_bus_adapter_.subscribe<CharacterRowActivatedEvent>(
        [this](const CharacterRowActivatedEvent& event) {
            character_row_handler_(event.getTabId(), event.getRowIndex());
        });

    character_command_subscription_ = event_bus_adapter_.subscribe<CharacterCommandEvent>(
        [this](const CharacterCommandEvent& event) {
            character_command_handler_(event.getCommand());
        });
}

void OverlayInteractionBridge::unregister_subscriptions() {
    unsubscribe_and_reset(inventory_click_subscription_);
    unsubscribe_and_reset(inventory_key_subscription_);
    unsubscribe_and_reset(character_tab_subscription_);
    unsubscribe_and_reset(character_row_subscription_);
    unsubscribe_and_reset(character_command_subscription_);
}

void OverlayInteractionBridge::unsubscribe_and_reset(std::shared_ptr<EventSubscription>& subscription) {
    if (subscription) {
        subscription->unsubscribe();
        subscription.reset();
    }
}

void OverlayInteractionBridge::set_inventory_click_handler(std::function<void(const inventory_entry&)> handler) {
    assign_or_default(inventory_click_handler_, std::move(handler), make_inventory_click_default());
}

void OverlayInteractionBridge::set_inventory_key_handler(std::function<void(const SDL_KeyboardEvent&)> handler) {
    assign_or_default(inventory_key_handler_, std::move(handler), make_inventory_key_default());
}

void OverlayInteractionBridge::set_character_tab_handler(std::function<void(const std::string&)> handler) {
    assign_or_default(character_tab_handler_, std::move(handler), make_character_tab_default());
}

void OverlayInteractionBridge::set_character_row_handler(std::function<void(const std::string&, int)> handler) {
    assign_or_default(character_row_handler_, std::move(handler), make_character_row_default());
}

void OverlayInteractionBridge::set_character_command_handler(std::function<void(CharacterCommand)> handler) {
    assign_or_default(character_command_handler_, std::move(handler), make_character_command_default());
}

} // namespace gui
} // namespace cataclysm

