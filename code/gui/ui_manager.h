#ifndef CATACLYSM_GUI_UI_MANAGER_H
#define CATACLYSM_GUI_UI_MANAGER_H

#include <cstddef>
#include <mutex>
#include <vector>

namespace cataclysm::gui {

class UiAdaptor;

class UiManager {
public:
    static UiManager& instance();

    void register_adaptor(UiAdaptor& adaptor);
    void unregister_adaptor(UiAdaptor& adaptor);

    void request_redraw();
    void request_screen_resize(int width, int height);

    std::size_t registered_count() const;
    bool is_registered(const UiAdaptor& adaptor) const;

private:
    UiManager() = default;
    UiManager(const UiManager&) = delete;
    UiManager& operator=(const UiManager&) = delete;

    void dispatch_redraw(const std::vector<UiAdaptor*>& adaptors);
    void dispatch_screen_resize(const std::vector<UiAdaptor*>& adaptors, int width, int height);

    mutable std::mutex mutex_;
    std::vector<UiAdaptor*> adaptors_;
};

}  // namespace cataclysm::gui

#endif  // CATACLYSM_GUI_UI_MANAGER_H
