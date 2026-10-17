#pragma once

#include "ui/ui_state.h"

#include <string>

namespace fps::ui {

enum class UiCommandType {
    ToggleSettings,
    ToggleStatusBar,
    ApplyVideoSettings,
    ToggleShareRequest,
    SelectCaptureSource,
    ToggleCapturePreview,
    RefreshCaptureSources,
    ToggleShowCursor,
    ToggleConnectionRequest,
    ExportPeerIdentity,
    PairWithFriend,
    DisconnectPeer,
    SetHost,
    SetSharePort,
    SetRemotePort,
    SelectAudioSource,
    SelectAudioOutput,
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
