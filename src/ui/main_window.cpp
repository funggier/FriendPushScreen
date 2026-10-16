#include "ui/main_window.h"

#include "diagnostics/diagnostics_service.h"
#include "resource.h"
#include "ui/theme.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace fps::ui {
namespace {

constexpr wchar_t kWindowClassName[] =
    L"FriendPushScreenMainWindow";
constexpr wchar_t kRenderSurfaceClassName[] =
    L"FriendPushScreenRenderSurface";

constexpr UINT kDeferredUiCommandMessage =
    WM_APP + 0x70;

constexpr int kOuterPadding = 12;
constexpr int kStatusHeight = 30;
constexpr int kPanelMaxWidth = 520;
constexpr int kPanelMinWidth = 340;
constexpr int kSettingsToggleButtonId = 2201;
constexpr int kSettingsToggleWidth = 160;

LONG_PTR ViewerWindowStyle(
    bool borderless,
    bool visible) {
    LONG_PTR style =
        borderless
            ? WS_POPUP
            : WS_OVERLAPPEDWINDOW;

    style |= WS_CLIPCHILDREN;

    if (visible) {
        style |= WS_VISIBLE;
    }

    return style;
}

bool KeyWasAlreadyDown(
    const MSG& message) {
    return
        (static_cast<std::uint64_t>(
             message.lParam) &
         (1ULL << 30)) != 0;
}


LRESULT CALLBACK RenderSurfaceProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case WM_ERASEBKGND:
        // D3D owns this surface. Do not let GDI erase
        // the last presented swap-chain image.
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam);
}

}  // namespace

ViewerViewport CalculateViewerViewport(
    int client_width,
    int client_height,
    int status_height) {
    const int safe_width =
        std::max(0, client_width);
    const int safe_height =
        std::max(0, client_height);
    const int safe_status_height =
        std::clamp(
            status_height,
            0,
            safe_height);

    return ViewerViewport{
        0,
        safe_status_height,
        safe_width,
        std::max(
            0,
            safe_height -
                safe_status_height)};
}

bool ShouldDeferUiCommand(
    UiCommandType type) {
    switch (type) {
    case UiCommandType::SelectCaptureSource:
    case UiCommandType::ToggleCapturePreview:
    case UiCommandType::RefreshCaptureSources:
    case UiCommandType::ToggleShowCursor:
        return true;
    default:
        return false;
    }
}



std::wstring NormalizeWin32MultilineText(
    std::wstring_view text) {
    std::wstring output;
    output.reserve(text.size() + 16);

    wchar_t previous = L'\0';
    for (const wchar_t ch : text) {
        if (ch == L'\n' &&
            previous != L'\r') {
            output.push_back(L'\r');
        }

        output.push_back(ch);
        previous = ch;
    }

    return output;
}

void MainWindow::SetResizeCallback(
    ResizeCallback callback) {
    resize_callback_ = std::move(callback);
}

void MainWindow::SetUiCommandCallback(
    UiCommandCallback callback) {
    ui_command_callback_ =
        std::move(callback);

    settings_panel_.SetCommandCallback(
        [this](const UiCommand& command) {
            Emit(command);
        });
}

void MainWindow::SetDiagnosticsReadyCallback(
    CommandCallback callback) {
    diagnostics_ready_callback_ =
        std::move(callback);
}

void MainWindow::Emit(
    UiCommand command) {
    if (!ui_command_callback_) {
        return;
    }

    if (hwnd_ != nullptr &&
        ShouldDeferUiCommand(
            command.type)) {
        deferred_ui_commands_.
            push_back(
                std::move(command));

        if (PostMessageW(
                hwnd_,
                kDeferredUiCommandMessage,
                0,
                0) != FALSE) {
            return;
        }

        command = std::move(
            deferred_ui_commands_.back());
        deferred_ui_commands_.pop_back();
    }

    ui_command_callback_(command);
}

void MainWindow::DispatchDeferredUiCommand() {
    if (deferred_ui_commands_.empty()) {
        return;
    }

    UiCommand command = std::move(
        deferred_ui_commands_.front());
    deferred_ui_commands_.pop_front();

    if (ui_command_callback_) {
        ui_command_callback_(command);
    }
}

void MainWindow::SetStatusText(
    std::wstring_view text) {
    if (hwnd_ == nullptr) {
        return;
    }

    std::wstring title{
        L"FriendPushScreen - "};
    title.append(text);

    SetWindowTextW(
        hwnd_,
        title.c_str());
}

bool MainWindow::SetDiagnosticsText(
    std::wstring_view text) {
    return settings_panel_.
        SetDiagnosticsText(text);
}

void MainWindow::SetCollecting(
    bool collecting) {
    settings_panel_.
        SetCollecting(collecting);
}

void MainWindow::SetCaptureSources(
    const std::vector<capture::CaptureSourceDescriptor>& sources,
    std::string_view selected_source_id,
    bool preview_active,
    bool show_cursor,
    std::string_view status_text) {
    settings_panel_.SetCaptureSources(
        sources,
        selected_source_id,
        preview_active,
        show_cursor,
        status_text);
}

std::optional<std::filesystem::path>
MainWindow::PromptDiagnosticsSavePath() {
    std::array<wchar_t, MAX_PATH> path{};
    wcscpy_s(
        path.data(),
        path.size(),
        L"FriendPushScreen-diagnostics.txt");

    constexpr wchar_t filter[] =
        L"Text files (*.txt)\0*.txt\0"
        L"All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize =
        sizeof(dialog);
    dialog.hwndOwner = hwnd_;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile =
        static_cast<DWORD>(
            path.size());
    dialog.lpstrDefExt = L"txt";
    dialog.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&dialog) ==
        FALSE) {
        return std::nullopt;
    }

    return std::filesystem::path{
        path.data()};
}

bool MainWindow::Create(
    HINSTANCE instance,
    int show_command) {
    WNDCLASSEXW window_class{};
    window_class.cbSize =
        sizeof(window_class);
    window_class.style =
        CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc =
        &MainWindow::WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon =
        static_cast<HICON>(
            LoadImageW(
                instance,
                MAKEINTRESOURCEW(
                    IDI_FRIENDPUSHSCREEN),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXICON),
                GetSystemMetrics(SM_CYICON),
                LR_DEFAULTCOLOR |
                    LR_SHARED));
    window_class.hIconSm =
        static_cast<HICON>(
            LoadImageW(
                instance,
                MAKEINTRESOURCEW(
                    IDI_FRIENDPUSHSCREEN),
                IMAGE_ICON,
                GetSystemMetrics(SM_CXSMICON),
                GetSystemMetrics(SM_CYSMICON),
                LR_DEFAULTCOLOR |
                    LR_SHARED));
    window_class.hbrBackground =
        reinterpret_cast<HBRUSH>(
            GetStockObject(
                BLACK_BRUSH));
    window_class.lpszClassName =
        kWindowClassName;

    if (RegisterClassExW(
            &window_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    WNDCLASSEXW render_class{};
    render_class.cbSize =
        sizeof(render_class);
    render_class.lpfnWndProc =
        RenderSurfaceProc;
    render_class.hInstance = instance;
    render_class.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    render_class.hbrBackground = nullptr;
    render_class.lpszClassName =
        kRenderSurfaceClassName;

    if (RegisterClassExW(
            &render_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"FriendPushScreen",
        WS_OVERLAPPEDWINDOW |
            WS_CLIPCHILDREN,
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

    const BOOL use_dark_mode = TRUE;
    constexpr DWORD kImmersiveDarkModeAttribute = 20;
    DwmSetWindowAttribute(
        hwnd_,
        kImmersiveDarkModeAttribute,
        &use_dark_mode,
        sizeof(use_dark_mode));

    render_hwnd_ = CreateWindowExW(
        0,
        kRenderSurfaceClassName,
        nullptr,
        WS_CHILD |
            WS_VISIBLE |
            WS_CLIPSIBLINGS,
        0,
        0,
        1,
        1,
        hwnd_,
        nullptr,
        instance,
        nullptr);

    status_bar_ = CreateWindowExW(
        0,
        L"STATIC",
        L"  Peer: Offline  |  "
        L"Connection: Disconnected  |  "
        L"Sharing: Not sharing",
        WS_CHILD |
            WS_VISIBLE |
            WS_BORDER |
            WS_CLIPSIBLINGS |
            SS_LEFT |
            SS_CENTERIMAGE,
        0,
        0,
        1,
        1,
        hwnd_,
        nullptr,
        instance,
        nullptr);

    settings_toggle_button_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Hide Settings (Tab)",
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            WS_CLIPSIBLINGS |
            BS_OWNERDRAW,
        0,
        0,
        1,
        1,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(
                kSettingsToggleButtonId)),
        instance,
        nullptr);

    if (render_hwnd_ == nullptr ||
        status_bar_ == nullptr ||
        settings_toggle_button_ == nullptr ||
        !settings_panel_.Create(
            hwnd_,
            instance)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    const HFONT font =
        static_cast<HFONT>(
            GetStockObject(
                DEFAULT_GUI_FONT));

    SendMessageW(
        status_bar_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            font),
        TRUE);

    SendMessageW(
        settings_toggle_button_,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(
            font),
        TRUE);

    theme::ApplyDarkBorder(status_bar_);
    theme::ApplyDarkBorder(
        settings_toggle_button_);

    settings_panel_.SetCommandCallback(
        [this](const UiCommand& command) {
            Emit(command);
        });

    LayoutChildren();

    ShowWindow(
        hwnd_,
        show_command);
    UpdateWindow(hwnd_);

    return true;
}

void MainWindow::LayoutChildren() {
    if (hwnd_ == nullptr ||
        render_hwnd_ == nullptr ||
        status_bar_ == nullptr) {
        return;
    }

    RECT client{};
    if (GetClientRect(
            hwnd_,
            &client) == FALSE) {
        return;
    }

    const int width =
        std::max(
            0,
            static_cast<int>(
                client.right -
                client.left));

    const int height =
        std::max(
            0,
            static_cast<int>(
                client.bottom -
                client.top));

    const bool show_status_chrome =
        !applied_fullscreen_;

    const ViewerViewport viewer =
        CalculateViewerViewport(
            width,
            height,
            show_status_chrome
                ? kStatusHeight
                : 0);

    SetWindowPos(
        render_hwnd_,
        HWND_BOTTOM,
        viewer.x,
        viewer.y,
        viewer.width,
        viewer.height,
        SWP_NOACTIVATE);

    int panel_width =
        std::clamp(
            (width * 42) / 100,
            kPanelMinWidth,
            kPanelMaxWidth);

    panel_width =
        std::min(
            panel_width,
            std::max(
                1,
                width -
                    (2 * kOuterPadding)));

    const int panel_y =
        viewer.y +
        kOuterPadding;
    const int panel_height =
        std::max(
            1,
            height -
                panel_y -
                kOuterPadding);

    if (settings_visible_) {
        settings_panel_.SetBounds(
            width -
                panel_width -
                kOuterPadding,
            panel_y,
            panel_width,
            panel_height);
    }

    settings_panel_.Show(
        settings_visible_);

    ShowWindow(
        status_bar_,
        show_status_chrome
            ? SW_SHOW
            : SW_HIDE);
    ShowWindow(
        settings_toggle_button_,
        show_status_chrome
            ? SW_SHOW
            : SW_HIDE);

    if (show_status_chrome) {
        const int toggle_x =
            std::max(
                kOuterPadding,
                width -
                    kOuterPadding -
                    kSettingsToggleWidth);

        SetWindowPos(
            status_bar_,
            HWND_TOP,
            0,
            0,
            width,
            kStatusHeight,
            SWP_NOACTIVATE);

        SetWindowPos(
            settings_toggle_button_,
            HWND_TOP,
            toggle_x,
            0,
            kSettingsToggleWidth,
            kStatusHeight,
            SWP_NOACTIVATE |
                SWP_SHOWWINDOW);
    }

    if (settings_visible_) {
        SetWindowPos(
            settings_panel_.Handle(),
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOACTIVATE);
    }

    if (show_status_chrome) {
        SetWindowPos(
            settings_toggle_button_,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOACTIVATE |
                SWP_SHOWWINDOW);
    }

    if (resize_callback_) {
        resize_callback_(
            static_cast<UINT>(
                viewer.width),
            static_cast<UINT>(
                viewer.height));
    }
}

std::wstring MainWindow::BuildStatusSummary(
    const UiState& state) const {
    std::wstring text =
        L"  Peer: Offline  |  Connection: ";

    switch (state.connection_state) {
    case ConnectionUiState::Disconnected:
        text += L"Disconnected";
        break;
    case ConnectionUiState::ConnectRequested:
        text += L"Requested";
        break;
    case ConnectionUiState::Connected:
        text += L"Connected";
        break;
    }

    text += L"  |  Sharing: ";

    switch (state.share_state) {
    case ShareUiState::NotSharing:
        text += L"Not sharing";
        break;
    case ShareUiState::ShareRequested:
        text += L"Requested";
        break;
    case ShareUiState::Sharing:
        text += L"Sharing";
        break;
    }

    text += L"  |  Incoming Audio: ";

    if (state.preferences.receiver_muted) {
        text += L"Muted";
    } else {
        text += L"Inactive";
    }

    return text;
}

void MainWindow::ApplyUiState(
    const UiState& state) {
    const bool visibility_changed =
        settings_visible_ !=
        state.preferences.settings_visible;

    settings_visible_ =
        state.preferences.settings_visible;

    if (settings_toggle_button_ != nullptr) {
        SetWindowTextW(
            settings_toggle_button_,
            settings_visible_
                ? L"Hide Settings (Tab)"
                : L"Settings (Tab)");
    }

    settings_panel_.ApplyState(state);

    if (status_bar_ != nullptr) {
        const std::wstring status =
            BuildStatusSummary(state);

        SetWindowTextW(
            status_bar_,
            status.c_str());
    }

    ApplyWindowState(state);
    LayoutChildren();

    if (visibility_changed &&
        hwnd_ != nullptr) {
        RedrawWindow(
            hwnd_,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
                RDW_ERASE |
                RDW_FRAME |
                RDW_ALLCHILDREN |
                RDW_UPDATENOW);
    }
}

void MainWindow::ApplyAlwaysOnTop(
    bool enabled) {
    if (hwnd_ == nullptr) {
        return;
    }

    SetWindowPos(
        hwnd_,
        enabled
            ? HWND_TOPMOST
            : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
            SWP_NOSIZE |
            SWP_NOACTIVATE);

    applied_topmost_ = enabled;
}

void MainWindow::ApplyBorderless(
    bool borderless) {
    if (hwnd_ == nullptr ||
        applied_fullscreen_) {
        return;
    }

    RECT rect{};
    GetWindowRect(hwnd_, &rect);

    const bool visible =
        IsWindowVisible(hwnd_) != FALSE;

    SetWindowLongPtrW(
        hwnd_,
        GWL_STYLE,
        ViewerWindowStyle(
            borderless,
            visible));

    SetWindowPos(
        hwnd_,
        nullptr,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOZORDER |
            SWP_NOACTIVATE |
            SWP_FRAMECHANGED);

    applied_borderless_ = borderless;
}

void MainWindow::EnterFullscreen(
    const UiState& state) {
    if (hwnd_ == nullptr ||
        applied_fullscreen_) {
        return;
    }

    windowed_placement_.length =
        sizeof(windowed_placement_);

    have_windowed_placement_ =
        GetWindowPlacement(
            hwnd_,
            &windowed_placement_) !=
        FALSE;

    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);

    const HMONITOR monitor_handle =
        MonitorFromWindow(
            hwnd_,
            MONITOR_DEFAULTTONEAREST);

    if (GetMonitorInfoW(
            monitor_handle,
            &monitor) == FALSE) {
        return;
    }

    const bool visible =
        IsWindowVisible(hwnd_) != FALSE;

    SetWindowLongPtrW(
        hwnd_,
        GWL_STYLE,
        ViewerWindowStyle(
            true,
            visible));

    SetWindowPos(
        hwnd_,
        state.preferences.always_on_top
            ? HWND_TOPMOST
            : HWND_TOP,
        monitor.rcMonitor.left,
        monitor.rcMonitor.top,
        monitor.rcMonitor.right -
            monitor.rcMonitor.left,
        monitor.rcMonitor.bottom -
            monitor.rcMonitor.top,
        SWP_NOACTIVATE |
            SWP_FRAMECHANGED);

    applied_fullscreen_ = true;
}

void MainWindow::ExitFullscreen(
    const UiState& state) {
    if (hwnd_ == nullptr ||
        !applied_fullscreen_) {
        return;
    }

    const bool visible =
        IsWindowVisible(hwnd_) != FALSE;

    SetWindowLongPtrW(
        hwnd_,
        GWL_STYLE,
        ViewerWindowStyle(
            state.preferences.borderless,
            visible));

    if (have_windowed_placement_) {
        SetWindowPlacement(
            hwnd_,
            &windowed_placement_);
    }

    SetWindowPos(
        hwnd_,
        state.preferences.always_on_top
            ? HWND_TOPMOST
            : HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
            SWP_NOSIZE |
            SWP_NOACTIVATE |
            SWP_FRAMECHANGED);

    applied_fullscreen_ = false;
    applied_borderless_ =
        state.preferences.borderless;
}

void MainWindow::ApplyWindowState(
    const UiState& state) {
    if (state.fullscreen !=
        applied_fullscreen_) {
        if (state.fullscreen) {
            EnterFullscreen(state);
        } else {
            ExitFullscreen(state);
        }
    }

    if (!state.fullscreen &&
        state.preferences.borderless !=
            applied_borderless_) {
        ApplyBorderless(
            state.preferences.borderless);
    }

    if (state.preferences.always_on_top !=
        applied_topmost_) {
        ApplyAlwaysOnTop(
            state.preferences.always_on_top);
    }
}

bool MainWindow::HandleShortcut(
    const MSG& message) {
    if (message.message != WM_KEYDOWN &&
        message.message !=
            WM_SYSKEYDOWN) {
        return false;
    }

    const int key =
        static_cast<int>(
            message.wParam);

    const bool control =
        (GetKeyState(VK_CONTROL) &
         0x8000) != 0;

    const bool shift =
        (GetKeyState(VK_SHIFT) &
         0x8000) != 0;

    UiCommandType command{};
    bool matched = true;

    if (key == VK_TAB &&
        !control &&
        !shift) {
        command =
            UiCommandType::
                ToggleSettings;
    } else if (key == VK_F11) {
        command =
            UiCommandType::
                ToggleFullscreen;
    } else if (key == VK_ESCAPE) {
        command =
            UiCommandType::
                ExitFullscreen;
    } else if (control &&
               shift &&
               key == 'T') {
        command =
            UiCommandType::
                ToggleAlwaysOnTop;
    } else if (control &&
               shift &&
               key == 'S') {
        command =
            UiCommandType::
                ToggleShareRequest;
    } else {
        matched = false;
    }

    if (!matched) {
        return false;
    }

    if (!KeyWasAlreadyDown(message)) {
        Emit(UiCommand{command});
    }

    return true;
}

LRESULT CALLBACK MainWindow::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    MainWindow* self =
        reinterpret_cast<MainWindow*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<
                CREATESTRUCTW*>(lparam);

        self =
            static_cast<MainWindow*>(
                create->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                self));
    }

    if (self != nullptr) {
        return self->HandleMessage(
            hwnd,
            message,
            wparam,
            lparam);
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam);
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

    case WM_COMMAND:
        if (LOWORD(wparam) ==
                kSettingsToggleButtonId &&
            HIWORD(wparam) ==
                BN_CLICKED) {
            Emit(UiCommand{
                UiCommandType::ToggleSettings
            });
            return 0;
        }
        break;

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<DRAWITEMSTRUCT*>(
                lparam);

        if (item != nullptr &&
            item->CtlType == ODT_BUTTON &&
            item->CtlID ==
                kSettingsToggleButtonId) {
            theme::DrawButton(*item);
            return TRUE;
        }

        break;
    }

    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(
                lparam) ==
            status_bar_) {
            const HDC dc =
                reinterpret_cast<HDC>(
                    wparam);

            SetTextColor(
                dc,
                theme::kTextPrimary);
            SetBkColor(
                dc,
                theme::kPanelBackground);

            return reinterpret_cast<LRESULT>(
                theme::PanelBrush());
        }
        break;

    case kDeferredUiCommandMessage:
        DispatchDeferredUiCommand();
        return 0;

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
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam);
}

}  // namespace fps::ui
