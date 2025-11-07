#include "overlay_ui.h"
#include "event_bus_adapter.h"
#include "mock_events.h"
#include "imgui.h"
#include "map_widget.h"

OverlayUI::OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter) : map_widget_(std::make_unique<MapWidget>()), event_bus_adapter_(event_bus_adapter) {}

OverlayUI::~OverlayUI() {}

void OverlayUI::Draw() {
    ImGui::ShowDemoWindow();

    map_widget_->Draw();

    ImGui::Begin("Event Publishing Demo");
    if (ImGui::Button("Publish Mock Event")) {
        event_bus_adapter_.publish(cataclysm::gui::UIButtonClickedEvent("test_button"));
    }
    ImGui::End();
}
