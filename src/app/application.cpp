#include "app/application.h"

#include "diagnostics/diagnostics_formatter.h"
#include "diagnostics/logger.h"
#include "platform/windows/os_probe.h"
#include "ui/settings_store.h"

#include <objbase.h>

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
    const HRESULT com_hr =
        CoInitializeEx(
            nullptr,
            COINIT_APARTMENTTHREADED);

    const bool should_uninitialize =
        SUCCEEDED(com_hr);

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
            renderer_.Render();
        });

    main_window_.SetFrameCallback(
        [this]() {
            if (renderer_initialized_) {
                renderer_.Render();
            }
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
            CoUninitialize();
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

        renderer_.Render();
    }

    ApplyUiState();

    // Normalize persisted values against this
    // Windows runtime (for example an Application
    // Audio preference moved from a newer OS).
    PersistUiPreferences();

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
            if (should_uninitialize) {
                CoUninitialize();
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

    diagnostics_service_.WaitForIdle(
        std::chrono::seconds{5});

    if (should_uninitialize) {
        CoUninitialize();
    }

    return static_cast<int>(
        message.wParam);
}

}  // namespace fps::app
