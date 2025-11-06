#ifndef GUI_EVENTS_H
#define GUI_EVENTS_H

#include "event_bus.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <chrono>

namespace cataclysm {
namespace gui {

/**
 * Base class for all GUI events.
 * Provides common functionality for all event types.
 */
class GuiEvent : public Event {
public:
    virtual ~GuiEvent() = default;
    std::string getEventType() const override { return getEventTypeName(); }
    virtual std::string getEventTypeName() const = 0;
    
    /**
     * Get the source component that created this event.
     * @return String identifier of the source component
     */
    std::string getSource() const { return source_; }
    
    /**
     * Set the source component that created this event.
     * @param source String identifier of the source component
     */
    void setSource(const std::string& source) { source_ = source; }
    
    /**
     * Get the timestamp when this event was created.
     * @return Event creation timestamp
     */
    std::uint64_t getTimestamp() const { return timestamp_; }

protected:
    GuiEvent() : timestamp_(getCurrentTimestamp()) {}
    GuiEvent(const std::string& source) : source_(source), timestamp_(getCurrentTimestamp()) {}
    
    static std::uint64_t getCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

private:
    std::string source_;
    std::uint64_t timestamp_;
};

/**
 * Event: UI Overlay Opened
 * Published when a GUI overlay is opened and becomes active.
 * Subscribers: ui_manager, game systems
 */
class UiOverlayOpenEvent : public GuiEvent {
public:
    UiOverlayOpenEvent() : GuiEvent("overlay_ui") {}
    explicit UiOverlayOpenEvent(const std::string& overlay_id) 
        : GuiEvent("overlay_ui"), overlay_id_(overlay_id) {}
    
    std::string getEventTypeName() const override { return "ui_overlay_open"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<UiOverlayOpenEvent>(overlay_id_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getOverlayId() const { return overlay_id_; }
    void setOverlayId(const std::string& id) { overlay_id_ = id; }
    
    bool isModal() const { return is_modal_; }
    void setModal(bool modal) { is_modal_ = modal; }

private:
    std::string overlay_id_;
    bool is_modal_ = false;
};

/**
 * Event: UI Overlay Closed
 * Published when a GUI overlay is closed and becomes inactive.
 * Subscribers: ui_manager, game systems
 */
class UiOverlayCloseEvent : public GuiEvent {
public:
    UiOverlayCloseEvent() : GuiEvent("overlay_ui") {}
    explicit UiOverlayCloseEvent(const std::string& overlay_id) 
        : GuiEvent("overlay_ui"), overlay_id_(overlay_id) {}
    
    std::string getEventTypeName() const override { return "ui_overlay_close"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<UiOverlayCloseEvent>(overlay_id_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getOverlayId() const { return overlay_id_; }
    void setOverlayId(const std::string& id) { overlay_id_ = id; }
    
    bool wasCancelled() const { return was_cancelled_; }
    void setCancelled(bool cancelled) { was_cancelled_ = cancelled; }

private:
    std::string overlay_id_;
    bool was_cancelled_ = false;
};

/**
 * Event: UI Filter Applied
 * Published when a filter is applied to a UI component (e.g., inventory filter).
 * Subscribers: inventory_ui, list components
 */
class UiFilterAppliedEvent : public GuiEvent {
public:
    UiFilterAppliedEvent() : GuiEvent("overlay_ui") {}
    UiFilterAppliedEvent(const std::string& filter_text, const std::string& target_component)
        : GuiEvent("overlay_ui"), filter_text_(filter_text), target_component_(target_component) {}
    
    std::string getEventTypeName() const override { return "ui_filter_applied"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<UiFilterAppliedEvent>(filter_text_, target_component_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getFilterText() const { return filter_text_; }
    void setFilterText(const std::string& text) { filter_text_ = text; }
    
    const std::string& getTargetComponent() const { return target_component_; }
    void setTargetComponent(const std::string& component) { target_component_ = component; }
    
    bool isCaseSensitive() const { return case_sensitive_; }
    void setCaseSensitive(bool sensitive) { case_sensitive_ = sensitive; }

private:
    std::string filter_text_;
    std::string target_component_;
    bool case_sensitive_ = false;
};

/**
 * Event: UI Item Selected
 * Published when an item is selected in a GUI component.
 * Subscribers: character system, map system, inventory system
 */
class UiItemSelectedEvent : public GuiEvent {
public:
    UiItemSelectedEvent() : GuiEvent("overlay_ui") {}
    UiItemSelectedEvent(const std::string& item_id, const std::string& source_component)
        : GuiEvent("overlay_ui"), item_id_(item_id), source_component_(source_component) {}
    
    std::string getEventTypeName() const override { return "ui_item_selected"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<UiItemSelectedEvent>(item_id_, source_component_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getItemId() const { return item_id_; }
    void setItemId(const std::string& id) { item_id_ = id; }
    
    const std::string& getSourceComponent() const { return source_component_; }
    void setSourceComponent(const std::string& component) { source_component_ = component; }
    
    bool isDoubleClick() const { return is_double_click_; }
    void setDoubleClick(bool double_click) { is_double_click_ = double_click; }
    
    int getItemCount() const { return item_count_; }
    void setItemCount(int count) { item_count_ = count; }

private:
    std::string item_id_;
    std::string source_component_;
    bool is_double_click_ = false;
    int item_count_ = 1;
};

/**
 * Event: Gameplay Status Change
 * Published when character or game status effects change.
 * Subscribers: OverlayUI, status display components
 */
class GameplayStatusChangeEvent : public GuiEvent {
public:
    GameplayStatusChangeEvent() : GuiEvent("gameplay") {}
    GameplayStatusChangeEvent(const std::string& status_type, const std::string& new_value)
        : GuiEvent("gameplay"), status_type_(status_type), new_value_(new_value) {}
    
    std::string getEventTypeName() const override { return "gameplay_status_change"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<GameplayStatusChangeEvent>(status_type_, new_value_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getStatusType() const { return status_type_; }
    void setStatusType(const std::string& type) { status_type_ = type; }
    
    const std::string& getNewValue() const { return new_value_; }
    void setNewValue(const std::string& value) { new_value_ = value; }
    
    const std::string& getOldValue() const { return old_value_; }
    void setOldValue(const std::string& value) { old_value_ = value; }
    
    bool isPositiveChange() const { return is_positive_change_; }
    void setPositiveChange(bool positive) { is_positive_change_ = positive; }

private:
    std::string status_type_;
    std::string new_value_;
    std::string old_value_;
    bool is_positive_change_ = false;
};

/**
 * Event: Gameplay Inventory Change
 * Published when the player's inventory changes.
 * Subscribers: OverlayUI, inventory display components
 */
class GameplayInventoryChangeEvent : public GuiEvent {
public:
    GameplayInventoryChangeEvent() : GuiEvent("gameplay") {}
    explicit GameplayInventoryChangeEvent(const std::string& change_type)
        : GuiEvent("gameplay"), change_type_(change_type) {}
    
    std::string getEventTypeName() const override { return "gameplay_inventory_change"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<GameplayInventoryChangeEvent>(change_type_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getChangeType() const { return change_type_; }
    void setChangeType(const std::string& type) { change_type_ = type; }
    
    const std::string& getItemId() const { return item_id_; }
    void setItemId(const std::string& id) { item_id_ = id; }
    
    int getItemCount() const { return item_count_; }
    void setItemCount(int count) { item_count_ = count; }
    
    const std::string& getItemName() const { return item_name_; }
    void setItemName(const std::string& name) { item_name_ = name; }

private:
    std::string change_type_;  // "added", "removed", "modified"
    std::string item_id_;
    int item_count_ = 0;
    std::string item_name_;
};

/**
 * Event: Gameplay Notice
 * Published to show transient notifications to the user.
 * Subscribers: OverlayUI, notification system
 */
class GameplayNoticeEvent : public GuiEvent {
public:
    GameplayNoticeEvent() : GuiEvent("gameplay") {}
    GameplayNoticeEvent(const std::string& message, const std::string& notice_type)
        : GuiEvent("gameplay"), message_(message), notice_type_(notice_type) {}
    
    std::string getEventTypeName() const override { return "gameplay_notice"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<GameplayNoticeEvent>(message_, notice_type_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getMessage() const { return message_; }
    void setMessage(const std::string& message) { message_ = message; }
    
    const std::string& getNoticeType() const { return notice_type_; }
    void setNoticeType(const std::string& type) { notice_type_ = type; }
    
    int getDuration() const { return duration_ms_; }
    void setDuration(int duration_ms) { duration_ms_ = duration_ms; }
    
    bool isPersistent() const { return is_persistent_; }
    void setPersistent(bool persistent) { is_persistent_ = persistent; }

private:
    std::string message_;
    std::string notice_type_;  // "info", "warning", "error", "success"
    int duration_ms_ = 3000;
    bool is_persistent_ = false;
};

/**
 * Event: UI Data Binding Update
 * Published when UI data bindings need to be refreshed.
 * Subscribers: OverlayUI, data binding system
 */
class UiDataBindingUpdateEvent : public GuiEvent {
public:
    UiDataBindingUpdateEvent() : GuiEvent("overlay_ui") {}
    UiDataBindingUpdateEvent(const std::string& binding_id, const std::string& data_source)
        : GuiEvent("overlay_ui"), binding_id_(binding_id), data_source_(data_source) {}
    
    std::string getEventTypeName() const override { return "ui_data_binding_update"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<UiDataBindingUpdateEvent>(binding_id_, data_source_);
        cloned->setSource(getSource());
        return cloned;
    }
    
    const std::string& getBindingId() const { return binding_id_; }
    void setBindingId(const std::string& id) { binding_id_ = id; }
    
    const std::string& getDataSource() const { return data_source_; }
    void setDataSource(const std::string& source) { data_source_ = source; }
    
    bool isForced() const { return is_forced_; }
    void setForced(bool forced) { is_forced_ = forced; }

private:
    std::string binding_id_;
    std::string data_source_;
    bool is_forced_ = false;
};

/**
 * Event: Performance Metrics Update
 * Published periodically to report GUI performance metrics.
 * Subscribers: Performance monitoring system
 */
class PerformanceMetricsUpdateEvent : public GuiEvent {
public:
    PerformanceMetricsUpdateEvent() : GuiEvent("performance_monitor") {}
    
    std::string getEventTypeName() const override { return "performance_metrics_update"; }
    std::unique_ptr<Event> clone() const override {
        auto cloned = std::make_unique<PerformanceMetricsUpdateEvent>();
        cloned->setSource(getSource());
        cloned->frame_time_ms_ = frame_time_ms_;
        cloned->draw_calls_ = draw_calls_;
        cloned->vertex_count_ = vertex_count_;
        cloned->subscribed_events_ = subscribed_events_;
        return cloned;
    }
    
    double getFrameTimeMs() const { return frame_time_ms_; }
    void setFrameTimeMs(double frame_time) { frame_time_ms_ = frame_time; }
    
    int getDrawCalls() const { return draw_calls_; }
    void setDrawCalls(int draw_calls) { draw_calls_ = draw_calls; }
    
    int getVertexCount() const { return vertex_count_; }
    void setVertexCount(int vertex_count) { vertex_count_ = vertex_count; }
    
    int getSubscribedEvents() const { return subscribed_events_; }
    void setSubscribedEvents(int count) { subscribed_events_ = count; }

private:
    double frame_time_ms_ = 0.0;
    int draw_calls_ = 0;
    int vertex_count_ = 0;
    int subscribed_events_ = 0;
};

} // namespace gui
} // namespace cataclysm

#endif // GUI_EVENTS_H