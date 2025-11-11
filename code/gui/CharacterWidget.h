#ifndef CHARACTER_WIDGET_H
#define CHARACTER_WIDGET_H

#include "CharacterOverlayState.h"
#include "event_bus_adapter.h"

class CharacterWidget {
public:
    CharacterWidget(cataclysm::gui::EventBusAdapter& event_bus_adapter);
    ~CharacterWidget();

    CharacterWidget(const CharacterWidget&) = delete;
    CharacterWidget& operator=(const CharacterWidget&) = delete;
    CharacterWidget(CharacterWidget&&) = delete;
    CharacterWidget& operator=(CharacterWidget&&) = delete;

    void Draw(const character_overlay_state& state);

private:
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
};

#endif // CHARACTER_WIDGET_H
