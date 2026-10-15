#include "ui/ui_state.h"

#include <algorithm>
#include <utility>

namespace fps::ui {

bool SupportsApplicationAudio(
    std::uint32_t windows_build) {
    return windows_build >= 20348;
}

UiState MakeDefaultUiState(
    std::uint32_t windows_build) {
    UiState state{};
    state.windows_build = windows_build;
    state.application_audio_supported =
        SupportsApplicationAudio(windows_build);
    return state;
}

UiState MakeStateFromPreferences(
    UiPreferences preferences,
    std::uint32_t windows_build) {
    UiState state =
        MakeDefaultUiState(windows_build);

    preferences.port =
        std::clamp<std::uint32_t>(
            preferences.port,
            1,
            65535);
    preferences.receiver_volume =
        std::clamp(
            preferences.receiver_volume,
            0,
            100);

    if (preferences.audio_source ==
            AudioSourceKind::Application &&
        !state.application_audio_supported) {
        preferences.audio_source =
            AudioSourceKind::None;
    }

    state.preferences = std::move(preferences);

    state.fullscreen = false;
    state.diagnostics_visible = false;
    state.share_state = ShareUiState::NotSharing;
    state.connection_state =
        ConnectionUiState::Disconnected;

    return state;
}

UiPreferences PersistentPreferencesFromState(
    const UiState& state) {
    return state.preferences;
}

void ToggleSettingsVisible(UiState& state) {
    state.preferences.settings_visible =
        !state.preferences.settings_visible;
}

void ToggleShareRequest(UiState& state) {
    switch (state.share_state) {
    case ShareUiState::NotSharing:
        state.share_state =
            ShareUiState::ShareRequested;
        break;

    case ShareUiState::ShareRequested:
    case ShareUiState::Sharing:
        state.share_state =
            ShareUiState::NotSharing;
        break;
    }
}

void ToggleConnectionRequest(UiState& state) {
    switch (state.connection_state) {
    case ConnectionUiState::Disconnected:
        state.connection_state =
            ConnectionUiState::ConnectRequested;
        break;

    case ConnectionUiState::ConnectRequested:
    case ConnectionUiState::Connected:
        state.connection_state =
            ConnectionUiState::Disconnected;
        break;
    }
}

void ToggleReceiverMute(UiState& state) {
    state.preferences.receiver_muted =
        !state.preferences.receiver_muted;
}

void SetReceiverVolume(
    UiState& state,
    int volume) {
    state.preferences.receiver_volume =
        std::clamp(volume, 0, 100);
}

bool CanSelectAudioSource(
    const UiState& state,
    AudioSourceKind source) {
    if (source == AudioSourceKind::Application) {
        return state.application_audio_supported;
    }

    return true;
}

bool SelectAudioSource(
    UiState& state,
    AudioSourceKind source) {
    if (!CanSelectAudioSource(state, source)) {
        return false;
    }

    state.preferences.audio_source = source;
    return true;
}

}  // namespace fps::ui
