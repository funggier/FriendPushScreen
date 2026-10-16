#pragma once

#include "media/video_codec_model.h"
#include "audio/wasapi_audio.h"

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
    Listening,
    Error,
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

struct VideoPreferences {
    media::VideoBackendPolicy encoder_mode{media::VideoBackendPolicy::PreferHardware};
    media::VideoBackendPolicy decoder_mode{media::VideoBackendPolicy::PreferHardware};
    std::string encoder_id;
    std::string decoder_id;
    std::uint32_t bitrate_mbps{20};
    std::uint32_t keyframe_interval{120};
    bool low_latency{true};
    bool operator==(const VideoPreferences&) const = default;
};

bool IsValidVideoPreferences(const VideoPreferences& preferences);
media::VideoCodecConfig MakeVideoCodecConfig(
    const VideoPreferences& preferences, std::uint32_t width, std::uint32_t height);

struct UiPreferences {
    VideoPreferences video;
    bool settings_visible{true};
    bool borderless{false};
    bool always_on_top{false};

    ConnectionMode connection_mode{ConnectionMode::DirectIpDdNs};
    std::string host;
    std::uint32_t share_port{42800};
    std::uint32_t remote_port{42800};
    bool show_cursor{true};

    AudioSourceKind audio_source{AudioSourceKind::None};
    std::string audio_source_id,receiver_output_id;
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

    bool receiving_video{false},transport_active{false};
    std::string peer_status,share_status,own_fingerprint,peer_fingerprint;
    std::uint32_t windows_build{0};
    bool application_audio_supported{false};
    std::vector<audio::AudioSourceDescriptor> audio_sources;
    std::vector<audio::AudioEndpoint> audio_outputs;
    std::string audio_status;
    bool audio_sending{},audio_receiving{},audio_feedback_blocked{};
};

struct PeerUiSnapshot {
    bool active{},connecting{},connected{},share_intent{},sharing{},receiving{};
    std::string status,share_status,own_fingerprint,peer_fingerprint;
};
void ApplyPeerUiSnapshot(UiState&,const PeerUiSnapshot&);

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
