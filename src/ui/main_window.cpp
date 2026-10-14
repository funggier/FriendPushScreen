#include "ui/main_window.h"

#include <string>

namespace fps::ui {
namespace {

constexpr wchar_t kWindowClassName[] =
    L"FriendPushScreenMainWindow";
constexpr UINT_PTR kRenderTimerId = 1;
constexpr UINT kRenderTimerMs = 100;

}  // namespace

void MainWindow::SetResizeCallback(ResizeCallback callback) {
    resize_callback_ = std::move(callback);
}

void MainWindow::SetFrameCallback(FrameCallback callback) {
    frame_callback_ = std::move(callback);
}

void MainWindow::SetStatusText(std::wstring_view text) {
    if (hwnd_ == nullptr) {
        return;
    }

    std::wstring title{L"FriendPushScreen — "};
    title.append(text);
    SetWindowTextW(hwnd_, title.c_str());
}

bool MainWindow::Create(HINSTANCE instance, int show_command) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &MainWindow::WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    window_class.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"FriendPushScreen",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        700,
        nullptr,
        nullptr,
        instance,
        this);
    if (hwnd_ == nullptr) {
        return false;
    }

    render_hwnd_ = CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0,
        0,
        1,
        1,
        hwnd_,
        nullptr,
        instance,
        nullptr);
    if (render_hwnd_ == nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    LayoutChildren();
    SetTimer(hwnd_, kRenderTimerId, kRenderTimerMs, nullptr);

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
    return true;
}

void MainWindow::LayoutChildren() {
    if (hwnd_ == nullptr || render_hwnd_ == nullptr) {
        return;
    }

    RECT client{};
    if (GetClientRect(hwnd_, &client) == FALSE) {
        return;
    }

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    SetWindowPos(
        render_hwnd_,
        nullptr,
        0,
        0,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);

    if (resize_callback_) {
        resize_callback_(
            static_cast<UINT>(width > 0 ? width : 0),
            static_cast<UINT>(height > 0 ? height : 0));
    }
}

LRESULT CALLBACK MainWindow::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));
    }

    if (self != nullptr) {
        return self->HandleMessage(
            hwnd,
            message,
            wparam,
            lparam);
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT MainWindow::HandleMessage(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case WM_SIZE:
        LayoutChildren();
        return 0;

    case WM_TIMER:
        if (wparam == kRenderTimerId && frame_callback_) {
            frame_callback_();
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, kRenderTimerId);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace fps::ui
