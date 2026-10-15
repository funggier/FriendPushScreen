#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "capture/capture_source.h"

#include "ui/ui_command.h"
#include "ui/ui_state.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fps::ui {

class SettingsPanel {
public:
    using CommandCallback =
        std::function<void(const UiCommand&)>;

    bool Create(
        HWND parent,
        HINSTANCE instance);

    HWND Handle() const noexcept {
        return hwnd_;
    }

    void SetCommandCallback(
        CommandCallback callback);

    void SetBounds(
        int x,
        int y,
        int width,
        int height);

    void Show(bool visible);

    void ApplyState(const UiState& state);

    bool SetDiagnosticsText(
        std::wstring_view text);

    void SetCollecting(bool collecting);

    void SetCaptureSources(
        const std::vector<capture::CaptureSourceDescriptor>& sources,
        std::string_view selected_source_id,
        bool preview_active,
        std::string_view status_text);

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

    HWND CreateLabel(
        HINSTANCE instance,
        const wchar_t* text);

    HWND CreateButton(
        HINSTANCE instance,
        int id,
        const wchar_t* text,
        DWORD style = BS_PUSHBUTTON);

    HWND CreateEdit(
        HINSTANCE instance,
        int id,
        DWORD style = ES_AUTOHSCROLL);

    void InitializeControls(
        HINSTANCE instance);

    void ApplyDefaultFont();

    void LayoutControls();

    void UpdateScrollBar();

    void SetScrollOffset(int offset);

    void ScrollBySmoothDelta(int delta);

    void AnimateScrollStep();


    void Emit(UiCommand command);

    std::string ReadEditUtf8(HWND edit) const;

    std::uint32_t ReadPort(HWND edit) const;

    void UpdateDiagnosticsVisibility(bool visible);

    void HideDropLists();
    void ToggleDropList(HWND list);
    void UpdateDropDownText(const UiState& state);

    HWND hwnd_{nullptr};

    HWND title_label_{nullptr};

    HWND share_heading_{nullptr};
    HWND source_label_{nullptr};
    HWND show_cursor_checkbox_{nullptr};
    HWND source_button_{nullptr};
    HWND source_popup_{nullptr};
    HWND source_list_{nullptr};
    HWND source_preview_button_{nullptr};
    HWND source_refresh_button_{nullptr};
    HWND source_status_{nullptr};
    HWND share_port_label_{nullptr};
    HWND share_port_edit_{nullptr};
    HWND share_button_{nullptr};
    HWND share_status_{nullptr};

    HWND connection_heading_{nullptr};
    HWND connection_mode_label_{nullptr};
    HWND connection_mode_button_{nullptr};
    HWND connection_mode_popup_{nullptr};
    HWND connection_mode_list_{nullptr};
    HWND host_label_{nullptr};
    HWND host_edit_{nullptr};
    HWND remote_port_label_{nullptr};
    HWND remote_port_edit_{nullptr};
    HWND connect_button_{nullptr};
    HWND connection_status_{nullptr};

    HWND audio_heading_{nullptr};
    HWND audio_source_label_{nullptr};
    HWND audio_source_button_{nullptr};
    HWND audio_source_popup_{nullptr};
    HWND audio_source_list_{nullptr};
    HWND audio_status_{nullptr};
    HWND mute_button_{nullptr};
    HWND volume_slider_{nullptr};
    HWND volume_label_{nullptr};
    HWND settings_scrollbar_{nullptr};

    HWND viewer_heading_{nullptr};
    HWND topmost_checkbox_{nullptr};
    HWND borderless_checkbox_{nullptr};
    HWND fullscreen_button_{nullptr};

    HWND advanced_heading_{nullptr};
    HWND diagnostics_toggle_{nullptr};
    HWND diagnostics_edit_{nullptr};
    HWND refresh_button_{nullptr};
    HWND save_button_{nullptr};

    std::vector<std::string> capture_source_ids_;

    CommandCallback command_callback_;

    bool applying_state_{false};
    bool diagnostics_visible_{false};
    int scroll_offset_{0};
    int scroll_target_offset_{0};
    int content_height_{0};
};

}  // namespace fps::ui
