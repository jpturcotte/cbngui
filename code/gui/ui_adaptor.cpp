#include "ui_adaptor.h"

namespace cataclysm::gui {

UiAdaptor::UiAdaptor() = default;

UiAdaptor::~UiAdaptor() = default;

void UiAdaptor::set_redraw_callback(RedrawCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    redraw_callback_ = std::move(callback);
}

void UiAdaptor::set_screen_resize_callback(ScreenResizeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    screen_resize_callback_ = std::move(callback);
}

void UiAdaptor::trigger_redraw() const {
    RedrawCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = redraw_callback_;
    }

    if (callback) {
        callback();
    }
}

void UiAdaptor::trigger_screen_resize(int width, int height) const {
    ScreenResizeCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = screen_resize_callback_;
    }

    if (callback) {
        callback(width, height);
    }
}

}  // namespace cataclysm::gui
