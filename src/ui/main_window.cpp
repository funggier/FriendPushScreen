#include "ui/main_window.h"

#include "diagnostics/diagnostics_service.h"

#include <commdlg.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace fps::ui {

std::wstring NormalizeWin32MultilineText(
    std::wstring_view text) {
    std::wstring output;
    output.reserve(text.size() + 16);

    wchar_t previous = L'\0';
    for (const wchar_t ch : text) {
        if (ch == L'\n' && previous != L'\r') {
            output.push_back(L'\r');
        }
        output.push_back(ch);
        previous = ch;
    }

    return output;
}

namespace {

constexpr wchar_t kWindowClassName[] =
    L"FriendPushScreenMainWindow";
constexpr UINT_PTR kRenderTimerId = 1;
constexpr UINT kRenderTimerMs = 100;

constexpr int kRefreshButtonId = 1001;
constexpr int kSaveButtonId = 1002;
constexpr int kPanelWidth = 420;
constexpr int kPadding = 10;
constexpr int kButtonHeight = 32;
constexpr int kButtonGap = 8;

}  // namespace

void MainWindow::SetResizeCallback(ResizeCallback callback) {
    resize_callback_ = std::move(callback);
}

void MainWindow::SetFrameCallback(FrameCallback callback) {
    frame_callback_ = std::move(callback);
}

void MainWindow::SetRefreshCallback(CommandCallback callback) {
    refresh_callback_ = std::move(callback);
}

void MainWindow::SetSaveCallback(CommandCallback callback) {
    save_callback_ = std::move(callback);
}

void MainWindow::SetDiagnosticsReadyCallback(
    CommandCallback callback) {
    diagnostics_ready_callback_ = std::move(callback);
}

void MainWindow::SetStatusText(std::wstring_view text) {
    if (hwnd_ == nullptr) {
        return;
    }

    std::wstring title{L"FriendPushScreen — "};
    title.append(text);
    SetWindowTextW(hwnd_, title.c_str());
}

bool MainWindow::SetDiagnosticsText(std::wstring_view text) {
    if (diagnostics_hwnd_ == nullptr) {
        return false;
    }

    const std::wstring copy =
        NormalizeWin32MultilineText(text);
    return SetWindowTextW(
               diagnostics_hwnd_,
               copy.c_str()) != FALSE;
}

void MainWindow::SetCollecting(bool collecting) {
    if (refresh_button_ != nullptr) {
        EnableWindow(
            refresh_button_,
            collecting ? FALSE : TRUE);
        SetWindowTextW(
            refresh_button_,
            collecting ? L"Collecting..." : L"Refresh");
    }
}

std::optional<std::filesystem::path>
MainWindow::PromptDiagnosticsSavePath() {
    std::array<wchar_t, MAX_PATH> path{};
    wcscpy_s(
        path.data(),
        path.size(),
        L"FriendPushScreen-diagnostics.txt");

    constexpr wchar_t filter[] =
        L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"txt";
    dialog.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&dialog) == FALSE) {
        return std::nullopt;
    }

    return std::filesystem::path{path.data()};
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
        1280,
        760,
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

    diagnostics_hwnd_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"Collecting diagnostics...",
        WS_CHILD |
            WS_VISIBLE |
            WS_VSCROLL |
            ES_LEFT |
            ES_MULTILINE |
            ES_AUTOVSCROLL |
            ES_READONLY,
        0,
        0,
        1,
        1,
        hwnd_,
        nullptr,
        instance,
        nullptr);

    refresh_button_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Refresh",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0,
        0,
        1,
        1,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kRefreshButtonId)),
        instance,
        nullptr);

    save_button_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Save Diagnostics",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0,
        0,
        1,
        1,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(kSaveButtonId)),
        instance,
        nullptr);

    if (render_hwnd_ == nullptr ||
        diagnostics_hwnd_ == nullptr ||
        refresh_button_ == nullptr ||
        save_button_ == nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    SendMessageW(
        diagnostics_hwnd_,
        EM_SETLIMITTEXT,
        static_cast<WPARAM>(1024 * 1024),
        0);

    const HFONT font =
        static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(
        diagnostics_hwnd_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(font),
        TRUE);
    SendMessageW(
        refresh_button_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(font),
        TRUE);
    SendMessageW(
        save_button_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(font),
        TRUE);

    LayoutChildren();
    SetTimer(hwnd_, kRenderTimerId, kRenderTimerMs, nullptr);

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
    return true;
}

void MainWindow::LayoutChildren() {
    if (hwnd_ == nullptr ||
        render_hwnd_ == nullptr ||
        diagnostics_hwnd_ == nullptr) {
        return;
    }

    RECT client{};
    if (GetClientRect(hwnd_, &client) == FALSE) {
        return;
    }

    const int width = std::max(
        0,
        static_cast<int>(client.right - client.left));
    const int height = std::max(
        0,
        static_cast<int>(client.bottom - client.top));

    const int panel_width =
        std::min(kPanelWidth, std::max(280, width / 2));
    const int render_width =
        std::max(0, width - panel_width - kPadding);
    const int button_width =
        std::max(
            100,
            (panel_width - (3 * kPadding) - kButtonGap) / 2);
    const int diagnostics_height =
        std::max(
            0,
            height - (3 * kPadding) - kButtonHeight);

    SetWindowPos(
        render_hwnd_,
        nullptr,
        0,
        0,
        render_width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE);

    const int panel_x = render_width + kPadding;
    SetWindowPos(
        diagnostics_hwnd_,
        nullptr,
        panel_x,
        kPadding,
        panel_width - (2 * kPadding),
        diagnostics_height,
        SWP_NOZORDER | SWP_NOACTIVATE);

    const int button_y =
        kPadding + diagnostics_height + kPadding;

    SetWindowPos(
        refresh_button_,
        nullptr,
        panel_x + kPadding,
        button_y,
        button_width,
        kButtonHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    SetWindowPos(
        save_button_,
        nullptr,
        panel_x + kPadding + button_width + kButtonGap,
        button_y,
        button_width,
        kButtonHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    if (resize_callback_) {
        resize_callback_(
            static_cast<UINT>(render_width),
            static_cast<UINT>(height));
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

    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED) {
            const int control_id = LOWORD(wparam);
            if (control_id == kRefreshButtonId && refresh_callback_) {
                refresh_callback_();
                return 0;
            }
            if (control_id == kSaveButtonId && save_callback_) {
                save_callback_();
                return 0;
            }
        }
        break;

    case diagnostics::WM_FPS_DIAGNOSTICS_READY:
        if (diagnostics_ready_callback_) {
            diagnostics_ready_callback_();
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
