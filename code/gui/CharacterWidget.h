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

    [[nodiscard]] bool GetTabRect(const std::string& tab_id, ImVec2* min, ImVec2* max) const;
    [[nodiscard]] bool GetRowRect(const std::string& tab_id, size_t row_index, ImVec2* min, ImVec2* max) const;
    [[nodiscard]] bool GetCommandButtonRect(const std::string& label, ImVec2* min, ImVec2* max) const;

private:
    struct InteractiveRect {
        std::string id;
        ImVec2 min{0.0f, 0.0f};
        ImVec2 max{0.0f, 0.0f};
    };

    void RecordRect(std::vector<InteractiveRect>& container, const std::string& id);
    bool FindRect(const std::vector<InteractiveRect>& container,
                  const std::string& id,
                  ImVec2* min,
                  ImVec2* max) const;

    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
    std::vector<InteractiveRect> tab_rects_;
    std::vector<InteractiveRect> row_rects_;
    std::vector<InteractiveRect> command_button_rects_;
};

#endif // CHARACTER_WIDGET_H
