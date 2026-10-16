#include "ui/settings_panel.h"

#include "ui/theme.h"
#include "media/mft_video_codec_catalog.h"
#include <mfapi.h>
#include <charconv>

#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace fps::ui {
namespace {

constexpr wchar_t kSettingsPanelClassName[] =
    L"FriendPushScreenSettingsPanel";

constexpr int kShareButtonId = 2101;
constexpr int kSharePortEditId = 2115;
constexpr int kConnectionModeId = 2102;
constexpr int kHostEditId = 2103;
constexpr int kRemotePortEditId = 2104;
constexpr int kConnectButtonId = 2105;
constexpr int kExportIdentityId=2127,kPairFriendId=2128,kDisconnectPeerId=2129;
constexpr int kPairSectionHeight=220;
constexpr int kAudioSourceId = 2106;
constexpr int kMuteButtonId = 2107;
constexpr int kVolumeSliderId = 2108;
constexpr int kTopmostId = 2109;
constexpr int kBorderlessId = 2110;
constexpr int kFullscreenId = 2111;
constexpr int kDiagnosticsToggleId = 2112;
constexpr int kRefreshButtonId = 2113;
constexpr int kSaveButtonId = 2114;
constexpr int kSettingsScrollbarId = 2116;
constexpr int kSourceId = 2117;
constexpr int kRefreshSourcesId = 2118;
constexpr int kPreviewSourceId = 2119;
constexpr int kShowCursorId = 2120;
constexpr int kEncoderId = 2121;
constexpr int kDecoderId = 2122;
constexpr int kBitrateId = 2123;
constexpr int kKeyframeId = 2124;
constexpr int kLowLatencyId = 2125;
constexpr int kApplyVideoId = 2126;
constexpr int kVideoSectionHeight = 384;

constexpr int kPadding = 16;
constexpr int kGap = 8;
constexpr int kButtonHeight = 32;
constexpr int kFieldHeight = 28;
constexpr int kLineScroll = 48;
constexpr int kScrollbarWidth = 14;
constexpr int kDropListRowHeight = 24;
constexpr int kSmoothWheelPixels = 72;
constexpr int kSmoothScrollIntervalMs = 15;
constexpr int kVolumeWheelStep = 5;
constexpr UINT_PTR kSmoothScrollTimerId = 41;

constexpr wchar_t kVolumeSliderClassName[] =
    L"FriendPushScreenVolumeSlider";
constexpr UINT kVolumeSetPosMessage =
    WM_APP + 0x62;
constexpr UINT kVolumePositionMessage =
    WM_APP + 0x44;

constexpr wchar_t kSettingsScrollbarClassName[] =
    L"FriendPushScreenSettingsScrollbar";
constexpr UINT kSettingsScrollPositionMessage =
    WM_APP + 0x43;
constexpr UINT kScrollbarSetRangeMessage =
    WM_APP + 0x60;
constexpr UINT kScrollbarSetPosMessage =
    WM_APP + 0x61;

constexpr wchar_t kDropPopupClassName[] =
    L"FriendPushScreenDropPopup";
constexpr UINT kDropSelectionMessage =
    WM_APP + 0x41;
constexpr WPARAM kDropKindConnection = 1;
constexpr WPARAM kDropKindAudio = 2;
constexpr WPARAM kDropKindSource = 3;
constexpr WPARAM kDropKindEncoder = 4;
constexpr WPARAM kDropKindDecoder = 5;

struct DropPopupInit {
    HWND notify{nullptr};
    WPARAM kind{0};
};

struct VolumeSliderState {
    int pos{100};
    bool dragging{false};
};

RECT VolumeSliderThumbRect(
    HWND hwnd,
    int pos) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width =
        std::max(
            1,
            static_cast<int>(
                client.right -
                client.left));
    const int height =
        std::max(
            1,
            static_cast<int>(
                client.bottom -
                client.top));

    const int thumb_width = 14;
    const int thumb_height =
        std::max(14, height - 10);
    const int track_left =
        4 + (thumb_width / 2);
    const int track_right =
        std::max(
            track_left + 1,
            width - 4 -
                (thumb_width / 2));
    const int travel =
        std::max(
            1,
            track_right -
                track_left);
    const int center_x =
        track_left +
        (std::clamp(pos, 0, 100) *
         travel) / 100;

    RECT thumb{};
    thumb.left =
        center_x -
        (thumb_width / 2);
    thumb.right =
        thumb.left +
        thumb_width;
    thumb.top =
        (height - thumb_height) / 2;
    thumb.bottom =
        thumb.top + thumb_height;
    return thumb;
}

int VolumeFromX(
    HWND hwnd,
    int x) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width =
        std::max(
            1,
            static_cast<int>(
                client.right -
                client.left));
    constexpr int kThumbWidth = 14;
    const int track_left =
        4 + (kThumbWidth / 2);
    const int track_right =
        std::max(
            track_left + 1,
            width - 4 -
                (kThumbWidth / 2));
    const int travel =
        std::max(
            1,
            track_right -
                track_left);

    const int clamped_x =
        std::clamp(
            x,
            track_left,
            track_right);

    return std::clamp(
        ((clamped_x - track_left) *
         100 +
         (travel / 2)) /
            travel,
        0,
        100);
}

void NotifyVolumePosition(
    HWND hwnd,
    int pos) {
    const HWND parent = GetParent(hwnd);
    if (parent != nullptr) {
        SendMessageW(
            parent,
            kVolumePositionMessage,
            static_cast<WPARAM>(pos),
            0);
    }
}

void SetVolumeSliderPosition(
    HWND hwnd,
    VolumeSliderState& state,
    int pos,
    bool notify_parent) {
    const int clamped =
        std::clamp(pos, 0, 100);

    if (clamped == state.pos) {
        return;
    }

    state.pos = clamped;
    InvalidateRect(
        hwnd,
        nullptr,
        FALSE);

    if (notify_parent) {
        NotifyVolumePosition(
            hwnd,
            state.pos);
    }
}

LRESULT CALLBACK VolumeSliderProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    auto* state =
        reinterpret_cast<VolumeSliderState*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        state =
            new (std::nothrow)
                VolumeSliderState{};
        if (state == nullptr) {
            return FALSE;
        }

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                state));
        return TRUE;
    }

    if (state == nullptr) {
        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam);
    }

    switch (message) {
    case kVolumeSetPosMessage:
        SetVolumeSliderPosition(
            hwnd,
            *state,
            static_cast<int>(wparam),
            false);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const HDC dc =
            BeginPaint(hwnd, &paint);

        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(
            dc,
            &client,
            theme::PanelBrush());

        const int center_y =
            (client.top + client.bottom) / 2;
        RECT track{};
        track.left = 6;
        track.right =
            std::max(
                track.left + 1,
                client.right - 6);
        track.top = center_y - 2;
        track.bottom = center_y + 2;
        FillRect(
            dc,
            &track,
            theme::SliderTrackBrush());

        const RECT thumb =
            VolumeSliderThumbRect(
                hwnd,
                state->pos);
        FillRect(
            dc,
            &thumb,
            theme::SliderThumbBrush());
        FrameRect(
            dc,
            &thumb,
            theme::BorderBrush());

        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        SetFocus(hwnd);
        state->dragging = true;
        SetVolumeSliderPosition(
            hwnd,
            *state,
            VolumeFromX(
                hwnd,
                GET_X_LPARAM(lparam)),
            true);
        return 0;

    case WM_MOUSEMOVE:
        if (state->dragging &&
            GetCapture() == hwnd) {
            SetVolumeSliderPosition(
                hwnd,
                *state,
                VolumeFromX(
                    hwnd,
                    GET_X_LPARAM(lparam)),
                true);
        }
        return 0;

    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        state->dragging = false;
        return 0;

    case WM_CAPTURECHANGED:
        state->dragging = false;
        return 0;

    case WM_MOUSEWHEEL: {
        const int delta =
            GET_WHEEL_DELTA_WPARAM(wparam);
        const int notches =
            delta / WHEEL_DELTA;
        const int step =
            notches == 0
                ? (delta > 0 ? 1 : -1)
                : notches;

        SetVolumeSliderPosition(
            hwnd,
            *state,
            state->pos +
                (step *
                 kVolumeWheelStep),
            true);
        return 0;
    }

    case WM_NCDESTROY:
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            0);
        delete state;
        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam);

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam);
}

HWND CreateVolumeSlider(
    HINSTANCE instance,
    HWND parent) {
    WNDCLASSEXW window_class{};
    window_class.cbSize =
        sizeof(window_class);
    window_class.lpfnWndProc =
        VolumeSliderProc;
    window_class.hInstance = instance;
    window_class.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        theme::PanelBrush();
    window_class.lpszClassName =
        kVolumeSliderClassName;

    if (RegisterClassExW(
            &window_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        kVolumeSliderClassName,
        nullptr,
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            WS_CLIPSIBLINGS,
        0,
        0,
        1,
        1,
        parent,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(
                kVolumeSliderId)),
        instance,
        nullptr);
}

struct SettingsScrollbarState {
    int max_offset{0};
    int page{1};
    int pos{0};
    bool dragging{false};
    int drag_anchor_y{0};
    int drag_start_pos{0};
};

RECT SettingsScrollbarThumbRect(
    HWND hwnd,
    const SettingsScrollbarState& state) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width =
        std::max(
            1,
            static_cast<int>(
                client.right -
                client.left));
    const int height =
        std::max(
            1,
            static_cast<int>(
                client.bottom -
                client.top));

    const int track_top = 2;
    const int track_bottom =
        std::max(track_top + 1, height - 2);
    const int track_height =
        std::max(
            1,
            track_bottom - track_top);

    const int total_extent =
        std::max(
            state.page,
            state.page +
                state.max_offset);

    const int proportional =
        total_extent > 0
            ? (track_height * state.page) /
                  total_extent
            : track_height;

    const int thumb_height =
        std::clamp(
            proportional,
            std::min(28, track_height),
            track_height);

    const int travel =
        std::max(
            0,
            track_height -
                thumb_height);

    const int thumb_top =
        track_top +
        (state.max_offset > 0 &&
         travel > 0
             ? (state.pos * travel) /
                   state.max_offset
             : 0);

    RECT thumb{};
    thumb.left = 2;
    thumb.right =
        std::max(3, width - 2);
    thumb.top = thumb_top;
    thumb.bottom =
        thumb_top + thumb_height;
    return thumb;
}

void NotifySettingsScrollbarPosition(
    HWND hwnd,
    int pos) {
    const HWND parent = GetParent(hwnd);
    if (parent != nullptr) {
        SendMessageW(
            parent,
            kSettingsScrollPositionMessage,
            static_cast<WPARAM>(pos),
            0);
    }
}

void SetSettingsScrollbarPosition(
    HWND hwnd,
    SettingsScrollbarState& state,
    int pos,
    bool notify_parent) {
    const int clamped =
        std::clamp(
            pos,
            0,
            std::max(0, state.max_offset));

    if (clamped == state.pos) {
        return;
    }

    state.pos = clamped;
    InvalidateRect(
        hwnd,
        nullptr,
        FALSE);

    if (notify_parent) {
        NotifySettingsScrollbarPosition(
            hwnd,
            state.pos);
    }
}

LRESULT CALLBACK SettingsScrollbarProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    auto* state =
        reinterpret_cast<SettingsScrollbarState*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        state =
            new (std::nothrow)
                SettingsScrollbarState{};
        if (state == nullptr) {
            return FALSE;
        }

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                state));
        return TRUE;
    }

    if (state == nullptr) {
        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam);
    }

    switch (message) {
    case kScrollbarSetRangeMessage:
        state->max_offset =
            std::max(
                0,
                static_cast<int>(wparam));
        state->page =
            std::max(
                1,
                static_cast<int>(lparam));
        state->pos =
            std::clamp(
                state->pos,
                0,
                state->max_offset);
        InvalidateRect(
            hwnd,
            nullptr,
            FALSE);
        return 0;

    case kScrollbarSetPosMessage:
        SetSettingsScrollbarPosition(
            hwnd,
            *state,
            static_cast<int>(wparam),
            false);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        const HDC dc =
            BeginPaint(hwnd, &paint);

        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(
            dc,
            &client,
            theme::PanelBrush());

        RECT track = client;
        const int center =
            (track.left + track.right) / 2;
        track.left = center - 2;
        track.right = center + 2;
        track.top += 2;
        track.bottom -= 2;
        FillRect(
            dc,
            &track,
            theme::SliderTrackBrush());

        const RECT thumb =
            SettingsScrollbarThumbRect(
                hwnd,
                *state);
        FillRect(
            dc,
            &thumb,
            theme::SliderThumbBrush());
        FrameRect(
            dc,
            &thumb,
            theme::BorderBrush());

        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        SetFocus(hwnd);

        const int y =
            GET_Y_LPARAM(lparam);
        const RECT thumb =
            SettingsScrollbarThumbRect(
                hwnd,
                *state);

        POINT point{
            GET_X_LPARAM(lparam),
            y};

        if (PtInRect(&thumb, point) != FALSE) {
            state->dragging = true;
            state->drag_anchor_y = y;
            state->drag_start_pos =
                state->pos;
            return 0;
        }

        RECT client{};
        GetClientRect(hwnd, &client);
        const int height =
            std::max(
                1,
                static_cast<int>(
                    client.bottom -
                    client.top));
        const int thumb_height =
            std::max(
                1,
                static_cast<int>(
                    thumb.bottom -
                    thumb.top));
        const int travel =
            std::max(
                1,
                height - 4 -
                    thumb_height);
        const int desired_top =
            std::clamp(
                y -
                    (thumb_height / 2) - 2,
                0,
                travel);
        const int desired =
            state->max_offset > 0
                ? (desired_top *
                   state->max_offset) /
                      travel
                : 0;

        SetSettingsScrollbarPosition(
            hwnd,
            *state,
            desired,
            true);
        return 0;
    }

    case WM_MOUSEMOVE:
        if (state->dragging &&
            GetCapture() == hwnd) {
            const RECT thumb =
                SettingsScrollbarThumbRect(
                    hwnd,
                    *state);
            RECT client{};
            GetClientRect(hwnd, &client);

            const int height =
                std::max(
                    1,
                    static_cast<int>(
                        client.bottom -
                        client.top));
            const int thumb_height =
                std::max(
                    1,
                    static_cast<int>(
                        thumb.bottom -
                        thumb.top));
            const int travel =
                std::max(
                    1,
                    height - 4 -
                        thumb_height);
            const int delta_y =
                GET_Y_LPARAM(lparam) -
                state->drag_anchor_y;
            const int delta_pos =
                state->max_offset > 0
                    ? (delta_y *
                       state->max_offset) /
                          travel
                    : 0;

            SetSettingsScrollbarPosition(
                hwnd,
                *state,
                state->drag_start_pos +
                    delta_pos,
                true);
        }
        return 0;

    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        state->dragging = false;
        return 0;

    case WM_CAPTURECHANGED:
        state->dragging = false;
        return 0;

    case WM_MOUSEWHEEL: {
        const HWND parent = GetParent(hwnd);
        if (parent != nullptr) {
            SendMessageW(
                parent,
                WM_MOUSEWHEEL,
                wparam,
                lparam);
        }
        return 0;
    }

    case WM_NCDESTROY:
        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            0);
        delete state;
        return DefWindowProcW(
            hwnd,
            message,
            wparam,
            lparam);

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam);
}

HWND CreateSettingsScrollbar(
    HINSTANCE instance,
    HWND parent) {
    WNDCLASSEXW window_class{};
    window_class.cbSize =
        sizeof(window_class);
    window_class.lpfnWndProc =
        SettingsScrollbarProc;
    window_class.hInstance = instance;
    window_class.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        theme::PanelBrush();
    window_class.lpszClassName =
        kSettingsScrollbarClassName;

    if (RegisterClassExW(
            &window_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }

    return CreateWindowExW(
        0,
        kSettingsScrollbarClassName,
        nullptr,
        WS_CHILD |
            WS_VISIBLE,
        0,
        0,
        1,
        1,
        parent,
        reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(
                kSettingsScrollbarId)),
        instance,
        nullptr);
}

LRESULT CALLBACK DropPopupProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam) {
    switch (message) {
    case WM_NCCREATE: {
        const auto* create =
            reinterpret_cast<CREATESTRUCTW*>(
                lparam);
        const auto* init =
            static_cast<const DropPopupInit*>(
                create->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            0,
            reinterpret_cast<LONG_PTR>(
                init != nullptr
                    ? init->notify
                    : nullptr));
        SetWindowLongPtrW(
            hwnd,
            sizeof(LONG_PTR),
            static_cast<LONG_PTR>(
                init != nullptr
                    ? init->kind
                    : 0));
        return TRUE;
    }

    case WM_SIZE: {
        const HWND list =
            GetWindow(hwnd, GW_CHILD);
        if (list != nullptr) {
            RECT client{};
            GetClientRect(hwnd, &client);
            SetWindowPos(
                list,
                HWND_TOP,
                0,
                0,
                std::max(
                    1,
                    static_cast<int>(
                        client.right -
                        client.left)),
                std::max(
                    1,
                    static_cast<int>(
                        client.bottom -
                        client.top)),
                SWP_NOACTIVATE);
        }
        return 0;
    }

    case WM_COMMAND:
        if (HIWORD(wparam) ==
                LBN_SELCHANGE) {
            const HWND list =
                reinterpret_cast<HWND>(
                    lparam);
            const int selection =
                static_cast<int>(
                    SendMessageW(
                        list,
                        LB_GETCURSEL,
                        0,
                        0));

            const HWND notify =
                reinterpret_cast<HWND>(
                    GetWindowLongPtrW(
                        hwnd,
                        0));
            const WPARAM kind =
                static_cast<WPARAM>(
                    GetWindowLongPtrW(
                        hwnd,
                        sizeof(LONG_PTR)));

            if (notify != nullptr &&
                selection >= 0) {
                PostMessageW(
                    notify,
                    kDropSelectionMessage,
                    kind,
                    static_cast<LPARAM>(
                        selection));
            }

            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        break;

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<DRAWITEMSTRUCT*>(
                lparam);

        if (item != nullptr &&
            item->CtlType == ODT_LISTBOX) {
            theme::DrawListItem(*item);
            return TRUE;
        }
        break;
    }

    case WM_MEASUREITEM: {
        auto* item =
            reinterpret_cast<MEASUREITEMSTRUCT*>(
                lparam);

        if (item != nullptr &&
            item->CtlType == ODT_LISTBOX) {
            item->itemHeight =
                kDropListRowHeight;
            return TRUE;
        }
        break;
    }

    case WM_ERASEBKGND: {
        const HDC dc =
            reinterpret_cast<HDC>(
                wparam);
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(
            dc,
            &client,
            theme::SurfaceBrush());
        return 1;
    }

    case WM_CTLCOLORLISTBOX: {
        const HDC dc =
            reinterpret_cast<HDC>(
                wparam);
        SetTextColor(
            dc,
            theme::kTextPrimary);
        SetBkColor(
            dc,
            theme::kSurfaceBackground);
        return reinterpret_cast<LRESULT>(
            theme::SurfaceBrush());
    }

    case WM_ACTIVATE:
        if (LOWORD(wparam) ==
                WA_INACTIVE) {
            ShowWindow(hwnd, SW_HIDE);
        }
        break;

    default:
        break;
    }

    return DefWindowProcW(
        hwnd,
        message,
        wparam,
        lparam);
}

HWND CreateDropPopup(
    HINSTANCE instance,
    HWND owner,
    HWND notify,
    WPARAM kind,
    HWND* list_out) {
    WNDCLASSEXW popup_class{};
    popup_class.cbSize =
        sizeof(popup_class);
    popup_class.lpfnWndProc =
        DropPopupProc;
    popup_class.cbWndExtra =
        static_cast<int>(
            sizeof(LONG_PTR) * 2);
    popup_class.hInstance = instance;
    popup_class.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);
    popup_class.hbrBackground =
        theme::SurfaceBrush();
    popup_class.lpszClassName =
        kDropPopupClassName;

    if (RegisterClassExW(
            &popup_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return nullptr;
    }

    const DropPopupInit init{
        notify,
        kind};

    const HWND popup =
        CreateWindowExW(
            WS_EX_TOOLWINDOW,
            kDropPopupClassName,
            nullptr,
            WS_POPUP |
                WS_BORDER |
                WS_CLIPCHILDREN,
            0,
            0,
            1,
            1,
            owner,
            nullptr,
            instance,
            const_cast<DropPopupInit*>(
                &init));

    if (popup == nullptr) {
        return nullptr;
    }

    const HWND list =
        CreateWindowExW(
            0,
            L"LISTBOX",
            nullptr,
            WS_CHILD |
                WS_VISIBLE |
                LBS_NOTIFY |
                LBS_OWNERDRAWFIXED |
                LBS_HASSTRINGS,
            0,
            0,
            1,
            1,
            popup,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(1)),
            instance,
            nullptr);

    if (list == nullptr) {
        DestroyWindow(popup);
        return nullptr;
    }

    theme::ApplyDarkBorder(popup);
    theme::ApplyDarkBorder(list);

    if (list_out != nullptr) {
        *list_out = list;
    }

    return popup;
}

void ApplyProgramOwnedControlFrame(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }

    std::array<wchar_t, 64> class_name{};
    GetClassNameW(
        hwnd,
        class_name.data(),
        static_cast<int>(
            class_name.size()));

    if (_wcsicmp(
            class_name.data(),
            L"Edit") == 0 ||
        _wcsicmp(
            class_name.data(),
            L"ComboBox") == 0 ||
        _wcsicmp(
            class_name.data(),
            L"ListBox") == 0) {
        theme::ApplyDarkBorder(hwnd);
    }
}
std::wstring Utf8ToWide(
    std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int required =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            nullptr,
            0);

    if (required <= 0) {
        return L"[invalid UTF-8]";
    }

    std::wstring output(
        static_cast<std::size_t>(
            required),
        L'\0');

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            output.data(),
            required) != required) {
        return L"[invalid UTF-8]";
    }

    return output;
}

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

void SetCheckIfDifferent(HWND control,bool checked) {
    const auto value=checked?BST_CHECKED:BST_UNCHECKED;
    if(SendMessageW(control,BM_GETCHECK,0,0)!=value)SendMessageW(control,BM_SETCHECK,value,0);
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
        theme::PanelBrush();
    window_class.lpszClassName =
        kSettingsPanelClassName;

    if (RegisterClassExW(&window_class) == 0 &&
        GetLastError() !=
            ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kSettingsPanelClassName,
        nullptr,
        WS_CHILD |
            WS_VISIBLE |
            WS_BORDER |
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

    theme::ApplyDarkBorder(hwnd_);

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

    RECT current{};GetWindowRect(hwnd_,&current);
    MapWindowPoints(HWND_DESKTOP,GetParent(hwnd_),reinterpret_cast<POINT*>(&current),2);
    if(current.left==x&&current.top==y&&current.right-current.left==std::max(1,width)&&current.bottom-current.top==std::max(1,height))return;
    SetWindowPos(
        hwnd_,
        HWND_TOP,
        x,
        y,
        std::max(1, width),
        std::max(1, height),
        SWP_NOACTIVATE);

    // WM_SIZE lays out controls only when the client geometry changes.
}

void SettingsPanel::Show(bool visible) {
    if (hwnd_ == nullptr) {
        return;
    }

    const bool shown=(GetWindowLongPtrW(hwnd_,GWL_STYLE)&WS_VISIBLE)!=0;
    if(shown==visible)return;

    if (!visible) {
        HideDropLists();
    }

    ShowWindow(
        hwnd_,
        visible ? SW_SHOW : SW_HIDE);

    if (visible) {
        SetWindowPos(
            hwnd_,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE |
                SWP_NOSIZE |
                SWP_NOACTIVATE);

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

    const HWND parent = GetParent(hwnd_);
    if (parent != nullptr) {
        RedrawWindow(
            parent,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
                RDW_ERASE |
                RDW_ALLCHILDREN |
                RDW_UPDATENOW);
    }
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
            WS_CLIPSIBLINGS |
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
    const DWORD button_style =
        style == BS_PUSHBUTTON
            ? BS_OWNERDRAW
            : style;

    return CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            WS_CLIPSIBLINGS |
            button_style,
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
        0,
        L"EDIT",
        nullptr,
        WS_CHILD |
            WS_VISIBLE |
            WS_TABSTOP |
            WS_BORDER |
            WS_CLIPSIBLINGS |
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

void SettingsPanel::InitializeVideoControls(HINSTANCE instance, HWND root_owner) {
    video_heading_ = CreateLabel(instance,L"VIDEO / H.264");
    encoder_label_ = CreateLabel(instance,L"Encoder (send)");
    decoder_label_ = CreateLabel(instance,L"Decoder (receive)");
    encoder_button_ = CreateButton(instance,kEncoderId,L"Choose encoder");
    decoder_button_ = CreateButton(instance,kDecoderId,L"Choose decoder");
    bitrate_label_ = CreateLabel(instance,L"Bitrate (Mbps)");
    keyframe_label_ = CreateLabel(instance,L"Keyframe (frames)");
    bitrate_edit_ = CreateEdit(instance,kBitrateId,ES_AUTOHSCROLL | ES_NUMBER);
    keyframe_edit_ = CreateEdit(instance,kKeyframeId,ES_AUTOHSCROLL | ES_NUMBER);
    SendMessageW(bitrate_edit_,EM_SETLIMITTEXT,3,0);
    SendMessageW(keyframe_edit_,EM_SETLIMITTEXT,3,0);
    low_latency_checkbox_ = CreateButton(instance,kLowLatencyId,L"Low latency encoding",BS_AUTOCHECKBOX);
    video_apply_button_ = CreateButton(instance,kApplyVideoId,L"Apply Video");
    video_status_ = CreateLabel(instance,L"Video controls apply to the local decoded preview.");
    encoder_popup_ = CreateDropPopup(instance,root_owner,hwnd_,kDropKindEncoder,&encoder_list_);
    decoder_popup_ = CreateDropPopup(instance,root_owner,hwnd_,kDropKindDecoder,&decoder_list_);
    ShowWindow(encoder_popup_,SW_HIDE); ShowWindow(decoder_popup_,SW_HIDE);
    encoder_choices_.push_back({media::VideoBackendPolicy::PreferHardware,{},L"Prefer GPU (allow CPU fallback)"});
    decoder_choices_.push_back({media::VideoBackendPolicy::PreferHardware,{},L"Prefer GPU (allow CPU fallback)"});
    if (SUCCEEDED(MFStartup(MF_VERSION,MFSTARTUP_FULL))) {
        for (const auto direction : {media::VideoCodecDirection::Encode,media::VideoCodecDirection::Decode}) {
            auto& choices = direction == media::VideoCodecDirection::Encode ? encoder_choices_ : decoder_choices_;
            for (const auto& descriptor : media::EnumerateMftVideoCodecDescriptors(media::VideoCodecKind::H264,direction)) {
                const auto id = media::MftVideoCodecIdentity(descriptor);
                const auto name = Utf8ToWide(descriptor.candidate.name);
                const bool hardware = descriptor.candidate.classification == media::VideoBackendClassification::Hardware;
                if (hardware || (direction == media::VideoCodecDirection::Decode && descriptor.candidate.d3d11_aware))
                    choices.push_back({media::VideoBackendPolicy::RequireHardware,id,L"[GPU] " + name});
                if (!hardware && descriptor.candidate.classification == media::VideoBackendClassification::Software)
                    choices.push_back({media::VideoBackendPolicy::ForceSoftware,id,L"[CPU] " + name});
            }
        }
        MFShutdown();
    }
}

void SettingsPanel::UpdateVideoChoices() {
    const auto update = [](auto& choices, HWND list, HWND button, media::VideoBackendPolicy policy, const std::string& id) {
        auto found = std::find_if(choices.begin(),choices.end(),[&](const auto& choice) {
            return choice.policy == policy && choice.id == id;
        });
        if (found == choices.end()) {
            const auto mode = policy == media::VideoBackendPolicy::ForceSoftware ? L"CPU" : L"GPU";
            choices.push_back({policy,id,std::wstring{mode} + (id.empty() ? L" only" : L" - saved codec unavailable")});
        }
        SendMessageW(list,LB_RESETCONTENT,0,0);
        int selection = 0;
        for (std::size_t i = 0; i < choices.size(); ++i) {
            SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(choices[i].text.c_str()));
            if (choices[i].policy == policy && choices[i].id == id) selection = static_cast<int>(i);
        }
        SendMessageW(list,LB_SETCURSEL,static_cast<WPARAM>(selection),0);
        const auto label = choices[static_cast<std::size_t>(selection)].text + L"  \u25BE";
        SetWindowTextW(button,label.c_str());
    };
    update(encoder_choices_,encoder_list_,encoder_button_,video_draft_.encoder_mode,video_draft_.encoder_id);
    update(decoder_choices_,decoder_list_,decoder_button_,video_draft_.decoder_mode,video_draft_.decoder_id);
}

void SettingsPanel::ApplyVideoDraft() {
    VideoPreferences requested = video_draft_;
    const auto read = [this](HWND edit, std::uint32_t& value) {
        const auto text = ReadEditUtf8(edit);
        const auto result = std::from_chars(text.data(),text.data() + text.size(),value);
        return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size();
    };
    const bool parsed = read(bitrate_edit_,requested.bitrate_mbps) &&
        read(keyframe_edit_,requested.keyframe_interval);
    requested.low_latency = SendMessageW(low_latency_checkbox_,BM_GETCHECK,0,0) == BST_CHECKED;
    if (!parsed || !IsValidVideoPreferences(requested)) {
        video_validation_error_ = true;
        SetWindowTextW(video_status_,L"Not applied. Bitrate must be 1-200 Mbps; keyframe interval must be 1-600 frames.");
        return;
    }
    video_validation_error_ = false;
    video_draft_ = requested;
    video_apply_pending_ = true;
    UiCommand command{UiCommandType::ApplyVideoSettings};
    command.video = std::move(requested);
    Emit(std::move(command));
}

void SettingsPanel::SetVideoCodecStatus(std::wstring_view text) {
    if (!video_validation_error_) SetTextIfDifferent(video_status_,std::wstring{text});
}

void SettingsPanel::InitializeControls(
    HINSTANCE instance) {
    title_label_ =
        CreateLabel(
            instance,
            L"Settings / Control");

    share_heading_ =
        CreateLabel(instance, L"SHARE");
    source_label_ =
        CreateLabel(
            instance,
            L"Source");
    show_cursor_checkbox_ =
        CreateButton(
            instance,
            kShowCursorId,
            L"Show Cursor",
            BS_AUTOCHECKBOX);
    source_button_ =
        CreateButton(
            instance,
            kSourceId,
            L"Select Source  \u25BE");
    source_preview_button_ =
        CreateButton(
            instance,
            kPreviewSourceId,
            L"Preview");
    source_refresh_button_ =
        CreateButton(
            instance,
            kRefreshSourcesId,
            L"Refresh");
    source_status_ =
        CreateLabel(
            instance,
            L"No source selected");

    share_port_label_ =
        CreateLabel(instance, L"Share Port");
    share_port_edit_ =
        CreateEdit(
            instance,
            kSharePortEditId,
            ES_AUTOHSCROLL |
                ES_NUMBER);
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
    connection_mode_button_ =
        CreateButton(
            instance,
            kConnectionModeId,
            L"Direct IP / DDNS  \u25BE");

    const HWND root_owner =
        GetAncestor(hwnd_, GA_ROOT);

    source_popup_ =
        CreateDropPopup(
            instance,
            root_owner,
            hwnd_,
            kDropKindSource,
            &source_list_);

    ShowWindow(
        source_popup_,
        SW_HIDE);

    InitializeVideoControls(instance,root_owner);

    connection_mode_popup_ =
        CreateDropPopup(
            instance,
            root_owner,
            hwnd_,
            kDropKindConnection,
            &connection_mode_list_);

    SendMessageW(
        connection_mode_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(L"LAN"));
    SendMessageW(
        connection_mode_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Direct IP / DDNS"));
    SendMessageW(
        connection_mode_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Tailscale"));

    ShowWindow(
        connection_mode_popup_,
        SW_HIDE);

    host_label_ =
        CreateLabel(instance, L"Host");
    host_edit_ =
        CreateEdit(
            instance,
            kHostEditId);
    remote_port_label_ =
        CreateLabel(instance, L"Remote Port");
    remote_port_edit_ =
        CreateEdit(
            instance,
            kRemotePortEditId,
            ES_AUTOHSCROLL |
                ES_NUMBER);
    connect_button_ =
        CreateButton(
            instance,
            kConnectButtonId,
            L"Connect");
    disconnect_button_=CreateButton(instance,kDisconnectPeerId,L"Disconnect");
    connection_status_ =
        CreateLabel(
            instance,
            L"Disconnected");

    export_identity_button_=CreateButton(instance,kExportIdentityId,L"Export Identity...");
    pair_friend_button_=CreateButton(instance,kPairFriendId,L"Pair with Friend...");
    own_identity_label_=CreateLabel(instance,L"Your SHA-256: initializing...");
    peer_identity_label_=CreateLabel(instance,L"Friend SHA-256: not paired");

    audio_heading_ =
        CreateLabel(
            instance,
            L"AUDIO");
    audio_source_label_ =
        CreateLabel(
            instance,
            L"Audio Source");
    audio_source_button_ =
        CreateButton(
            instance,
            kAudioSourceId,
            L"No Audio  \u25BE");

    audio_source_popup_ =
        CreateDropPopup(
            instance,
            root_owner,
            hwnd_,
            kDropKindAudio,
            &audio_source_list_);

    SendMessageW(
        audio_source_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"No Audio"));
    SendMessageW(
        audio_source_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Desktop / System Audio"));
    SendMessageW(
        audio_source_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Device Source"));
    SendMessageW(
        audio_source_list_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(
            L"Application Audio"));

    ShowWindow(
        audio_source_popup_,
        SW_HIDE);

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
        CreateVolumeSlider(
            instance,
            hwnd_);
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
            0,
            L"EDIT",
            L"Collecting diagnostics...",
            WS_CHILD |
                WS_BORDER |
                WS_VSCROLL |
                WS_CLIPSIBLINGS |
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

    settings_scrollbar_ =
        CreateSettingsScrollbar(
            instance,
            hwnd_);

    UpdateDiagnosticsVisibility(false);
}

void SettingsPanel::ApplyDefaultFont() {
    const HFONT font =
        static_cast<HFONT>(
            GetStockObject(
                DEFAULT_GUI_FONT));

    const std::vector<HWND> controls{
        title_label_,
        share_heading_,
        source_label_,
        show_cursor_checkbox_,
        source_button_,
        source_preview_button_,
        source_refresh_button_,
        source_status_,
        share_port_label_,
        share_port_edit_,
        share_button_,
        share_status_,
        video_heading_, encoder_label_, encoder_button_, encoder_list_,
        decoder_label_, decoder_button_, decoder_list_,
        bitrate_label_, bitrate_edit_, keyframe_label_, keyframe_edit_,
        low_latency_checkbox_, video_apply_button_, video_status_,
        connection_heading_,
        connection_mode_label_,
        connection_mode_button_,
        connection_mode_list_,
        host_label_,
        host_edit_,
        remote_port_label_,
        remote_port_edit_,
        connect_button_,disconnect_button_,
        connection_status_,
        export_identity_button_,pair_friend_button_,own_identity_label_,peer_identity_label_,
        audio_heading_,
        audio_source_label_,
        audio_source_button_,
        audio_source_list_,
        audio_status_,
        mute_button_,
        volume_slider_,
        volume_label_,
        settings_scrollbar_,
        viewer_heading_,
        topmost_checkbox_,
        borderless_checkbox_,
        fullscreen_button_,
        advanced_heading_,
        diagnostics_toggle_,
        diagnostics_edit_,
        refresh_button_,
        save_button_
    };

    for (HWND control : controls) {
        if (control == nullptr) {
            continue;
        }

        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(
                font),
            TRUE);

        ApplyProgramOwnedControlFrame(control);
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

    const int scrollbar_x =
        width -
        kPadding -
        kScrollbarWidth;

    const int field_width =
        std::max(
            240,
            scrollbar_x -
                kPadding -
                kGap);

    const auto place =
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

    const auto move = [&](HWND control,int x,int y,int w,int h) {
        place(control,x,y + (y >= 316 ? kVideoSectionHeight : 0) + (y >= 560 ? kPairSectionHeight : 0),w,h);
    };
    place(video_heading_,kPadding,316,field_width,20);
    place(encoder_label_,kPadding,342,field_width,18);
    place(encoder_button_,kPadding,362,field_width,kFieldHeight);
    place(decoder_label_,kPadding,402,field_width,18);
    place(decoder_button_,kPadding,422,field_width,kFieldHeight);
    const int half = (field_width - kGap) / 2;
    place(bitrate_label_,kPadding,462,half,18);
    place(keyframe_label_,kPadding + half + kGap,462,half,18);
    place(bitrate_edit_,kPadding,482,half,kFieldHeight);
    place(keyframe_edit_,kPadding + half + kGap,482,half,kFieldHeight);
    place(low_latency_checkbox_,kPadding,520,half,26);
    place(video_apply_button_,kPadding,554,field_width,kButtonHeight);
    place(video_status_,kPadding,596,field_width,92);

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
        source_label_,
        kPadding,
        78,
        field_width,
        18);

    move(
        show_cursor_checkbox_,
        kPadding,
        132,
        field_width,
        26);


    move(
        source_button_,
        kPadding,
        98,
        field_width,
        kFieldHeight);

    const int source_preview_width = 96;
    const int source_refresh_width = 76;
    const int source_status_width =
        std::max(
            100,
            field_width -
                source_preview_width -
                source_refresh_width -
                (2 * kGap));

    move(
        source_status_,
        kPadding,
        162,
        source_status_width,
        32);
    move(
        source_preview_button_,
        kPadding +
            source_status_width +
            kGap,
        162,
        source_preview_width,
        kButtonHeight);
    move(
        source_refresh_button_,
        kPadding +
            field_width -
            source_refresh_width,
        162,
        source_refresh_width,
        kButtonHeight);

    move(
        share_port_label_,
        kPadding,
        204,
        120,
        18);

    const int share_port_width = 120;
    move(
        share_port_edit_,
        kPadding,
        224,
        share_port_width,
        kFieldHeight);
    move(
        share_button_,
        kPadding +
            share_port_width +
            kGap,
        224,
        std::max(
            120,
            field_width -
                share_port_width -
                kGap),
        kButtonHeight);
    move(
        share_status_,
        kPadding,
        262,
        field_width,
        40);

    move(
        connection_heading_,
        kPadding,
        316,
        field_width,
        20);
    move(
        connection_mode_label_,
        kPadding,
        342,
        field_width,
        18);
    move(
        connection_mode_button_,
        kPadding,
        362,
        field_width,
        kFieldHeight);

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
        402,
        host_width,
        18);
    move(
        remote_port_label_,
        kPadding +
            host_width +
            kGap,
        402,
        port_width,
        18);
    move(
        host_edit_,
        kPadding,
        422,
        host_width,
        kFieldHeight);
    move(
        remote_port_edit_,
        kPadding +
            host_width +
            kGap,
        422,
        port_width,
        kFieldHeight);
    move(
        connect_button_,
        kPadding,
        460,
        half,
        kButtonHeight);
    move(disconnect_button_,kPadding+half+kGap,460,half,kButtonHeight);
    move(
        connection_status_,
        kPadding,
        500,
        field_width,
        48);

    place(export_identity_button_,kPadding,552+kVideoSectionHeight,half,kButtonHeight);
    place(pair_friend_button_,kPadding+half+kGap,552+kVideoSectionHeight,half,kButtonHeight);
    place(own_identity_label_,kPadding,592+kVideoSectionHeight,field_width,72);
    place(peer_identity_label_,kPadding,668+kVideoSectionHeight,field_width,72);
    move(
        audio_heading_,
        kPadding,
        560,
        field_width,
        20);
    move(
        audio_source_label_,
        kPadding,
        586,
        field_width,
        18);
    move(
        audio_source_button_,
        kPadding,
        606,
        field_width,
        kFieldHeight);
    move(
        audio_status_,
        kPadding,
        646,
        field_width,
        56);

    const int mute_width = 140;
    move(
        mute_button_,
        kPadding,
        712,
        mute_width,
        kButtonHeight);
    move(
        volume_slider_,
        kPadding +
            mute_width +
            kGap,
        712,
        std::max(
            100,
            field_width -
                mute_width -
                kGap),
        kButtonHeight);
    move(
        volume_label_,
        kPadding,
        752,
        field_width,
        20);

    move(
        viewer_heading_,
        kPadding,
        792,
        field_width,
        20);
    move(
        topmost_checkbox_,
        kPadding,
        818,
        field_width,
        26);
    move(
        borderless_checkbox_,
        kPadding,
        848,
        field_width,
        26);
    move(
        fullscreen_button_,
        kPadding,
        882,
        field_width,
        kButtonHeight);

    move(
        advanced_heading_,
        kPadding,
        936,
        field_width,
        20);
    move(
        diagnostics_toggle_,
        kPadding,
        962,
        field_width,
        kButtonHeight);

    if (diagnostics_visible_) {
        move(
            diagnostics_edit_,
            kPadding,
            1004,
            field_width,
            280);

        const int button_width =
            std::max(
                100,
                (field_width - kGap) / 2);

        move(
            refresh_button_,
            kPadding,
            1294,
            button_width,
            kButtonHeight);
        move(
            save_button_,
            kPadding +
                button_width +
                kGap,
            1294,
            button_width,
            kButtonHeight);

        content_height_ = 1342 + kVideoSectionHeight + kPairSectionHeight;
    } else {
        content_height_ = 1012 + kVideoSectionHeight + kPairSectionHeight;
    }

    SetWindowPos(
        settings_scrollbar_,
        HWND_TOP,
        scrollbar_x,
        kPadding,
        kScrollbarWidth,
        std::max(
            1,
            static_cast<int>(
                client.bottom -
                client.top) -
                (2 * kPadding)),
        SWP_NOACTIVATE);

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
    if (hwnd_ == nullptr ||
        settings_scrollbar_ == nullptr) {
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
    scroll_target_offset_ =
        std::clamp(
            scroll_target_offset_,
            0,
            max_offset);

    SendMessageW(
        settings_scrollbar_,
        kScrollbarSetRangeMessage,
        static_cast<WPARAM>(
            max_offset),
        static_cast<LPARAM>(page));

    SendMessageW(
        settings_scrollbar_,
        kScrollbarSetPosMessage,
        static_cast<WPARAM>(
            scroll_offset_),
        0);

    ShowWindow(
        settings_scrollbar_,
        max_offset > 0
            ? SW_SHOW
            : SW_HIDE);
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

    HideDropLists();
    KillTimer(
        hwnd_,
        kSmoothScrollTimerId);
    scroll_offset_ = clamped;
    scroll_target_offset_ = clamped;
    LayoutControls();
}

void SettingsPanel::ScrollBySmoothDelta(
    int delta) {
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

    scroll_target_offset_ =
        std::clamp(
            scroll_target_offset_ + delta,
            0,
            max_offset);

    if (scroll_target_offset_ ==
        scroll_offset_) {
        return;
    }

    HideDropLists();

    SetTimer(
        hwnd_,
        kSmoothScrollTimerId,
        kSmoothScrollIntervalMs,
        nullptr);
}

void SettingsPanel::AnimateScrollStep() {
    const int distance =
        scroll_target_offset_ -
        scroll_offset_;

    if (distance == 0) {
        KillTimer(
            hwnd_,
            kSmoothScrollTimerId);
        return;
    }

    const int magnitude =
        std::abs(distance);
    const int step =
        std::max(
            1,
            std::min(
                28,
                (magnitude + 3) / 4));

    const int next =
        scroll_offset_ +
        (distance > 0
            ? step
            : -step);

    scroll_offset_ =
        distance > 0
            ? std::min(
                  next,
                  scroll_target_offset_)
            : std::max(
                  next,
                  scroll_target_offset_);

    LayoutControls();

    if (scroll_offset_ ==
        scroll_target_offset_) {
        KillTimer(
            hwnd_,
            kSmoothScrollTimerId);
    }
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

std::uint32_t SettingsPanel::ReadPort(HWND edit) const {
    if (edit == nullptr) {
        return 0;
    }

    std::array<wchar_t, 16> buffer{};
    GetWindowTextW(
        edit,
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

void SettingsPanel::HideDropLists() {
    if (encoder_popup_) ShowWindow(encoder_popup_,SW_HIDE);
    if (decoder_popup_) ShowWindow(decoder_popup_,SW_HIDE);
    if (source_popup_ != nullptr) {
        ShowWindow(
            source_popup_,
            SW_HIDE);
    }

    if (connection_mode_popup_ != nullptr) {
        ShowWindow(
            connection_mode_popup_,
            SW_HIDE);
    }

    if (audio_source_popup_ != nullptr) {
        ShowWindow(
            audio_source_popup_,
            SW_HIDE);
    }
}

void SettingsPanel::ToggleDropList(HWND list) {
    if (list == nullptr) {
        return;
    }

    HWND button = nullptr;
    HWND popup = nullptr;

    if (list == source_list_) {
        button = source_button_;
        popup = source_popup_;
    } else if (list == connection_mode_list_) {
        button = connection_mode_button_;
        popup = connection_mode_popup_;
    } else if (list == audio_source_list_) {
        button = audio_source_button_;
        popup = audio_source_popup_;
    } else if (list == encoder_list_) {
        button = encoder_button_; popup = encoder_popup_;
    } else if (list == decoder_list_) {
        button = decoder_button_; popup = decoder_popup_;
    }

    if (button == nullptr ||
        popup == nullptr) {
        return;
    }

    const bool was_visible =
        IsWindowVisible(popup) != FALSE;

    HideDropLists();

    if (was_visible) {
        return;
    }

    RECT button_rect{};
    if (GetWindowRect(
            button,
            &button_rect) == FALSE) {
        return;
    }

    const int item_count =
        static_cast<int>(
            SendMessageW(
                list,
                LB_GETCOUNT,
                0,
                0));

    const int popup_width =
        std::max(
            1,
            static_cast<int>(
                button_rect.right -
                button_rect.left));

    const int popup_height =
        std::max(
            kDropListRowHeight + 4,
            (std::max(1, item_count) *
             kDropListRowHeight) +
                4);

    int popup_x = button_rect.left;
    int popup_y = button_rect.bottom + 2;

    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    const HMONITOR monitor_handle =
        MonitorFromWindow(
            button,
            MONITOR_DEFAULTTONEAREST);

    if (GetMonitorInfoW(
            monitor_handle,
            &monitor) != FALSE) {
        if (popup_y + popup_height >
            monitor.rcWork.bottom) {
            popup_y =
                button_rect.top -
                popup_height -
                2;
        }

        const int work_left =
            static_cast<int>(
                monitor.rcWork.left);
        const int work_right =
            static_cast<int>(
                monitor.rcWork.right);

        popup_x = std::clamp(
            popup_x,
            work_left,
            std::max(
                work_left,
                work_right -
                    popup_width));
    }

    SetWindowPos(
        popup,
        HWND_TOP,
        popup_x,
        popup_y,
        popup_width,
        popup_height,
        SWP_SHOWWINDOW);

    SetFocus(list);

    RedrawWindow(
        popup,
        nullptr,
        nullptr,
        RDW_INVALIDATE |
            RDW_ERASE |
            RDW_FRAME |
            RDW_ALLCHILDREN |
            RDW_UPDATENOW);
}

void SettingsPanel::UpdateDropDownText(const UiState& state) {
    if(!dropdown_initialized_||applied_connection_mode_!=state.preferences.connection_mode) {
        const wchar_t* connection=L"Direct IP / DDNS";
        if(state.preferences.connection_mode==ConnectionMode::Lan)connection=L"LAN";
        else if(state.preferences.connection_mode==ConnectionMode::Tailscale)connection=L"Tailscale";
        SetTextIfDifferent(connection_mode_button_,std::wstring{connection}+L"  \u25BE");
        SendMessageW(connection_mode_list_,LB_SETCURSEL,static_cast<WPARAM>(state.preferences.connection_mode),0);
        applied_connection_mode_=state.preferences.connection_mode;
    }
    if(!dropdown_initialized_||applied_audio_source_!=state.preferences.audio_source) {
        const wchar_t* audio=L"No Audio";
        if(state.preferences.audio_source==AudioSourceKind::System)audio=L"Desktop / System Audio";
        else if(state.preferences.audio_source==AudioSourceKind::Device)audio=L"Device Source";
        else if(state.preferences.audio_source==AudioSourceKind::Application)audio=L"Application Audio";
        SetTextIfDifferent(audio_source_button_,std::wstring{audio}+L"  \u25BE");
        SendMessageW(audio_source_list_,LB_SETCURSEL,static_cast<WPARAM>(state.preferences.audio_source),0);
        applied_audio_source_=state.preferences.audio_source;
    }
    dropdown_initialized_=true;
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

    UpdateDropDownText(state);
    if (video_apply_pending_ && video_draft_ == state.preferences.video) {
        video_dirty_ = false; video_apply_pending_ = false;
    }
    if (!video_initialized_ || !video_dirty_) {
        const bool changed=!video_initialized_||video_draft_!=state.preferences.video;
        video_draft_ = state.preferences.video;
        video_initialized_ = true;
        if(changed)UpdateVideoChoices();
        SetTextIfDifferent(bitrate_edit_,std::to_wstring(video_draft_.bitrate_mbps));
        SetTextIfDifferent(keyframe_edit_,std::to_wstring(video_draft_.keyframe_interval));
        SetCheckIfDifferent(low_latency_checkbox_,video_draft_.low_latency);
    }

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
        share_port_edit_,
        PortText(
            state.preferences.share_port));
    SetTextIfDifferent(
        remote_port_edit_,
        PortText(
            state.preferences.remote_port));

    const bool sharing=state.share_state!=ShareUiState::NotSharing;
    SetTextIfDifferent(share_button_,sharing?L"Stop Share":L"Start Share");
    SetTextIfDifferent(share_status_,Utf8ToWide(state.share_status.empty()?"Not sharing":state.share_status));
    SetTextIfDifferent(connect_button_,state.connection_state==ConnectionUiState::Connected?L"Connected":
        (state.connection_state==ConnectionUiState::ConnectRequested?L"Connecting...":L"Connect"));
    EnableWindow(connect_button_,state.connection_state==ConnectionUiState::Connected||state.connection_state==ConnectionUiState::ConnectRequested?FALSE:TRUE);
    EnableWindow(disconnect_button_,state.transport_active?TRUE:FALSE);
    SetTextIfDifferent(connection_status_,Utf8ToWide(state.peer_status.empty()?"Pair with your friend, then Connect":state.peer_status));
    const auto fingerprint=[](const char* label,const std::string& value) {
        std::string text(label);
        if(value.empty())return Utf8ToWide(text+"not paired");
        for(std::size_t i=0;i<value.size();++i){if(i&&i%8==0)text+=' ';text+=value[i];}
        return Utf8ToWide(text);
    };
    SetTextIfDifferent(own_identity_label_,fingerprint("Your SHA-256:\n",state.own_fingerprint));
    SetTextIfDifferent(peer_identity_label_,fingerprint("Friend SHA-256:\n",state.peer_fingerprint));
    EnableWindow(share_port_edit_,state.transport_active?FALSE:TRUE);
    EnableWindow(host_edit_,state.connection_state==ConnectionUiState::Connected?FALSE:TRUE);
    EnableWindow(remote_port_edit_,state.connection_state==ConnectionUiState::Connected?FALSE:TRUE);

    switch (state.preferences.audio_source) {
    case AudioSourceKind::None:
        SetTextIfDifferent(
            audio_status_,
            state.application_audio_supported
                ? L"Audio disabled. Application Audio capability is available on this Windows build."
                : L"Audio disabled. Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;

    case AudioSourceKind::System:
        SetTextIfDifferent(
            audio_status_,
            state.application_audio_supported
                ? L"Desktop / System Audio selected - capture starts in FPS-006A."
                : L"Desktop / System Audio selected - capture starts in FPS-006A. Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;

    case AudioSourceKind::Device:
        SetTextIfDifferent(
            audio_status_,
            state.application_audio_supported
                ? L"Device Source selected - live device enumeration/capture starts in FPS-006A."
                : L"Device Source selected - live device enumeration/capture starts in FPS-006A. Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;

    case AudioSourceKind::Application:
        SetTextIfDifferent(
            audio_status_,
            state.application_audio_supported
                ? L"Application Audio selected - capture starts in FPS-006A."
                : L"Application Audio is unsupported on this Windows build (requires build 20348+).");
        break;
    }

    SetTextIfDifferent(
        mute_button_,
        state.preferences.receiver_muted
            ? L"Unmute Remote Audio"
            : L"Mute Remote Audio");

    SendMessageW(
        volume_slider_,
        kVolumeSetPosMessage,
        static_cast<WPARAM>(
            state.preferences.receiver_volume),
        0);

    const std::wstring volume_text =
        L"Volume: " +
        std::to_wstring(
            state.preferences.receiver_volume) +
        L"%";

    SetTextIfDifferent(
        volume_label_,
        volume_text);

    SetCheckIfDifferent(show_cursor_checkbox_,state.preferences.show_cursor);

    SetCheckIfDifferent(topmost_checkbox_,state.preferences.always_on_top);

    SetCheckIfDifferent(borderless_checkbox_,state.preferences.borderless);

    SetTextIfDifferent(
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
        SetTextIfDifferent(
            diagnostics_toggle_,
            diagnostics_visible_
                ? L"Hide Diagnostics"
                : L"Show Diagnostics");
        // Status updates do not change control geometry.
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

void SettingsPanel::SetCaptureSources(
    const std::vector<capture::CaptureSourceDescriptor>& sources,
    std::string_view selected_source_id,
    bool preview_active,
    bool show_cursor,
    std::string_view status_text) {
    if (source_list_ == nullptr ||
        source_button_ == nullptr ||
        source_preview_button_ == nullptr ||
        source_status_ == nullptr) {
        return;
    }

    const bool refresh=sources!=applied_sources_||selected_source_id!=applied_source_id_;
    if(refresh) {
        HideDropLists();
        SendMessageW(source_list_,LB_RESETCONTENT,0,0);
        applied_sources_=sources;applied_source_id_=selected_source_id;
    }

    capture_source_ids_.clear();
    capture_source_ids_.reserve(
        sources.size());

    int selected_index = -1;
    bool selected_available = false;
    bool selected_cursor_supported = true;
    std::wstring selected_label;

    for (std::size_t index = 0;
         index < sources.size();
         ++index) {
        const auto& source =
            sources[index];

        std::wstring label =
            Utf8ToWide(
                source.display_name);

        if (source.availability ==
            capture::CaptureAvailability::Unavailable) {
            label += L" [Unavailable]";
        }

        if(refresh)SendMessageW(
            source_list_,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(
                label.c_str()));

        capture_source_ids_.push_back(
            source.id);

        if (source.id ==
            selected_source_id) {
            selected_index =
                static_cast<int>(
                    index);
            selected_label = label;
            selected_available =
                source.availability ==
                capture::CaptureAvailability::Available;
            selected_cursor_supported =
                capture::CaptureSourceSupportsCursor(
                    source.kind);
        }
    }

    if (selected_index >= 0) {
        if(refresh)SendMessageW(
            source_list_,
            LB_SETCURSEL,
            static_cast<WPARAM>(
                selected_index),
            0);

        selected_label += L"  \u25BE";
        SetWindowTextW(
            source_button_,
            selected_label.c_str());
    } else {
        SetWindowTextW(
            source_button_,
            sources.empty()
                ? L"No Sources  \u25BE"
                : L"Select Source  \u25BE");
    }

    SetWindowTextW(
        source_preview_button_,
        preview_active
            ? L"Stop Preview"
            : L"Preview");

    EnableWindow(
        source_preview_button_,
        selected_index >= 0 &&
                selected_available
            ? TRUE
            : FALSE);

    const bool cursor_applicable =
        selected_index < 0 ||
        selected_cursor_supported;

    EnableWindow(
        show_cursor_checkbox_,
        cursor_applicable
            ? TRUE
            : FALSE);

    SetWindowTextW(
        show_cursor_checkbox_,
        cursor_applicable
            ? L"Show Cursor"
            : L"Show Cursor (N/A)");

    SendMessageW(
        show_cursor_checkbox_,
        BM_SETCHECK,
        show_cursor
            ? BST_CHECKED
            : BST_UNCHECKED,
        0);

    const std::wstring status =
        status_text.empty()
            ? L"No source selected"
            : Utf8ToWide(status_text);

    SetWindowTextW(
        source_status_,
        status.c_str());
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

    case WM_ERASEBKGND: {
        const HDC dc =
            reinterpret_cast<HDC>(wparam);

        RECT rect{};
        GetClientRect(hwnd_, &rect);
        FillRect(
            dc,
            &rect,
            theme::PanelBrush());
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        const HDC dc =
            reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, theme::kTextPrimary);
        SetBkColor(dc, theme::kPanelBackground);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(
            theme::PanelBrush());
    }

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        const HDC dc =
            reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, theme::kTextPrimary);
        SetBkColor(dc, theme::kSurfaceBackground);
        return reinterpret_cast<LRESULT>(
            theme::SurfaceBrush());
    }

    case WM_CTLCOLORBTN: {
        const HDC dc =
            reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, theme::kTextPrimary);
        SetBkColor(dc, theme::kSurfaceBackground);
        return reinterpret_cast<LRESULT>(
            theme::SurfaceBrush());
    }

    case WM_DRAWITEM: {
        const auto* item =
            reinterpret_cast<DRAWITEMSTRUCT*>(
                lparam);

        if (item != nullptr &&
            item->CtlType == ODT_BUTTON) {
            theme::DrawButton(*item);
            return TRUE;
        }

        if (item != nullptr &&
            item->CtlType == ODT_COMBOBOX) {
            theme::DrawComboItem(*item);
            return TRUE;
        }

        if (item != nullptr &&
            item->CtlType == ODT_LISTBOX) {
            theme::DrawListItem(*item);
            return TRUE;
        }
        break;
    }

    case WM_MEASUREITEM: {
        auto* item =
            reinterpret_cast<MEASUREITEMSTRUCT*>(
                lparam);

        if (item != nullptr &&
            (item->CtlType == ODT_COMBOBOX ||
             item->CtlType == ODT_LISTBOX)) {
            item->itemHeight = kDropListRowHeight;
            return TRUE;
        }

        break;
    }

    case WM_TIMER:
        if (wparam ==
                kSmoothScrollTimerId) {
            AnimateScrollStep();
            return 0;
        }
        break;

    case WM_MOUSEWHEEL: {
        const int delta =
            GET_WHEEL_DELTA_WPARAM(wparam);
        const int notches =
            delta / WHEEL_DELTA;
        const int wheel_steps =
            notches == 0
                ? (delta > 0 ? 1 : -1)
                : notches;

        ScrollBySmoothDelta(
            -(wheel_steps *
              kSmoothWheelPixels));
        return 0;
    }

    case kSettingsScrollPositionMessage:
        SetScrollOffset(
            static_cast<int>(wparam));
        return 0;

    case kVolumePositionMessage:
        Emit(UiCommand{
            UiCommandType::SetReceiverVolume,
            std::clamp(
                static_cast<int>(wparam),
                0,
                100),
            {}
        });
        return 0;

    case kDropSelectionMessage: {
        const int selection =
            static_cast<int>(lparam);

        if (selection < 0) {
            return 0;
        }

        if (wparam == kDropKindEncoder || wparam == kDropKindDecoder) {
            const auto& choices = wparam == kDropKindEncoder ? encoder_choices_ : decoder_choices_;
            if (selection >= static_cast<int>(choices.size())) return 0;
            const auto& choice = choices[static_cast<std::size_t>(selection)];
            if (wparam == kDropKindEncoder) {
                video_draft_.encoder_mode = choice.policy; video_draft_.encoder_id = choice.id;
            } else {
                video_draft_.decoder_mode = choice.policy; video_draft_.decoder_id = choice.id;
            }
            video_dirty_ = true;
            UpdateVideoChoices();
            return 0;
        }

        if (wparam ==
            kDropKindSource) {
            if (selection >=
                static_cast<int>(
                    capture_source_ids_.size())) {
                return 0;
            }

            Emit(UiCommand{
                UiCommandType::SelectCaptureSource,
                selection,
                capture_source_ids_[
                    static_cast<std::size_t>(
                        selection)]
            });
            return 0;
        }

        if (wparam ==
            kDropKindConnection) {
            if (selection > 2) {
                return 0;
            }

            Emit(UiCommand{
                UiCommandType::SetConnectionMode,
                selection,
                {}
            });
            return 0;
        }

        if (wparam ==
            kDropKindAudio) {
            if (selection > 3) {
                return 0;
            }

            Emit(UiCommand{
                UiCommandType::SelectAudioSource,
                selection,
                {}
            });
            return 0;
        }

        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int notification =
            HIWORD(wparam);

        if (!applying_state_ && notification == EN_CHANGE && (id == kBitrateId || id == kKeyframeId)) {
            video_dirty_ = true; video_validation_error_ = false;
            return 0;
        }
        if (notification == BN_CLICKED) {
            if (id == kEncoderId || id == kDecoderId) {
                ToggleDropList(id == kEncoderId ? encoder_list_ : decoder_list_);
                return 0;
            }
            if (id == kApplyVideoId) { HideDropLists(); ApplyVideoDraft(); return 0; }
            if (id == kLowLatencyId) { video_dirty_ = true; return 0; }
            if (id == kSourceId) {
                ToggleDropList(
                    source_list_);
                return 0;
            }

            if (id == kConnectionModeId) {
                ToggleDropList(
                    connection_mode_list_);
                return 0;
            }

            if (id == kAudioSourceId) {
                ToggleDropList(
                    audio_source_list_);
                return 0;
            }

            HideDropLists();

            switch (id) {
            case kShowCursorId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleShowCursor
                });
                return 0;

            case kPreviewSourceId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleCapturePreview
                });
                return 0;

            case kRefreshSourcesId:
                Emit(UiCommand{
                    UiCommandType::
                        RefreshCaptureSources
                });
                return 0;

            case kShareButtonId:
                Emit(UiCommand{
                    UiCommandType::
                        ToggleShareRequest
                });
                return 0;

            case kExportIdentityId:Emit(UiCommand{UiCommandType::ExportPeerIdentity});return 0;
            case kDisconnectPeerId:Emit(UiCommand{UiCommandType::DisconnectPeer});return 0;
            case kPairFriendId:Emit(UiCommand{UiCommandType::PairWithFriend});return 0;
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

        if (id == kSharePortEditId &&
            notification ==
                EN_CHANGE) {
            Emit(UiCommand{
                UiCommandType::SetSharePort,
                static_cast<int>(
                    ReadPort(share_port_edit_)),
                {}
            });
            return 0;
        }

        if (id == kRemotePortEditId &&
            notification ==
                EN_CHANGE) {
            Emit(UiCommand{
                UiCommandType::SetRemotePort,
                static_cast<int>(
                    ReadPort(remote_port_edit_)),
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
