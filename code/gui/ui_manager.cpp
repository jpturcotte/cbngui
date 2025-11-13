#include "ui_manager.h"

#include <algorithm>

#include "ui_adaptor.h"

namespace cataclysm::gui {

UiManager& UiManager::instance() {
    static UiManager instance;
    return instance;
}

void UiManager::register_adaptor(UiAdaptor& adaptor) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(adaptors_.begin(), adaptors_.end(), &adaptor) == adaptors_.end()) {
        adaptors_.push_back(&adaptor);
    }
}

void UiManager::unregister_adaptor(UiAdaptor& adaptor) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find(adaptors_.begin(), adaptors_.end(), &adaptor);
    if (it != adaptors_.end()) {
        adaptors_.erase(it);
    }
}

void UiManager::request_redraw() {
    std::vector<UiAdaptor*> adaptors_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        adaptors_copy = adaptors_;
    }

    dispatch_redraw(adaptors_copy);
}

void UiManager::request_screen_resize(int width, int height) {
    std::vector<UiAdaptor*> adaptors_copy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        adaptors_copy = adaptors_;
    }

    dispatch_screen_resize(adaptors_copy, width, height);
}

std::size_t UiManager::registered_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return adaptors_.size();
}

bool UiManager::is_registered(const UiAdaptor& adaptor) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(adaptors_.begin(), adaptors_.end(), &adaptor) != adaptors_.end();
}

void UiManager::dispatch_redraw(const std::vector<UiAdaptor*>& adaptors) {
    for (UiAdaptor* adaptor : adaptors) {
        if (adaptor) {
            adaptor->trigger_redraw();
        }
    }
}

void UiManager::dispatch_screen_resize(const std::vector<UiAdaptor*>& adaptors, int width, int height) {
    for (UiAdaptor* adaptor : adaptors) {
        if (adaptor) {
            adaptor->trigger_screen_resize(width, height);
        }
    }
}

}  // namespace cataclysm::gui
