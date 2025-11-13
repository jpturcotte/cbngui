#ifndef CATACLYSM_GUI_UI_ADAPTOR_H
#define CATACLYSM_GUI_UI_ADAPTOR_H

#include <functional>
#include <mutex>

namespace cataclysm::gui {

class UiAdaptor {
public:
    using RedrawCallback = std::function<void()>;
    using ScreenResizeCallback = std::function<void(int, int)>;

    UiAdaptor();
    ~UiAdaptor();

    void set_redraw_callback(RedrawCallback callback);
    void set_screen_resize_callback(ScreenResizeCallback callback);

    void trigger_redraw() const;
    void trigger_screen_resize(int width, int height) const;

private:
    mutable std::mutex mutex_;
    RedrawCallback redraw_callback_;
    ScreenResizeCallback screen_resize_callback_;
};

}  // namespace cataclysm::gui

#endif  // CATACLYSM_GUI_UI_ADAPTOR_H
