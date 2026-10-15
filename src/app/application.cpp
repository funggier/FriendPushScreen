#include "app/application.h"

#include "capture/capture_source_catalog.h"

#include "capture/desktop_duplication_capture.h"

#include "capture/media_foundation_capture.h"

#include "capture/windows_graphics_capture.h"

#include "diagnostics/diagnostics_formatter.h"
#include "diagnostics/logger.h"
#include "platform/windows/os_probe.h"
#include "ui/settings_store.h"

#include <objbase.h>
#include <roapi.h>

#include <algorithm>
#include <utility>
#include <chrono>
#include <cstdint>
#include <string>

namespace fps::app {
namespace {

const wchar_t* RendererStatusText(
    graphics::RendererMode mode) {
    switch (mode) {
    case graphics::RendererMode::Hardware:
        return L"D3D11 Hardware";
    case graphics::RendererMode::WarpFallback:
        return L"D3D11 WARP Fallback";
    case graphics::RendererMode::Error:
        return L"D3D11 Renderer Error";
    }

    return L"D3D11 Renderer Error";
}

std::wstring Utf8ToWide(
    const std::string& value) {
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
        return
            L"[UTF-8 conversion failed]";
    }

    std::wstring output(
        static_cast<std::size_t>(
            required),
        L'\0');

    const int written =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            output.data(),
            required);

    if (written != required) {
        return
            L"[UTF-8 conversion failed]";
    }

    return output;
}

std::string WideStatusToUtf8(
    const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required =
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

    if (required <= 0) {
        return "Unknown";
    }

    std::string output(
        static_cast<std::size_t>(
            required),
        '\0');

    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            output.data(),
            required,
            nullptr,
            nullptr) !=
        required) {
        return "Unknown";
    }

    return output;
}

bool IsPreferenceCommand(
    ui::UiCommandType type) {
    switch (type) {
    case ui::UiCommandType::ToggleSettings:
    case ui::UiCommandType::ToggleShowCursor:
    case ui::UiCommandType::SetConnectionMode:
    case ui::UiCommandType::SetHost:
    case ui::UiCommandType::SetSharePort:
    case ui::UiCommandType::SetRemotePort:
    case ui::UiCommandType::SelectAudioSource:
    case ui::UiCommandType::ToggleReceiverMute:
    case ui::UiCommandType::SetReceiverVolume:
    case ui::UiCommandType::ToggleAlwaysOnTop:
    case ui::UiCommandType::ToggleBorderless:
        return true;

    default:
        return false;
    }
}

}  // namespace

void Application::RefreshDiagnostics() {
    if (diagnostics_service_.
            StartCollection(
                main_window_.Handle())) {
        main_window_.SetCollecting(true);

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Info,
                "diagnostics",
                "collection started");
    }
}

void Application::OnDiagnosticsReady() {
    const auto snapshot =
        diagnostics_service_.Latest();

    main_window_.SetCollecting(false);

    if (snapshot == nullptr) {
        main_window_.SetDiagnosticsText(
            L"Diagnostics collection "
            L"completed without a snapshot.");
        return;
    }

    std::string text;
    text += "Renderer\n";
    text += "Mode: ";
    text += WideStatusToUtf8(
        renderer_status_);
    text += "\n\n";
    text += diagnostics::
        FormatDiagnosticsText(
            *snapshot,
            true);

    const std::wstring wide_text =
        Utf8ToWide(text);

    const bool ui_updated =
        main_window_.SetDiagnosticsText(
            wide_text);

    const std::string update_message =
        "collection completed; chars=" +
        std::to_string(
            wide_text.size()) +
        "; ui_update=" +
        (ui_updated
             ? "true"
             : "false");

    diagnostics::Logger::Instance().
        Log(
            ui_updated
                ? diagnostics::LogLevel::Info
                : diagnostics::LogLevel::Error,
            "diagnostics",
            update_message);
}

void Application::SaveDiagnostics() {
    const auto snapshot =
        diagnostics_service_.Latest();

    if (snapshot == nullptr) {
        MessageBoxW(
            main_window_.Handle(),
            L"No completed diagnostics "
            L"snapshot is available yet.",
            L"FriendPushScreen",
            MB_OK |
                MB_ICONINFORMATION);
        return;
    }

    const auto path =
        main_window_.
            PromptDiagnosticsSavePath();

    if (!path.has_value()) {
        return;
    }

    if (diagnostics::SaveDiagnosticsUtf8(
            *snapshot,
            *path)) {
        MessageBoxW(
            main_window_.Handle(),
            L"Diagnostics report saved.",
            L"FriendPushScreen",
            MB_OK |
                MB_ICONINFORMATION);

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Info,
                "diagnostics",
                "diagnostics report saved");
    } else {
        MessageBoxW(
            main_window_.Handle(),
            L"Could not save the "
            L"diagnostics report.",
            L"FriendPushScreen",
            MB_OK |
                MB_ICONERROR);

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Error,
                "diagnostics",
                "diagnostics report save failed");
    }
}

void Application::PersistUiPreferences() {
    if (!ui::SavePreferencesUtf8(
            ui::PersistentPreferencesFromState(
                ui_state_),
            ui::DefaultSettingsPath())) {
        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Warning,
                "ui",
                "could not persist UI preferences");
    }
}

void Application::ApplyUiState() {
    main_window_.ApplyUiState(
        ui_state_);
    UpdateCaptureUi();
}

void Application::UpdateCaptureUi() {
    bool preview_active = false;

    if (capture_session_ != nullptr) {
        const auto state =
            capture_session_->State();

        preview_active =
            state ==
                capture::CaptureSessionState::Starting ||
            state ==
                capture::CaptureSessionState::Previewing;
    }

    main_window_.SetCaptureSources(
        capture_sources_,
        selected_source_id_,
        preview_active,
        ui_state_.preferences.show_cursor,
        capture_status_);
}

void Application::RefreshCaptureSources() {
    auto refreshed =
        capture::BuildCaptureSourceCatalog(
            main_window_.Handle());

    if (selected_source_.has_value()) {
        auto selected =
            std::find_if(
                refreshed.begin(),
                refreshed.end(),
                [this](const auto& source) {
                    return source.id ==
                        selected_source_id_;
                });

        if (selected != refreshed.end()) {
            selected_source_ = *selected;
        } else {
            auto unavailable =
                *selected_source_;
            unavailable.availability =
                capture::CaptureAvailability::
                    Unavailable;
            selected_source_ = unavailable;
            refreshed.push_back(
                std::move(unavailable));

            if (capture_session_ != nullptr &&
                capture_session_->State() ==
                    capture::CaptureSessionState::
                        SourceLost) {
                capture_status_ =
                    capture_session_->
                        StatusText();
            }
        }
    }

    capture_sources_ =
        std::move(refreshed);

    UpdateCaptureUi();
}

std::unique_ptr<capture::ICaptureSession>
Application::CreateCaptureSession(
    const capture::CaptureSourceDescriptor& source) {
    std::unique_ptr<capture::ICaptureSession>
        session;

    switch (source.kind) {
    case capture::CaptureSourceKind::Monitor:
        session = std::make_unique<
            capture::DesktopDuplicationCaptureSession>(
                source,
                renderer_.Device());
        break;

    case capture::CaptureSourceKind::Window:
        session = std::make_unique<
            capture::WindowsGraphicsCaptureSession>(
                source,
                renderer_.Device());
        break;

    case capture::CaptureSourceKind::VideoDevice:
        session = std::make_unique<
            capture::MediaFoundationCaptureSession>(
                source);
        break;
    }

    if (session != nullptr) {
        session->SetCursorCaptureEnabled(
            ui_state_.preferences.show_cursor);
    }

    return session;
}

void Application::StopCaptureSession() {
    if (capture_session_ != nullptr) {
        capture_session_->Stop();
        capture_session_.reset();
    }

    last_preview_frame_ = {};
    last_capture_state_ =
        capture::CaptureSessionState::Stopped;
}

void Application::SelectCaptureSource(
    std::string_view source_id) {
    const auto found =
        std::find_if(
            capture_sources_.begin(),
            capture_sources_.end(),
            [source_id](const auto& source) {
                return source.id == source_id;
            });

    if (found == capture_sources_.end()) {
        capture_status_ =
            "Selected source is no longer available";
        UpdateCaptureUi();
        return;
    }

    if (found->availability !=
        capture::CaptureAvailability::Available) {
        capture_status_ =
            "Selected source is unavailable";
        UpdateCaptureUi();
        return;
    }

    if (capture_session_ != nullptr) {
        StopCaptureSession();
        if (renderer_initialized_) {
            renderer_.RenderPlaceholder();
        }
    }

    selected_source_ = *found;
    selected_source_id_ = found->id;
    capture_status_ =
        "Ready to preview";

    UpdateCaptureUi();
}

void Application::StartCapturePreview() {
    if (!selected_source_.has_value()) {
        capture_status_ =
            "Select a source before previewing";
        UpdateCaptureUi();
        return;
    }

    if (selected_source_->availability !=
        capture::CaptureAvailability::Available) {
        capture_status_ =
            "Selected source is unavailable";
        UpdateCaptureUi();
        return;
    }

    StopCaptureSession();

    capture_status_ =
        "Starting local preview...";

    capture_session_ =
        CreateCaptureSession(
            *selected_source_);

    if (capture_session_ == nullptr) {
        capture_status_ =
            "No capture backend is available for this source";
        UpdateCaptureUi();
        return;
    }

    if (!capture_session_->Start()) {
        capture_status_ =
            capture_session_->StatusText();
        last_capture_state_ =
            capture_session_->State();
        capture_session_.reset();
        if (renderer_initialized_) {
            renderer_.RenderPlaceholder();
        }
        UpdateCaptureUi();
        return;
    }

    last_capture_state_ =
        capture_session_->State();
    capture_status_ =
        capture_session_->StatusText();

    if (renderer_initialized_) {
        renderer_.RenderPlaceholder();
    }

    UpdateCaptureUi();
}

void Application::StopCapturePreview() {
    StopCaptureSession();

    if (renderer_initialized_) {
        renderer_.RenderPlaceholder();
    }

    capture_status_ =
        selected_source_.has_value()
            ? "Ready to preview"
            : "No source selected";

    UpdateCaptureUi();
}

void Application::ToggleCapturePreview() {
    bool preview_active = false;

    if (capture_session_ != nullptr) {
        const auto state =
            capture_session_->State();
        preview_active =
            state ==
                capture::CaptureSessionState::Starting ||
            state ==
                capture::CaptureSessionState::Previewing;
    }

    if (preview_active) {
        StopCapturePreview();
    } else {
        StartCapturePreview();
    }
}

void Application::TickCapturePreview() {
    if (!renderer_initialized_ ||
        capture_session_ == nullptr) {
        return;
    }

    capture::CapturedFrame frame{};

    if (capture_session_->TryAcquireFrame(
            frame) &&
        frame.IsValid()) {
        if (renderer_.RenderFrame(frame)) {
            last_preview_frame_ =
                std::move(frame);
        } else {
            capture_status_ =
                "Preview renderer failed";
            last_preview_frame_ = {};
            renderer_.RenderPlaceholder();
            UpdateCaptureUi();
        }
    }

    const auto state =
        capture_session_->State();

    if (state == last_capture_state_) {
        return;
    }

    last_capture_state_ = state;
    capture_status_ =
        capture_session_->StatusText();

    if (state ==
            capture::CaptureSessionState::
                SourceLost ||
        state ==
            capture::CaptureSessionState::
                Error) {
        last_preview_frame_ = {};
        renderer_.RenderPlaceholder();
    }

    if (state ==
        capture::CaptureSessionState::SourceLost) {
        RefreshCaptureSources();
        return;
    }

    UpdateCaptureUi();
}

void Application::HandleUiCommand(
    const ui::UiCommand& command) {
    bool state_changed = false;

    switch (command.type) {
    case ui::UiCommandType::ToggleSettings:
        ui::ToggleSettingsVisible(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleShareRequest:
        ui::ToggleShareRequest(
            ui_state_);
        state_changed = true;

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Info,
                "ui",
                ui_state_.share_state ==
                        ui::ShareUiState::
                            ShareRequested
                    ? "share requested; "
                      "media pipeline not active"
                    : "share request cancelled");
        break;

    case ui::UiCommandType::SelectCaptureSource:
        SelectCaptureSource(command.text);
        return;

    case ui::UiCommandType::ToggleCapturePreview:
        ToggleCapturePreview();
        return;

    case ui::UiCommandType::RefreshCaptureSources:
        RefreshCaptureSources();
        return;

    case ui::UiCommandType::ToggleShowCursor:
        ui::ToggleShowCursor(ui_state_);
        if (capture_session_ != nullptr) {
            capture_session_->
                SetCursorCaptureEnabled(
                    ui_state_.preferences.
                        show_cursor);
        }
        state_changed = true;
        break;

    case ui::UiCommandType::
        ToggleConnectionRequest:
        ui::ToggleConnectionRequest(
            ui_state_);
        state_changed = true;

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Info,
                "ui",
                ui_state_.connection_state ==
                        ui::ConnectionUiState::
                            ConnectRequested
                    ? "connect requested; "
                      "transport not active"
                    : "connect request cancelled");
        break;

    case ui::UiCommandType::SetConnectionMode:
        if (command.value >= 0 &&
            command.value <= 2) {
            ui_state_.preferences.
                connection_mode =
                static_cast<
                    ui::ConnectionMode>(
                    command.value);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::SetHost:
        ui_state_.preferences.host =
            command.text;
        state_changed = true;
        break;

    case ui::UiCommandType::SetSharePort:
        if (command.value >= 1 &&
            command.value <= 65535) {
            ui_state_.preferences.share_port =
                static_cast<std::uint32_t>(
                    command.value);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::SetRemotePort:
        if (command.value >= 1 &&
            command.value <= 65535) {
            ui_state_.preferences.remote_port =
                static_cast<std::uint32_t>(
                    command.value);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::SelectAudioSource:
        if (command.value >= 0 &&
            command.value <= 3) {
            const auto source =
                static_cast<
                    ui::AudioSourceKind>(
                    command.value);

            if (ui::SelectAudioSource(
                    ui_state_,
                    source)) {
                state_changed = true;
            } else {
                diagnostics::Logger::Instance().
                    Log(
                        diagnostics::LogLevel::Info,
                        "ui",
                        "Application Audio selection "
                        "rejected: unsupported on "
                        "this Windows build");
                ApplyUiState();
                return;
            }
        }
        break;

    case ui::UiCommandType::ToggleReceiverMute:
        ui::ToggleReceiverMute(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::SetReceiverVolume:
        ui::SetReceiverVolume(
            ui_state_,
            command.value);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleAlwaysOnTop:
        ui::ToggleAlwaysOnTop(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleBorderless:
        ui::ToggleBorderless(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleFullscreen:
        ui::ToggleFullscreen(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ExitFullscreen:
        if (ui_state_.fullscreen) {
            ui::SetFullscreen(
                ui_state_,
                false);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::ToggleDiagnostics:
        ui::ToggleDiagnosticsVisible(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::RefreshDiagnostics:
        RefreshDiagnostics();
        return;

    case ui::UiCommandType::SaveDiagnostics:
        SaveDiagnostics();
        return;
    }

    if (state_changed) {
        if (IsPreferenceCommand(
                command.type)) {
            PersistUiPreferences();
        }

        ApplyUiState();
    }
}

int Application::Run(
    HINSTANCE instance,
    int show_command) {
    const HRESULT runtime_hr =
        RoInitialize(
            RO_INIT_SINGLETHREADED);

    const bool should_uninitialize =
        SUCCEEDED(runtime_hr);

    if (FAILED(runtime_hr) &&
        runtime_hr != RPC_E_CHANGED_MODE) {
        return 3;
    }

    const auto os =
        platform::windows::
            ProbeOperatingSystem();

    const std::uint32_t windows_build =
        os.build;

    const ui::UiPreferences preferences =
        ui::LoadPreferences(
            ui::DefaultSettingsPath(),
            ui::UiPreferences{});

    ui_state_ =
        ui::MakeStateFromPreferences(
            preferences,
            windows_build);

    main_window_.SetResizeCallback(
        [this](
            UINT width,
            UINT height) {
            if (!renderer_initialized_) {
                return;
            }

            renderer_.Resize(
                width,
                height);

            if (last_preview_frame_.IsValid()) {
                renderer_.RenderFrame(
                    last_preview_frame_);
            } else {
                renderer_.RenderPlaceholder();
            }
        });

    main_window_.SetFrameCallback(
        [this]() {
            TickCapturePreview();
        });

    main_window_.SetUiCommandCallback(
        [this](
            const ui::UiCommand& command) {
            HandleUiCommand(command);
        });

    main_window_.SetDiagnosticsReadyCallback(
        [this]() {
            OnDiagnosticsReady();
        });

    if (!main_window_.Create(
            instance,
            show_command)) {
        if (should_uninitialize) {
            RoUninitialize();
        }

        return 1;
    }

    renderer_initialized_ =
        renderer_.Initialize(
            main_window_.RenderHandle());

    renderer_status_ =
        RendererStatusText(
            renderer_.Mode());

    main_window_.SetStatusText(
        renderer_status_);

    if (renderer_initialized_) {
        RECT client{};

        if (GetClientRect(
                main_window_.RenderHandle(),
                &client) != FALSE) {
            renderer_.Resize(
                static_cast<UINT>(
                    client.right -
                    client.left),
                static_cast<UINT>(
                    client.bottom -
                    client.top));
        }

        renderer_.RenderPlaceholder();
    }

    ApplyUiState();

    // Normalize persisted values against this
    // Windows runtime (for example an Application
    // Audio preference moved from a newer OS).
    PersistUiPreferences();

    RefreshCaptureSources();
    RefreshDiagnostics();

    MSG message{};

    while (true) {
        const BOOL result =
            GetMessageW(
                &message,
                nullptr,
                0,
                0);

        if (result == 0) {
            break;
        }

        if (result == -1) {
            StopCaptureSession();

            if (should_uninitialize) {
                RoUninitialize();
            }

            return 2;
        }

        if (main_window_.
                HandleShortcut(
                    message)) {
            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    StopCaptureSession();

    diagnostics_service_.WaitForIdle(
        std::chrono::seconds{5});

    if (should_uninitialize) {
        RoUninitialize();
    }

    return static_cast<int>(
        message.wParam);
}

}  // namespace fps::app
