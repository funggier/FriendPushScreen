#pragma once

#include <cstdint>
#include <string>

namespace fps::ui {

enum class ConnectionMode {
    Lan,
    DirectIpDdNs,
    Tailscale
};

enum class ConnectionUiState {
    Disconnected,
    ConnectRequested,
    Connected
};

enum class ShareUiState {
    NotSharing,
    ShareRequested,
    Sharing
};

enum class AudioSourceKind {
    None,
    System,
    Device,
    Application
};

struct UiPreferences {
    bool settings_visible{true};
    bool borderless{false};
    bool always_on_top{false};

    ConnectionMode connection_mode{ConnectionMode::DirectIpDdNs};
    std::string host;
    std::uint32_t share_port{42800};
    std::uint32_t remote_port{42800};
    bool show_cursor{true};

    AudioSourceKind audio_source{AudioSourceKind::None};
    bool receiver_muted{false};
    int receiver_volume{100};
};

struct UiState {
    UiPreferences preferences;

    bool fullscreen{false};
    bool diagnostics_visible{false};

    ShareUiState share_state{ShareUiState::NotSharing};
    ConnectionUiState connection_state{
        ConnectionUiState::Disconnected};

    std::uint32_t windows_build{0};
    bool application_audio_supported{false};
};

bool SupportsApplicationAudio(std::uint32_t windows_build);

UiState MakeDefaultUiState(std::uint32_t windows_build);

UiState MakeStateFromPreferences(
    UiPreferences preferences,
    std::uint32_t windows_build);

UiPreferences PersistentPreferencesFromState(
    const UiState& state);

void ToggleSettingsVisible(UiState& state);
void ToggleAlwaysOnTop(UiState& state);
void ToggleBorderless(UiState& state);
void ToggleFullscreen(UiState& state);
void SetFullscreen(UiState& state, bool fullscreen);
void ToggleDiagnosticsVisible(UiState& state);
void ToggleShareRequest(UiState& state);
void ToggleConnectionRequest(UiState& state);
void ToggleShowCursor(UiState& state);
void ToggleReceiverMute(UiState& state);
void SetReceiverVolume(UiState& state, int volume);

bool CanSelectAudioSource(
    const UiState& state,
    AudioSourceKind source);

bool SelectAudioSource(
    UiState& state,
    AudioSourceKind source);

}  // namespace fps::ui
