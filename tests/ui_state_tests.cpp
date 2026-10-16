#include "test_support.h"

#include "ui/ui_state.h"

#include <string>

void RunUiStateTests() {
    using namespace fps::ui;

    UiState state = MakeDefaultUiState(19045);

    FPS_EXPECT_TRUE(state.preferences.settings_visible);
    FPS_EXPECT_TRUE(!state.fullscreen);
    FPS_EXPECT_TRUE(!state.preferences.borderless);
    FPS_EXPECT_TRUE(!state.preferences.always_on_top);
    FPS_EXPECT_EQ(
        state.share_state,
        ShareUiState::NotSharing);
    FPS_EXPECT_EQ(
        state.connection_state,
        ConnectionUiState::Disconnected);
    FPS_EXPECT_EQ(
        state.preferences.connection_mode,
        ConnectionMode::DirectIpDdNs);
    FPS_EXPECT_EQ(state.preferences.share_port, 42800u);
    FPS_EXPECT_EQ(state.preferences.remote_port, 42800u);
    FPS_EXPECT_EQ(
        state.preferences.audio_source,
        AudioSourceKind::None);
    FPS_EXPECT_TRUE(!state.preferences.receiver_muted);
    FPS_EXPECT_EQ(state.preferences.receiver_volume, 100);

    {
        VideoPreferences video;
        FPS_EXPECT_TRUE(IsValidVideoPreferences(video));
        video.encoder_mode = fps::media::VideoBackendPolicy::ForceSoftware;
        video.decoder_mode = fps::media::VideoBackendPolicy::RequireHardware;
        video.bitrate_mbps = 12; video.keyframe_interval = 90; video.low_latency = false;
        video.encoder_id = "encoder"; video.decoder_id = "decoder";
        const auto config = MakeVideoCodecConfig(video,1366,768);
        FPS_EXPECT_EQ(config.width,1366U);
        FPS_EXPECT_EQ(config.height,768U);
        FPS_EXPECT_EQ(config.target_bitrate_bps,12'000'000U);
        FPS_EXPECT_EQ(config.keyframe_interval_frames,90U);
        FPS_EXPECT_TRUE(!config.low_latency);
        FPS_EXPECT_EQ(config.encoder_id,std::string{"encoder"});
        FPS_EXPECT_EQ(config.decoder_id,std::string{"decoder"});
        video.bitrate_mbps = 0;
        FPS_EXPECT_TRUE(!IsValidVideoPreferences(video));
        video.bitrate_mbps = 201;
        FPS_EXPECT_TRUE(!IsValidVideoPreferences(video));
        video.bitrate_mbps = 20; video.keyframe_interval = 601;
        FPS_EXPECT_TRUE(!IsValidVideoPreferences(video));
        video.keyframe_interval = 120; video.encoder_id = "bad\nkey=true";
        FPS_EXPECT_TRUE(!IsValidVideoPreferences(video));
    }
    ToggleSettingsVisible(state);
    FPS_EXPECT_TRUE(!state.preferences.settings_visible);
    ToggleSettingsVisible(state);
    FPS_EXPECT_TRUE(state.preferences.settings_visible);

    ToggleAlwaysOnTop(state);
    FPS_EXPECT_TRUE(state.preferences.always_on_top);
    ToggleAlwaysOnTop(state);
    FPS_EXPECT_TRUE(!state.preferences.always_on_top);

    ToggleBorderless(state);
    FPS_EXPECT_TRUE(state.preferences.borderless);
    ToggleBorderless(state);
    FPS_EXPECT_TRUE(!state.preferences.borderless);

    ToggleFullscreen(state);
    FPS_EXPECT_TRUE(state.fullscreen);
    SetFullscreen(state, false);
    FPS_EXPECT_TRUE(!state.fullscreen);

    ToggleDiagnosticsVisible(state);
    FPS_EXPECT_TRUE(state.diagnostics_visible);
    ToggleDiagnosticsVisible(state);
    FPS_EXPECT_TRUE(!state.diagnostics_visible);

    ToggleShareRequest(state);
    FPS_EXPECT_EQ(
        state.share_state,
        ShareUiState::ShareRequested);
    FPS_EXPECT_TRUE(state.share_state != ShareUiState::Sharing);
    ToggleShareRequest(state);
    FPS_EXPECT_EQ(
        state.share_state,
        ShareUiState::NotSharing);

    ToggleConnectionRequest(state);
    FPS_EXPECT_EQ(
        state.connection_state,
        ConnectionUiState::ConnectRequested);
    FPS_EXPECT_TRUE(
        state.connection_state != ConnectionUiState::Connected);
    ToggleConnectionRequest(state);
    FPS_EXPECT_EQ(
        state.connection_state,
        ConnectionUiState::Disconnected);

    SetReceiverVolume(state, -20);
    FPS_EXPECT_EQ(state.preferences.receiver_volume, 0);
    SetReceiverVolume(state, 140);
    FPS_EXPECT_EQ(state.preferences.receiver_volume, 100);
    SetReceiverVolume(state, 37);
    FPS_EXPECT_EQ(state.preferences.receiver_volume, 37);

    ToggleReceiverMute(state);
    FPS_EXPECT_TRUE(state.preferences.receiver_muted);
    ToggleReceiverMute(state);
    FPS_EXPECT_TRUE(!state.preferences.receiver_muted);

    FPS_EXPECT_TRUE(state.preferences.show_cursor);
    ToggleShowCursor(state);
    FPS_EXPECT_TRUE(!state.preferences.show_cursor);
    ToggleShowCursor(state);
    FPS_EXPECT_TRUE(state.preferences.show_cursor);

    FPS_EXPECT_TRUE(!SupportsApplicationAudio(19045));
    FPS_EXPECT_TRUE(!SupportsApplicationAudio(20347));
    FPS_EXPECT_TRUE(SupportsApplicationAudio(20348));
    FPS_EXPECT_TRUE(SupportsApplicationAudio(26100));

    FPS_EXPECT_TRUE(
        !CanSelectAudioSource(
            state,
            AudioSourceKind::Application));

    state = MakeDefaultUiState(20348);
    FPS_EXPECT_TRUE(
        CanSelectAudioSource(
            state,
            AudioSourceKind::Application));
    FPS_EXPECT_TRUE(
        SelectAudioSource(
            state,
            AudioSourceKind::Application));
    FPS_EXPECT_EQ(
        state.preferences.audio_source,
        AudioSourceKind::Application);

    state = MakeDefaultUiState(19045);
    FPS_EXPECT_TRUE(
        !SelectAudioSource(
            state,
            AudioSourceKind::Application));
    FPS_EXPECT_EQ(
        state.preferences.audio_source,
        AudioSourceKind::None);

    state.fullscreen = true;
    const UiPreferences persisted =
        PersistentPreferencesFromState(state);
    FPS_EXPECT_TRUE(
        persisted.settings_visible ==
        state.preferences.settings_visible);
    FPS_EXPECT_TRUE(
        persisted.borderless ==
        state.preferences.borderless);
    FPS_EXPECT_TRUE(
        persisted.always_on_top ==
        state.preferences.always_on_top);

    UiState restored = MakeStateFromPreferences(
        persisted,
        19045);
    FPS_EXPECT_TRUE(!restored.fullscreen);
    FPS_EXPECT_EQ(
        restored.share_state,
        ShareUiState::NotSharing);
    FPS_EXPECT_EQ(
        restored.connection_state,
        ConnectionUiState::Disconnected);
}
