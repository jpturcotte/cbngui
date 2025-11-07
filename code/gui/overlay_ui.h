#ifndef OVERLAY_UI_H
#define OVERLAY_UI_H

namespace cataclysm {
namespace gui {
class EventBusAdapter;
}
}

class OverlayUI {
public:
    OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter);
    ~OverlayUI();

    void Draw();

private:
    cataclysm::gui::EventBusAdapter& event_bus_adapter_;
};

#endif // OVERLAY_UI_H
