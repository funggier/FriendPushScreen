#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <functional>
#include <string_view>

namespace fps::ui {

class MainWindow {
public:
    using ResizeCallback = std::function<void(UINT, UINT)>;
    using FrameCallback = std::function<void()>;

    bool Create(HINSTANCE instance, int show_command);

    HWND Handle() const noexcept { return hwnd_; }
    HWND RenderHandle() const noexcept { return render_hwnd_; }

    void SetResizeCallback(ResizeCallback callback);
    void SetFrameCallback(FrameCallback callback);
    void SetStatusText(std::wstring_view text);

private:
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam);

    LRESULT HandleMessage(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam);

    void LayoutChildren();

    HWND hwnd_{nullptr};
    HWND render_hwnd_{nullptr};
    ResizeCallback resize_callback_;
    FrameCallback frame_callback_;
};

}  // namespace fps::ui
