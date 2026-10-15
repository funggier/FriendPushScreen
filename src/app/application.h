#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "capture/capture_frame.h"
#include "capture/capture_session.h"
#include "capture/capture_source.h"
#include "diagnostics/diagnostics_service.h"
#include "graphics/preview_renderer.h"
#include "media/video_local_loop.h"
#include "ui/main_window.h"
#include "ui/ui_command.h"
#include "ui/ui_state.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fps::app {

class Application {
public:
    int Run(
        HINSTANCE instance,
        int show_command);

private:
    void RefreshDiagnostics();
    void OnDiagnosticsReady();
    void SaveDiagnostics();

    void RefreshCaptureSources();
    void SelectCaptureSource(
        std::string_view source_id);
    void StartCapturePreview();
    void StopCapturePreview();
    void ToggleCapturePreview();

    std::unique_ptr<capture::ICaptureSession>
    CreateCaptureSession(
        const capture::CaptureSourceDescriptor&
            source);

    void StopCaptureSession();
    void TickCapturePreview();
    void UpdateCaptureUi();

    void HandleUiCommand(
        const ui::UiCommand& command);

    void PersistUiPreferences();
    void ApplyUiState();

    ui::MainWindow main_window_;
    graphics::PreviewRenderer renderer_;

    diagnostics::DiagnosticsService
        diagnostics_service_;

    ui::UiState ui_state_{};

    std::vector<
        capture::CaptureSourceDescriptor>
        capture_sources_;

    std::optional<
        capture::CaptureSourceDescriptor>
        selected_source_;

    std::string selected_source_id_;
    std::string capture_status_{
        "No source selected"};

    std::unique_ptr<
        capture::ICaptureSession>
        capture_session_;

    capture::CapturedFrame
        last_preview_frame_{};

    capture::CaptureSessionState
        last_capture_state_{
            capture::CaptureSessionState::
                Stopped};

    std::unique_ptr<media::VideoLocalLoop> codec_loop_;
    bool codec_failed_{false};
    bool raw_preview_{false};
    bool force_software_codec_{false};
    ULONGLONG last_codec_status_tick_{0};
    bool renderer_initialized_{false};
    std::wstring renderer_status_;
};

}  // namespace fps::app
