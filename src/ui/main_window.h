#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "ui/settings_panel.h"
#include "ui/ui_command.h"
#include "ui/ui_state.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fps::ui {

std::wstring NormalizeWin32MultilineText(
    std::wstring_view text);

struct ViewerViewport {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

ViewerViewport CalculateViewerViewport(
    int client_width,
    int client_height,
    int status_height);

class MainWindow {
public:
    using ResizeCallback =
        std::function<void(UINT, UINT)>;
    using FrameCallback =
        std::function<void()>;
    using UiCommandCallback =
        std::function<void(const UiCommand&)>;
    using CommandCallback =
        std::function<void()>;

    bool Create(
        HINSTANCE instance,
        int show_command);

    HWND Handle() const noexcept {
        return hwnd_;
    }

    HWND RenderHandle() const noexcept {
        return render_hwnd_;
    }

    void SetResizeCallback(
        ResizeCallback callback);

    void SetFrameCallback(
        FrameCallback callback);

    void SetUiCommandCallback(
        UiCommandCallback callback);

    void SetDiagnosticsReadyCallback(
        CommandCallback callback);

    void SetStatusText(
        std::wstring_view text);

    bool SetDiagnosticsText(
        std::wstring_view text);

    void SetCollecting(bool collecting);

    void SetCaptureSources(
        const std::vector<capture::CaptureSourceDescriptor>& sources,
        std::string_view selected_source_id,
        bool preview_active,
        bool show_cursor,
        std::string_view status_text);

    std::optional<std::filesystem::path>
    PromptDiagnosticsSavePath();

    void ApplyUiState(
        const UiState& state);

    bool HandleShortcut(
        const MSG& message);

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

    void Emit(UiCommand command);

    void ApplyWindowState(
        const UiState& state);

    void ApplyBorderless(bool borderless);

    void EnterFullscreen(
        const UiState& state);

    void ExitFullscreen(
        const UiState& state);

    void ApplyAlwaysOnTop(bool enabled);

    std::wstring BuildStatusSummary(
        const UiState& state) const;

    HWND hwnd_{nullptr};
    HWND render_hwnd_{nullptr};
    HWND status_bar_{nullptr};
    HWND settings_toggle_button_{nullptr};

    SettingsPanel settings_panel_;

    ResizeCallback resize_callback_;
    FrameCallback frame_callback_;
    UiCommandCallback ui_command_callback_;
    CommandCallback diagnostics_ready_callback_;

    bool settings_visible_{true};

    bool applied_fullscreen_{false};
    bool applied_borderless_{false};
    bool applied_topmost_{false};

    WINDOWPLACEMENT windowed_placement_{
        sizeof(WINDOWPLACEMENT)};
    bool have_windowed_placement_{false};
};

}  // namespace fps::ui
