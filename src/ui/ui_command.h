#pragma once

#include "ui/ui_state.h"

#include <string>

namespace fps::ui {

enum class UiCommandType {
    ToggleSettings,
    ApplyVideoSettings,
    ToggleShareRequest,
    SelectCaptureSource,
    ToggleCapturePreview,
    RefreshCaptureSources,
    ToggleShowCursor,
    ToggleConnectionRequest,
    SetConnectionMode,
    SetHost,
    SetSharePort,
    SetRemotePort,
    SelectAudioSource,
    ToggleReceiverMute,
    SetReceiverVolume,
    ToggleAlwaysOnTop,
    ToggleBorderless,
    ToggleFullscreen,
    ExitFullscreen,
    ToggleDiagnostics,
    RefreshDiagnostics,
    SaveDiagnostics
};

struct UiCommand {
    UiCommandType type{UiCommandType::ToggleSettings};
    int value{0};
    std::string text;
    VideoPreferences video;
};

}  // namespace fps::ui
