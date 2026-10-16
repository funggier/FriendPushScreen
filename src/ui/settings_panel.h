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

    void SetVideoCodecStatus(std::wstring_view text);

    void SetCaptureSources(
        const std::vector<capture::CaptureSourceDescriptor>& sources,
        std::string_view selected_source_id,
        bool preview_active,
        bool show_cursor,
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

    void InitializeVideoControls(HINSTANCE instance, HWND root_owner);
    void UpdateVideoChoices();
    void ApplyVideoDraft();

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
    void UpdateAudioChoices(const UiState&);

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

    struct VideoChoice {
        media::VideoBackendPolicy policy;
        std::string id;
        std::wstring text;
    };
    std::vector<VideoChoice> encoder_choices_, decoder_choices_;
    VideoPreferences video_draft_;
    bool video_initialized_{false}, video_dirty_{false}, video_validation_error_{false};
    bool video_apply_pending_{false};
    HWND video_heading_{nullptr}, encoder_label_{nullptr}, decoder_label_{nullptr};
    HWND encoder_button_{nullptr}, decoder_button_{nullptr};
    HWND encoder_popup_{nullptr}, decoder_popup_{nullptr}, encoder_list_{nullptr}, decoder_list_{nullptr};
    HWND bitrate_label_{nullptr}, bitrate_edit_{nullptr}, keyframe_label_{nullptr}, keyframe_edit_{nullptr};
    HWND low_latency_checkbox_{nullptr}, video_apply_button_{nullptr}, video_status_{nullptr};

    HWND connection_heading_{nullptr};
    HWND connection_mode_label_{nullptr};
    HWND connection_mode_button_{nullptr};
    HWND connection_mode_popup_{nullptr};
    HWND connection_mode_list_{nullptr};
    HWND host_label_{nullptr};
    HWND host_edit_{nullptr};
    HWND remote_port_label_{nullptr};
    HWND remote_port_edit_{nullptr};
    HWND connect_button_{nullptr},disconnect_button_{nullptr};
    HWND connection_status_{nullptr};
    HWND export_identity_button_{nullptr},pair_friend_button_{nullptr},own_identity_label_{nullptr},peer_identity_label_{nullptr};

    HWND audio_heading_{nullptr};
    HWND audio_source_label_{nullptr};
    HWND audio_source_button_{nullptr};
    HWND audio_source_popup_{nullptr};
    HWND audio_source_list_{nullptr};
    HWND audio_status_{nullptr};
    HWND audio_output_label_{nullptr},audio_output_button_{nullptr},audio_output_popup_{nullptr},audio_output_list_{nullptr};
    std::vector<audio::AudioSourceDescriptor> audio_source_choices_;
    std::vector<audio::AudioEndpoint> audio_output_choices_;
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
    std::vector<capture::CaptureSourceDescriptor> applied_sources_;
    std::string applied_source_id_;

    CommandCallback command_callback_;

    bool dropdown_initialized_{false};
    ConnectionMode applied_connection_mode_{ConnectionMode::DirectIpDdNs};
    AudioSourceKind applied_audio_source_{AudioSourceKind::None};
    bool applying_state_{false};
    bool diagnostics_visible_{false};
    int scroll_offset_{0};
    int scroll_target_offset_{0};
    int content_height_{0};
};

}  // namespace fps::ui
