#include "ui/theme.h"

#include <CommCtrl.h>

#include <array>

namespace fps::ui::theme {
namespace {

HBRUSH MakeCachedBrush(COLORREF color) {
    return CreateSolidBrush(color);
}

LRESULT CALLBACK DarkBorderSubclassProc(
    HWND hwnd,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    UINT_PTR subclass_id,
    DWORD_PTR) {
    if (message == WM_NCPAINT) {
        const LRESULT result =
            DefSubclassProc(
                hwnd,
                message,
                wparam,
                lparam);

        const HDC dc = GetWindowDC(hwnd);
        if (dc != nullptr) {
            RECT rect{};
            GetWindowRect(hwnd, &rect);
            OffsetRect(
                &rect,
                -rect.left,
                -rect.top);

            FrameRect(
                dc,
                &rect,
                BorderBrush());

            ReleaseDC(hwnd, dc);
        }

        return result;
    }

    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(
            hwnd,
            DarkBorderSubclassProc,
            subclass_id);
    }

    return DefSubclassProc(
        hwnd,
        message,
        wparam,
        lparam);
}

}  // namespace

HBRUSH WindowBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kWindowBackground);
    return brush;
}

HBRUSH PanelBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kPanelBackground);
    return brush;
}

HBRUSH SurfaceBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kSurfaceBackground);
    return brush;
}

HBRUSH BorderBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kBorder);
    return brush;
}

HBRUSH ButtonBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kButton);
    return brush;
}

HBRUSH ButtonPressedBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kButtonPressed);
    return brush;
}

HBRUSH SliderTrackBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kSliderTrack);
    return brush;
}

HBRUSH SliderThumbBrush() {
    static HBRUSH brush =
        MakeCachedBrush(kSliderThumb);
    return brush;
}

void ApplyDarkBorder(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }

    SetWindowSubclass(
        hwnd,
        DarkBorderSubclassProc,
        1,
        0);

    RedrawWindow(
        hwnd,
        nullptr,
        nullptr,
        RDW_FRAME |
            RDW_INVALIDATE);
}

void DrawButton(
    const DRAWITEMSTRUCT& item) {
    RECT rect = item.rcItem;

    const bool pressed =
        (item.itemState & ODS_SELECTED) != 0;
    const bool disabled =
        (item.itemState & ODS_DISABLED) != 0;

    FillRect(
        item.hDC,
        &rect,
        pressed
            ? ButtonPressedBrush()
            : ButtonBrush());

    FrameRect(
        item.hDC,
        &rect,
        BorderBrush());

    std::array<wchar_t, 256> text{};
    GetWindowTextW(
        item.hwndItem,
        text.data(),
        static_cast<int>(text.size()));

    SetBkMode(
        item.hDC,
        TRANSPARENT);

    SetTextColor(
        item.hDC,
        disabled
            ? kTextSecondary
            : kTextPrimary);

    DrawTextW(
        item.hDC,
        text.data(),
        -1,
        &rect,
        DT_CENTER |
            DT_VCENTER |
            DT_SINGLELINE |
            DT_END_ELLIPSIS);

    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = rect;
        InflateRect(
            &focus,
            -3,
            -3);
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void DrawComboItem(
    const DRAWITEMSTRUCT& item) {
    RECT rect = item.rcItem;

    const bool selected =
        (item.itemState & ODS_SELECTED) != 0;
    const bool disabled =
        (item.itemState & ODS_DISABLED) != 0;

    FillRect(
        item.hDC,
        &rect,
        selected
            ? ButtonBrush()
            : SurfaceBrush());

    if ((item.itemState & ODS_COMBOBOXEDIT) != 0) {
        FrameRect(
            item.hDC,
            &rect,
            BorderBrush());
    }

    if (item.itemID !=
        static_cast<UINT>(-1)) {
        std::array<wchar_t, 256> text{};

        SendMessageW(
            item.hwndItem,
            CB_GETLBTEXT,
            item.itemID,
            reinterpret_cast<LPARAM>(
                text.data()));

        SetBkMode(
            item.hDC,
            TRANSPARENT);

        SetTextColor(
            item.hDC,
            disabled
                ? kTextSecondary
                : kTextPrimary);

        RECT text_rect = rect;
        text_rect.left += 8;
        text_rect.right -= 6;

        DrawTextW(
            item.hDC,
            text.data(),
            -1,
            &text_rect,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS);
    }

    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = rect;
        InflateRect(
            &focus,
            -2,
            -2);
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

void DrawListItem(
    const DRAWITEMSTRUCT& item) {
    RECT rect = item.rcItem;

    const bool selected =
        (item.itemState & ODS_SELECTED) != 0;
    const bool disabled =
        (item.itemState & ODS_DISABLED) != 0;

    FillRect(
        item.hDC,
        &rect,
        selected
            ? ButtonBrush()
            : SurfaceBrush());

    if (item.itemID !=
        static_cast<UINT>(-1)) {
        std::array<wchar_t, 256> text{};

        SendMessageW(
            item.hwndItem,
            LB_GETTEXT,
            item.itemID,
            reinterpret_cast<LPARAM>(
                text.data()));

        SetBkMode(
            item.hDC,
            TRANSPARENT);

        SetTextColor(
            item.hDC,
            disabled
                ? kTextSecondary
                : kTextPrimary);

        RECT text_rect = rect;
        text_rect.left += 8;
        text_rect.right -= 6;

        DrawTextW(
            item.hDC,
            text.data(),
            -1,
            &text_rect,
            DT_LEFT |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_END_ELLIPSIS);
    }

    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = rect;
        InflateRect(
            &focus,
            -2,
            -2);
        DrawFocusRect(
            item.hDC,
            &focus);
    }
}

}  // namespace fps::ui::theme
