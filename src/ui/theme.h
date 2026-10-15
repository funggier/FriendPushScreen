#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace fps::ui::theme {

inline constexpr COLORREF kWindowBackground =
    RGB(12, 15, 20);
inline constexpr COLORREF kPanelBackground =
    RGB(18, 22, 28);
inline constexpr COLORREF kSurfaceBackground =
    RGB(29, 35, 44);
inline constexpr COLORREF kSurfaceHover =
    RGB(36, 44, 55);
inline constexpr COLORREF kBorder =
    RGB(58, 68, 80);
inline constexpr COLORREF kButton =
    RGB(38, 46, 57);
inline constexpr COLORREF kButtonPressed =
    RGB(48, 58, 71);
inline constexpr COLORREF kTextPrimary =
    RGB(238, 242, 247);
inline constexpr COLORREF kTextSecondary =
    RGB(174, 184, 196);
inline constexpr COLORREF kSliderTrack =
    RGB(48, 57, 68);
inline constexpr COLORREF kSliderThumb =
    RGB(104, 118, 134);

HBRUSH WindowBrush();
HBRUSH PanelBrush();
HBRUSH SurfaceBrush();
HBRUSH BorderBrush();
HBRUSH ButtonBrush();
HBRUSH ButtonPressedBrush();
HBRUSH SliderTrackBrush();
HBRUSH SliderThumbBrush();

void ApplyDarkBorder(HWND hwnd);

void DrawButton(
    const DRAWITEMSTRUCT& item);

void DrawComboItem(
    const DRAWITEMSTRUCT& item);

void DrawListItem(
    const DRAWITEMSTRUCT& item);

}  // namespace fps::ui::theme
