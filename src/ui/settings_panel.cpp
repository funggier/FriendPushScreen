#include "ui/settings_panel.h"

#include <CommCtrl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace fps::ui {
namespace {

constexpr wchar_t kSettingsPanelClassName[] =
    L"FriendPushScreenSettingsPanel";

constexpr int kShareButtonId = 2101;
constexpr int kConnectionModeId = 2102;
constexpr int kHostEditId = 2103;
constexpr int kPortEditId = 2104;
constexpr int kConnectButtonId = 2105;
constexpr int kAudioSourceId = 2106;
constexpr int kMuteButtonId = 2107;
constexpr int kVolumeSliderId = 2108;
constexpr int kTopmostId = 2109;
constexpr int kBorderlessId = 2110;
constexpr int kFullscreenId = 2111;
constexpr int kDiagnosticsToggleId = 2112;
constexpr int kRefreshButtonId = 2113;
constexpr int kSaveButtonId = 2114;

constexpr int kPadding = 16;
constexpr int kGap = 8;
constexpr int kButtonHeight = 32;
constexpr int kFieldHeight = 28;
constexpr int kLineScroll = 48;

std::wstring NormalizeMultiline(
    std::wstring_view text) {
    std::wstring output;
    output.reserve(text.size() + 32);

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

void SetTextIfDifferent(
    HWND hwnd,
    const std::wstring& value) {
    if (hwnd == nullptr) {
        return;
    }

    const int length = GetWindowTextLengthW(hwnd);
    std::wstring current(
        static_cast<std::size_t>(
            std::max(0, length)),
        L'\0');

    if (length > 0) {
        std::vector<wchar_t> buffer(
            static_cast<std::size_t>(length + 1),
            L'\0');
        GetWindowTextW(
            hwnd,
            buffer.data(),
            length + 1);
        current.assign(buffer.data());
    }

    if (current != value) {
        SetWindowTextW(
            hwnd,
            value.c_str());
    }
}

std::wstring PortText(
    std::uint32_t port) {
    return std::to_wstring(port);
}

}  // namespace

bool SettingsPanel::Create(
    HWND parent,
    HINSTANCE instance) {
    INITCOMMONCONTROLSEX common_controls{};
    common_controls.dwSize =
        sizeof(common_controls);
    common_controls.dwICC =
        ICC_BAR_CLASSES;
    InitCommonControlsEx(&common_controls);

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc =
        &SettingsPanel::WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        reinterpret_cast<HBRUSH>(
            COLOR_WINDOW + 1);
    window_class.lpszClassName =
        kSettingsPanelClassName;

    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        kSettingsPanelClassName,
        nullptr,
        WS_CHILD |
            WS_VISIBLE |
            WS_VSCROLL |
            WS_CLIPCHILDREN |
            WS_CLIPSIBLINGS,
        0,
        0,
        1,
        1,
        parent,
        nullptr,
        instance,
        this);

    if (hwnd_ == nullptr) {
        return false;
    }

    InitializeControls(instance);
    ApplyDefaultFont();
    LayoutControls();
    return true;
}

void SettingsPanel::SetCommandCallback(
    CommandCallback callback) {
    command_callback_ = std::move(callback);
}

void SettingsPanel::SetBounds(
    int x,
    int y,
    int width,
    int height) {
    if (hwnd_ == nullptr) {
        return;
    }

    SetWindowPos(
        hwnd_,
        HWND_TOP,
        x,
        y,
        std::max(1, width),
        std::max(1, height),
        SWP_NOACTIVATE);

    LayoutControls();
}

void SettingsPanel::Show(bool visible) {
    if (hwnd_ == nullptr) {
        return;
    }

    ShowWindow(
        hwnd_,
        visible ? SW_SHOW : SW_HIDE);
}


HWND SettingsPanel::CreateLabel(
    HINSTANCE instance,
    const wchar_t* text) {
    return CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD |
            WS_VISIBLE |
            SS_LEFT,
        0,
        0,
        1,
        1,
        hwnd_,
        nullptr,
        instance,
        nullptr);
}

HWND SettingsPanel::CreateButton(
    HINSTANCE instance,
    int id,
    const wchar_t* text,
    DWORD style) {
    return CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            style,
        0,
        0,
        1,
        1,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(id)),
        instance,
        nullptr);
}

HWND SettingsPanel::CreateEdit(
    HINSTANCE instance,
    int id,
    DWORD style) {
    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        nullptr,
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            style,
        0,
        0,
        1,
        1,
        hwnd_,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(id)),
        instance,
        nullptr);
}

void SettingsPanel::InitializeControls(
    HINSTANCE instance) {
    title_label_ =
        CreateLabel(
            instance,
            L"Settings / Control");

    share_heading_ =
        CreateLabel(instance, L"SHARE");
    share_button_ =
        CreateButton(
            instance,
            kShareButtonId,
            L"Start Share");
    share_status_ =
        CreateLabel(
            instance,
            L"Not sharing");

    connection_heading_ =
        CreateLabel(
            instance,
            L"CONNECTION");
    connection_mode_label_ =
        CreateLabel(
            instance,
            L"Connection Mode");
    connection_mode_combo_ =
        CreateWindowExW(
            0,
            WC_COMBOBOXW,
            nullptr,
            WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0,
            0,
            1,
            1,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    kConnectionModeId)),
            instance,
            nullptr);

    SendMessageW(
        connection_mode_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"Auto"));
    SendMessageW(
        connection_mode_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"LAN"));
    SendMessageW(
        connection_mode_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Direct IP / DDNS"));
    SendMessageW(
        connection_mode_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Tailscale"));

    host_label_ =
        CreateLabel(instance, L"Host");
    host_edit_ =
        CreateEdit(
            instance,
            kHostEditId);
    port_label_ =
        CreateLabel(instance, L"Port");
    port_edit_ =
        CreateEdit(
            instance,
            kPortEditId,
            ES_AUTOHSCROLL |
                ES_NUMBER);
    connect_button_ =
        CreateButton(
            instance,
            kConnectButtonId,
            L"Connect");
    connection_status_ =
        CreateLabel(
            instance,
            L"Disconnected");

    audio_heading_ =
        CreateLabel(
            instance,
            L"AUDIO");
    audio_source_label_ =
        CreateLabel(
            instance,
            L"Audio Source");
    audio_source_combo_ =
        CreateWindowExW(
            0,
            WC_COMBOBOXW,
            nullptr,
            WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                CBS_DROPDOWNLIST |
                WS_VSCROLL,
            0,
            0,
            1,
            1,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    kAudioSourceId)),
            instance,
            nullptr);

    SendMessageW(
        audio_source_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"No Audio"));
    SendMessageW(
        audio_source_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Desktop / System Audio"));
    SendMessageW(
        audio_source_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Device Source"));
    SendMessageW(
        audio_source_combo_,
        CB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Application Audio"));

    audio_status_ =
        CreateLabel(
            instance,
            L"Audio disabled");
    mute_button_ =
        CreateButton(
            instance,
            kMuteButtonId,
            L"Mute Remote Audio");

    volume_slider_ =
        CreateWindowExW(
            0,
            TRACKBAR_CLASSW,
            nullptr,
            WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                TBS_HORZ |
                TBS_AUTOTICKS,
            0,
            0,
            1,
            1,
            hwnd_,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    kVolumeSliderId)),
            instance,
            nullptr);

    SendMessageW(
        volume_slider_,
        TBM_SETRANGE,
        TRUE,
        MAKELONG(0, 100));
    SendMessageW(
        volume_slider_,
        TBM_SETTICFREQ,
        10,
        0);

    volume_label_ =
        CreateLabel(
            instance,
            L"Volume: 100%");

    viewer_heading_ =
        CreateLabel(
            instance,
            L"VIEWER");
    topmost_checkbox_ =
        CreateButton(
            instance,
            kTopmostId,
            L"Always on Top",
            BS_AUTOCHECKBOX);
    borderless_checkbox_ =
        CreateButton(
            instance,
            kBorderlessId,
            L"Borderless Viewer",
            BS_AUTOCHECKBOX);
    fullscreen_button_ =
        CreateButton(
            instance,
            kFullscreenId,
            L"Enter Fullscreen (F11)");

    advanced_heading_ =
        CreateLabel(
            instance,
            L"ADVANCED");
    diagnostics_toggle_ =
        CreateButton(
            instance,
            kDiagnosticsToggleId,
            L"Show Diagnostics");

    diagnostics_edit_ =
        CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"Collecting diagnostics...",
            WS_CHILD |
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

    SendMessageW(
        diagnostics_edit_,
        EM_SETLIMITTEXT,
        static_cast<WPARAM>(
            1024 * 1024),
        0);

    refresh_button_ =
        CreateButton(
            instance,
            kRefreshButtonId,
            L"Refresh");
    save_button_ =
        CreateButton(
            instance,
            kSaveButtonId,
            L"Save Diagnostics");

    UpdateDiagnosticsVisibility(false);
}

void SettingsPanel::ApplyDefaultFont() {
    const HFONT font =
        static_cast<HFONT>(
            GetStockObject(
                DEFAULT_GUI_FONT));

    const std::array<HWND, 28> controls{{
        title_label_,
        share_heading_,
        share_button_,
        share_status_,
        connection_heading_,
        connection_mode_label_,
        connection_mode_combo_,
        host_label_,
        host_edit_,
        port_label_,
        port_edit_,
        connect_button_,
        connection_status_,
        audio_heading_,
        audio_source_label_,
        audio_source_combo_,
        audio_status_,
        mute_button_,
        volume_slider_,
        volume_label_,
        viewer_heading_,
        topmost_checkbox_,
        borderless_checkbox_,
        fullscreen_button_,
        advanced_heading_,
        diagnostics_toggle_,
        diagnostics_edit_,
        refresh_button_
    }};

    for (HWND control : controls) {
        if (control != nullptr) {
            SendMessageW(
                control,
                WM_SETFONT,
                reinterpret_cast<WPARAM>(
                    font),
                TRUE);
        }
    }

    if (save_button_ != nullptr) {
        SendMessageW(
            save_button_,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font),
            TRUE);
    }
}

void SettingsPanel::LayoutControls() {
    if (hwnd_ == nullptr) {
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
            1,
            static_cast<int>(
                client.right - client.left));

    const int field_width =
        std::max(
            240,
            width - (2 * kPadding));

    const auto move =
        [this](
            HWND control,
            int x,
            int y,
            int control_width,
            int control_height) {
            if (control == nullptr) {
                return;
            }

            SetWindowPos(
                control,
                nullptr,
                x,
                y - scroll_offset_,
                std::max(1, control_width),
                std::max(1, control_height),
                SWP_NOZORDER |
                    SWP_NOACTIVATE);
        };

    move(
        title_label_,
        kPadding,
        14,
        field_width,
        28);

    move(
        share_heading_,
        kPadding,
        54,
        field_width,
        20);
    move(
        share_button_,
        kPadding,
        78,
        field_width,
        kButtonHeight);
    move(
        share_status_,
        kPadding,
        116,
        field_width,
        40);

    move(
        connection_heading_,
        kPadding,
        166,
        field_width,
        20);
    move(
        connection_mode_label_,
        kPadding,
        192,
        field_width,
        18);
    move(
        connection_mode_combo_,
        kPadding,
        212,
        field_width,
        260);

    const int host_width =
        std::max(
            160,
            (field_width * 2) / 3 -
                (kGap / 2));
    const int port_width =
        std::max(
            84,
            field_width -
                host_width -
                kGap);

    move(
        host_label_,
        kPadding,
        252,
        host_width,
        18);
    move(
        port_label_,
        kPadding +
            host_width +
            kGap,
        252,
        port_width,
        18);
    move(
        host_edit_,
        kPadding,
        272,
        host_width,
        kFieldHeight);
    move(
        port_edit_,
        kPadding +
            host_width +
            kGap,
        272,
        port_width,
        kFieldHeight);
    move(
        connect_button_,
        kPadding,
        310,
        field_width,
        kButtonHeight);
    move(
        connection_status_,
        kPadding,
        350,
        field_width,
        48);

    move(
        audio_heading_,
        kPadding,
        410,
        field_width,
        20);
    move(
        audio_source_label_,
        kPadding,
        436,
        field_width,
        18);
    move(
        audio_source_combo_,
        kPadding,
        456,
        field_width,
        260);
    move(
        audio_status_,
        kPadding,
        496,
        field_width,
        56);

    const int mute_width = 140;
    move(
        mute_button_,
        kPadding,
        562,
        mute_width,
        kButtonHeight);
    move(
        volume_slider_,
        kPadding +
            mute_width +
            kGap,
        558,
        std::max(
            100,
            field_width -
                mute_width -
                kGap),
        40);
    move(
        volume_label_,
        kPadding,
        602,
        field_width,
        20);

    move(
        viewer_heading_,
        kPadding,
        642,
        field_width,
        20);
    move(
        topmost_checkbox_,
        kPadding,
        668,
        field_width,
        26);
    move(
        borderless_checkbox_,
        kPadding,
        698,
        field_width,
        26);
    move(
        fullscreen_button_,
        kPadding,
        732,
        field_width,
        kButtonHeight);

    move(
        advanced_heading_,
        kPadding,
        786,
        field_width,
        20);
    move(
        diagnostics_toggle_,
        kPadding,
        812,
        field_width,
        kButtonHeight);

    if (diagnostics_visible_) {
        move(
            diagnostics_edit_,
            kPadding,
            854,
            field_width,
            280);

        const int button_width =
            std::max(
                100,
                (field_width - kGap) / 2);

        move(
            refresh_button_,
            kPadding,
            1144,
            button_width,
            kButtonHeight);
        move(
            save_button_,
            kPadding +
                button_width +
                kGap,
            1144,
            button_width,
            kButtonHeight);

        content_height_ = 1192;
    } else {
        content_height_ = 862;
    }

    UpdateScrollBar();

    RedrawWindow(
        hwnd_,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN);
}

void SettingsPanel::UpdateScrollBar() {
    if (hwnd_ == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);

    const int page =
        std::max(
            1,
            static_cast<int>(
                client.bottom -
                client.top));

    const int max_offset =
        std::max(
            0,
            content_height_ - page);

    scroll_offset_ =
        std::clamp(
            scroll_offset_,
            0,
            max_offset);

    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask =
        SIF_RANGE |
        SIF_PAGE |
        SIF_POS;
    info.nMin = 0;
    info.nMax =
        std::max(0, content_height_ - 1);
    info.nPage =
        static_cast<UINT>(page);
    info.nPos = scroll_offset_;

    SetScrollInfo(
        hwnd_,
        SB_VERT,
        &info,
        TRUE);
}

void SettingsPanel::SetScrollOffset(
    int offset) {
    RECT client{};
    GetClientRect(hwnd_, &client);

    const int page =
        std::max(
            1,
            static_cast<int>(
                client.bottom -
                client.top));

    const int max_offset =
        std::max(
            0,
            content_height_ - page);

    const int clamped =
        std::clamp(
            offset,
            0,
            max_offset);

    if (clamped == scroll_offset_) {
        return;
    }

    scroll_offset_ = clamped;
    LayoutControls();
}

void SettingsPanel::Emit(
    UiCommand command) {
    if (applying_state_ ||
        !command_callback_) {
        return;
    }

    command_callback_(command);
}

std::string SettingsPanel::ReadEditUtf8(
    HWND edit) const {
    if (edit == nullptr) {
        return {};
    }

    const int length =
        GetWindowTextLengthW(edit);
    if (length <= 0) {
        return {};
    }

    std::vector<wchar_t> wide(
        static_cast<std::size_t>(
            length + 1),
        L'\0');

    GetWindowTextW(
        edit,
        wide.data(),
        length + 1);

    const int required =
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wide.data(),
            length,
            nullptr,
            0,
            nullptr,
            nullptr);

    if (required <= 0) {
        return {};
    }

    std::string utf8(
        static_cast<std::size_t>(
            required),
        '\0');

    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wide.data(),
            length,
            utf8.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }

    return utf8;
}

std::uint32_t SettingsPanel::ReadPort() const {
    if (port_edit_ == nullptr) {
        return 0;
    }

    std::array<wchar_t, 16> buffer{};
    GetWindowTextW(
        port_edit_,
        buffer.data(),
        static_cast<int>(
            buffer.size()));

    wchar_t* end = nullptr;
    const unsigned long value =
        wcstoul(
            buffer.data(),
            &end,
            10);

    if (end == buffer.data() ||
        *end != L'\0' ||
        value < 1 ||
        value > 65535) {
        return 0;
    }

    return static_cast<std::uint32_t>(
        value);
}

void SettingsPanel::UpdateDiagnosticsVisibility(
    bool visible) {
    diagnostics_visible_ = visible;

    const int command =
        visible ? SW_SHOW : SW_HIDE;

    ShowWindow(
        diagnostics_edit_,
        command);
    ShowWindow(
        refresh_button_,
        command);
    ShowWindow(
        save_button_,
        command);

    SetWindowTextW(
        diagnostics_toggle_,
        visible
            ? L"Hide Diagnostics"
            : L"Show Diagnostics");

    LayoutControls();
}

void SettingsPanel::ApplyState(
    const UiState& state) {
    applying_state_ = true;

    SendMessageW(
        connection_mode_combo_,
        CB_SETCURSEL,
        static_cast<WPARAM>(
            state.preferences.connection_mode),
        0);

    std::wstring host;
    if (!state.preferences.host.empty()) {
        const int required =
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                state.preferences.host.data(),
                static_cast<int>(
                    state.preferences.host.size()),
                nullptr,
                0);

        if (required > 0) {
            host.resize(
                static_cast<std::size_t>(
                    required));
            MultiByteToWideChar(
                CP_UTF8,
                MB_ERR_INVALID_CHARS,
                state.preferences.host.data(),
                static_cast<int>(
                    state.preferences.host.size()),
                host.data(),
                required);
        }
    }

    SetTextIfDifferent(
        host_edit_,
        host);
    SetTextIfDifferent(
        port_edit_,
        PortText(
            state.preferences.port));

    SendMessageW(
        audio_source_combo_,
        CB_SETCURSEL,
        static_cast<WPARAM>(
            state.preferences.audio_source),
        0);

    const bool share_requested =
        state.share_state ==
        ShareUiState::ShareRequested;

    SetWindowTextW(
        share_button_,
        share_requested
            ? L"Cancel Share Request"
            : L"Start Share");

    SetWindowTextW(
        share_status_,
        share_requested
            ? L"Share requested - media pipeline is not active in FPS-004."
            : L"Not sharing");

    const bool connect_requested =
        state.connection_state ==
        ConnectionUiState::ConnectRequested;

    SetWindowTextW(
        connect_button_,
        connect_requested
            ? L"Cancel Connect Request"
            : L"Connect");

    SetWindowTextW(
        connection_status_,
        connect_requested
            ? L"Connect requested - transport is not implemented in FPS-004."
            : L"Disconnected - choose a mode/endpoint, then Connect.");

    switch (state.preferences.audio_source) {
    case AudioSourceKind::None:
        SetWindowTextW(
            audio_status_,
            state.application_audio_supported
                ? L"Audio disabled. Application Audio capability is available on this Windows build."
                : L"Audio disabled. Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;

    case AudioSourceKind::System:
        SetWindowTextW(
            audio_status_,
            state.application_audio_supported
                ? L"Desktop / System Audio selected - capture starts in FPS-006A."
                : L"Desktop / System Audio selected - capture starts in FPS-006A. Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;

    case AudioSourceKind::Device:
        SetWindowTextW(
            audio_status_,
            state.application_audio_supported
                ? L"Device Source selected - live device enumeration/capture starts in FPS-006A."
                : L"Device Source selected - live device enumeration/capture starts in FPS-006A. Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;

    case AudioSourceKind::Application:
        SetWindowTextW(
            audio_status_,
            state.application_audio_supported
                ? L"Application Audio selected - capture starts in FPS-006A."
                : L"Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;
    }

    SetWindowTextW(
        mute_button_,
        state.preferences.receiver_muted
            ? L"Unmute Remote Audio"
            : L"Mute Remote Audio");

    SendMessageW(
        volume_slider_,
        TBM_SETPOS,
        TRUE,
        state.preferences.receiver_volume);

    const std::wstring volume_text =
        L"Volume: " +
        std::to_wstring(
            state.preferences.receiver_volume) +
        L"%";

    SetWindowTextW(
        volume_label_,
        volume_text.c_str());

    SendMessageW(
        topmost_checkbox_,
        BM_SETCHECK,
        state.preferences.always_on_top
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    SendMessageW(
        borderless_checkbox_,
        BM_SETCHECK,
        state.preferences.borderless
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    SetWindowTextW(
        fullscreen_button_,
        state.fullscreen
            ? L"Exit Fullscreen (Esc)"
            : L"Enter Fullscreen (F11)");

    const bool visibility_changed =
        diagnostics_visible_ !=
        state.diagnostics_visible;

    diagnostics_visible_ =
        state.diagnostics_visible;

    applying_state_ = false;

    if (visibility_changed) {
        UpdateDiagnosticsVisibility(
            diagnostics_visible_);
    } else {
        SetWindowTextW(
            diagnostics_toggle_,
            diagnostics_visible_
                ? L"Hide Diagnostics"
                : L"Show Diagnostics");
        LayoutControls();
    }
}

bool SettingsPanel::SetDiagnosticsText(
    std::wstring_view text) {
    if (diagnostics_edit_ == nullptr) {
        return false;
    }

    const std::wstring normalized =
        NormalizeMultiline(text);

    return SetWindowTextW(
               diagnostics_edit_,
               normalized.c_str()) !=
           FALSE;
}

void SettingsPanel::SetCollecting(
    bool collecting) {
    if (refresh_button_ == nullptr) {
        return;
    }

    EnableWindow(
        refresh_button_,
        collecting ? FALSE : TRUE);

    SetWindowTextW(
        refresh_button_,
        collecting
            ? L"Collecting..."
            : L"Refresh");
}

LRESULT CALLBACK SettingsPanel::WindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    SettingsPanel* self =
        reinterpret_cast<SettingsPanel*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<CREATESTRUCTW*>(
                lparam);

        self =
            static_cast<SettingsPanel*>(
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

LRESULT SettingsPanel::HandleMessage(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case WM_SIZE:
        LayoutControls();
        return 0;

    case WM_MOUSEWHEEL: {
        const int delta =
            GET_WHEEL_DELTA_WPARAM(wparam);
        SetScrollOffset(
            scroll_offset_ -
            ((delta / WHEEL_DELTA) *
             (kLineScroll * 2)));
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask =
            SIF_ALL;
        GetScrollInfo(
            hwnd_,
            SB_VERT,
            &info);

        int target = scroll_offset_;

        switch (LOWORD(wparam)) {
        case SB_LINEUP:
            target -= kLineScroll;
            break;

        case SB_LINEDOWN:
            target += kLineScroll;
            break;

        case SB_PAGEUP:
            target -=
                static_cast<int>(
                    info.nPage);
            break;

        case SB_PAGEDOWN:
            target +=
                static_cast<int>(
                    info.nPage);
            break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            target = info.nTrackPos;
            break;

        case SB_TOP:
            target = 0;
            break;

        case SB_BOTTOM:
            target = content_height_;
            break;

        default:
            return 0;
        }

        SetScrollOffset(target);
        return 0;
    }

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lparam) ==
            volume_slider_) {
            const int volume =
                static_cast<int>(
                    SendMessageW(
                        volume_slider_,
                        TBM_GETPOS,
                        0,
                        0));

            Emit(UiCommand{
                UiCommandType::SetReceiverVolume,
                volume,
                {}
            });
            return 0;
        }
        break;

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int notification =
            HIWORD(wparam);

        if (notification == BN_CLICKED) {
            switch (id) {
            case kShareButtonId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleShareRequest
                });
                return 0;

            case kConnectButtonId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleConnectionRequest
                });
                return 0;

            case kMuteButtonId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleReceiverMute
                });
                return 0;

            case kTopmostId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleAlwaysOnTop
                });
                return 0;

            case kBorderlessId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleBorderless
                });
                return 0;

            case kFullscreenId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleFullscreen
                });
                return 0;

            case kDiagnosticsToggleId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleDiagnostics
                });
                return 0;

            case kRefreshButtonId:
                Emit(UiCommand{
                    UiCommandType::
                        RefreshDiagnostics
                });
                return 0;

            case kSaveButtonId:
                Emit(UiCommand{
                    UiCommandType::
                        SaveDiagnostics
                });
                return 0;

            default:
                break;
            }
        }

        if (id == kConnectionModeId &&
            notification ==
                CBN_SELCHANGE) {
            const int selection =
                static_cast<int>(
                    SendMessageW(
                        connection_mode_combo_,
                        CB_GETCURSEL,
                        0,
                        0));

            if (selection >= 0 &&
                selection <= 3) {
                Emit(UiCommand{
                    UiCommandType::
                        SetConnectionMode,
                    selection,
                    {}
                });
            }
            return 0;
        }

        if (id == kAudioSourceId &&
            notification ==
                CBN_SELCHANGE) {
            const int selection =
                static_cast<int>(
                    SendMessageW(
                        audio_source_combo_,
                        CB_GETCURSEL,
                        0,
                        0));

            if (selection >= 0 &&
                selection <= 3) {
                Emit(UiCommand{
                    UiCommandType::
                        SelectAudioSource,
                    selection,
                    {}
                });
            }
            return 0;
        }

        if (id == kHostEditId &&
            notification ==
                EN_CHANGE) {
            Emit(UiCommand{
                UiCommandType::SetHost,
                0,
                ReadEditUtf8(host_edit_)
            });
            return 0;
        }

        if (id == kPortEditId &&
            notification ==
                EN_CHANGE) {
            Emit(UiCommand{
                UiCommandType::SetPort,
                static_cast<int>(
                    ReadPort()),
                {}
            });
            return 0;
        }

        break;
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

}  // namespace fps::ui
