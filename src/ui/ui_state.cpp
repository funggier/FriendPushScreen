#include "ui/ui_state.h"

#include <algorithm>
#include <utility>

namespace fps::ui {

void ApplyPeerUiSnapshot(UiState& s,const PeerUiSnapshot& p) {
    s.transport_active=p.active;s.receiving_video=p.receiving;
    s.connection_state=p.connected?ConnectionUiState::Connected:(p.connecting?ConnectionUiState::ConnectRequested:
        (p.active?ConnectionUiState::Listening:ConnectionUiState::Disconnected));
    s.share_state=p.sharing?ShareUiState::Sharing:(p.share_intent?ShareUiState::ShareRequested:ShareUiState::NotSharing);
    s.peer_status=p.status;s.share_status=p.share_status;s.own_fingerprint=p.own_fingerprint;s.peer_fingerprint=p.peer_fingerprint;
}

bool IsValidVideoPreferences(const VideoPreferences& preferences) {
    const auto valid_mode = [](media::VideoBackendPolicy mode) {
        return mode == media::VideoBackendPolicy::PreferHardware ||
            mode == media::VideoBackendPolicy::RequireHardware ||
            mode == media::VideoBackendPolicy::ForceSoftware;
    };
    const auto valid_id = [](const std::string& id) {
        return id.size() <= 16384 &&
            std::none_of(id.begin(), id.end(), [](unsigned char c) { return c < 32 || c == 127; });
    };
    return valid_mode(preferences.encoder_mode) && valid_mode(preferences.decoder_mode) &&
        valid_id(preferences.encoder_id) && valid_id(preferences.decoder_id) &&
        preferences.bitrate_mbps >= 1 && preferences.bitrate_mbps <= 200 &&
        preferences.keyframe_interval >= 1 && preferences.keyframe_interval <= 600;
}

media::VideoCodecConfig MakeVideoCodecConfig(
    const VideoPreferences& preferences, std::uint32_t width, std::uint32_t height) {
    media::VideoCodecConfig config;
    config.width = width; config.height = height;
    config.encoder_id = preferences.encoder_id;
    config.decoder_id = preferences.decoder_id;
    config.target_bitrate_bps = preferences.bitrate_mbps * 1'000'000U;
    config.keyframe_interval_frames = preferences.keyframe_interval;
    config.low_latency = preferences.low_latency;
    return config;
}

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

    preferences.share_port =
        std::clamp<std::uint32_t>(
            preferences.share_port,
            1,
            65535);
    preferences.remote_port =
        std::clamp<std::uint32_t>(
            preferences.remote_port,
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

    if (!IsValidVideoPreferences(preferences.video)) preferences.video = {};
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

void ToggleAlwaysOnTop(UiState& state) {
    state.preferences.always_on_top =
        !state.preferences.always_on_top;
}

void ToggleBorderless(UiState& state) {
    state.preferences.borderless =
        !state.preferences.borderless;
}

void ToggleFullscreen(UiState& state) {
    state.fullscreen = !state.fullscreen;
}

void SetFullscreen(
    UiState& state,
    bool fullscreen) {
    state.fullscreen = fullscreen;
}

void ToggleDiagnosticsVisible(UiState& state) {
    state.diagnostics_visible =
        !state.diagnostics_visible;
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
    case ConnectionUiState::Error:
    case ConnectionUiState::Disconnected:
        state.connection_state =
            ConnectionUiState::ConnectRequested;
        break;

    case ConnectionUiState::ConnectRequested:
    case ConnectionUiState::Listening:
    case ConnectionUiState::Connected:
        state.connection_state =
            ConnectionUiState::Disconnected;
        break;
    }
}

void ToggleShowCursor(UiState& state) {
    state.preferences.show_cursor =
        !state.preferences.show_cursor;
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
