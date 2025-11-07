#include "overlay_ui.h"
#include "event_bus_adapter.h"
#include "mock_events.h"
#include "imgui.h"

OverlayUI::OverlayUI(cataclysm::gui::EventBusAdapter& event_bus_adapter) : event_bus_adapter_(event_bus_adapter) {}

OverlayUI::~OverlayUI() {}

void OverlayUI::Draw() {
    ImGui::ShowDemoWindow();

    ImGui::Begin("Event Publishing Demo");
    if (ImGui::Button("Publish Mock Event")) {
        event_bus_adapter_.publish(cataclysm::gui::UIButtonClickedEvent("test_button"));
    }
    ImGui::End();
}
