#ifndef MOCK_EVENTS_H
#define MOCK_EVENTS_H

#include "event_bus.h"
#include <string>

namespace cataclysm {
namespace gui {

class UIButtonClickedEvent : public Event {
public:
    UIButtonClickedEvent(std::string button_id) : button_id(button_id) {}
    std::string getEventType() const override { return "UIButtonClickedEvent"; }
    std::unique_ptr<Event> clone() const override { return std::make_unique<UIButtonClickedEvent>(*this); }

    std::string button_id;
};

} // namespace gui
} // namespace cataclysm

#endif // MOCK_EVENTS_H
