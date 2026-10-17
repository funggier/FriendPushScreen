#include "test_support.h"

#include "ui/settings_store.h"

#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

void RunSettingsStoreTests() {
    using namespace fps::ui;

    {
        std::cout<<"SettingsSaveDoesNotBlockUiAndFlushesLatest\n";
        std::atomic<DWORD> writerThread{};std::atomic<int> finalVolume{-1};
        AsyncPreferencesWriter writer([&](const UiPreferences& p,const std::filesystem::path&){writerThread=GetCurrentThreadId();Sleep(100);finalVolume=p.receiver_volume;return true;});
        UiPreferences p;p.receiver_volume=100;const auto start=GetTickCount64();
        writer.Submit(p,"unused-test-path");
        FPS_EXPECT_TRUE(GetTickCount64()-start<50);
        p.receiver_volume=50;writer.Submit(p,"unused-test-path");
        p.receiver_volume=0;writer.Submit(p,"unused-test-path");
        FPS_EXPECT_TRUE(writer.Flush());FPS_EXPECT_EQ(finalVolume.load(),0);
        FPS_EXPECT_TRUE(writerThread.load()!=GetCurrentThreadId());
    }
    {
        std::cout << "StatusStripPreferenceRoundTripAndLegacyDefaults\n";
        const auto hidden=ParsePreferences("status_bar_visible=false\n");
        FPS_EXPECT_TRUE(SerializePreferences(hidden).find("status_bar_visible=false\n")!=std::string::npos);
        FPS_EXPECT_TRUE(SerializePreferences(ParsePreferences("version=1\n")).find("status_bar_visible=true\n")!=std::string::npos);
        FPS_EXPECT_TRUE(SerializePreferences(ParsePreferences("status_bar_visible=maybe\n")).find("status_bar_visible=true\n")!=std::string::npos);
    }
    UiPreferences preferences{};
    preferences.settings_visible = false;
    preferences.borderless = true;
    preferences.always_on_top = true;
    preferences.connection_mode =
        ConnectionMode::DirectIpDdNs;
    preferences.host = "friend.example.net";
    preferences.share_port = 42801;
    preferences.remote_port = 43123;
    preferences.show_cursor = false;
    preferences.audio_source =
        AudioSourceKind::System;
    preferences.receiver_muted = true;
    preferences.receiver_volume = 42;

    const std::string text =
        SerializePreferences(preferences);

    FPS_EXPECT_TRUE(
        text.find("version=1") != std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("connection_mode=direct_ip_ddns") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("host=friend.example.net") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("share_port=42801") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("remote_port=43123") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("show_cursor=false") !=
        std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("share_state=") == std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("sharing=") == std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("connected") == std::string::npos);
    FPS_EXPECT_TRUE(
        text.find("fullscreen") == std::string::npos);

    const UiPreferences parsed =
        ParsePreferences(text);

    FPS_EXPECT_EQ(
        parsed.settings_visible,
        preferences.settings_visible);
    FPS_EXPECT_EQ(
        parsed.borderless,
        preferences.borderless);
    FPS_EXPECT_EQ(
        parsed.always_on_top,
        preferences.always_on_top);
    FPS_EXPECT_EQ(
        parsed.connection_mode,
        preferences.connection_mode);
    FPS_EXPECT_EQ(
        parsed.host,
        preferences.host);
    FPS_EXPECT_EQ(
        parsed.share_port,
        preferences.share_port);
    FPS_EXPECT_EQ(
        parsed.remote_port,
        preferences.remote_port);
    FPS_EXPECT_EQ(
        parsed.show_cursor,
        preferences.show_cursor);
    FPS_EXPECT_EQ(
        parsed.audio_source,
        preferences.audio_source);
    FPS_EXPECT_EQ(
        parsed.receiver_muted,
        preferences.receiver_muted);
    FPS_EXPECT_EQ(
        parsed.receiver_volume,
        preferences.receiver_volume);

    {
        const UiPreferences corrupt = ParsePreferences(
            "version=1\n"
            "settings_visible=maybe\n"
            "connection_mode=unknown\n"
            "share_port=0\n"
            "remote_port=999999\n"
            "receiver_volume=-200\n"
            "garbage=noise\n");

        FPS_EXPECT_TRUE(corrupt.settings_visible);
        FPS_EXPECT_EQ(
            corrupt.connection_mode,
            ConnectionMode::DirectIpDdNs);
        FPS_EXPECT_EQ(corrupt.share_port, 42800u);
        FPS_EXPECT_EQ(corrupt.remote_port, 42800u);
        FPS_EXPECT_EQ(corrupt.receiver_volume, 100);
    }

    {
        UiPreferences hostile{};
        hostile.host =
            "friend.example.net\n"
            "always_on_top=true\r\n"
            "receiver_volume=1";

        const std::string safe =
            SerializePreferences(hostile);

        FPS_EXPECT_TRUE(
            safe.find("\nalways_on_top=true\n") ==
            std::string::npos);
        FPS_EXPECT_TRUE(
            safe.find("\nreceiver_volume=1\n") ==
            std::string::npos);
    }

    {
        const auto path =
            std::filesystem::temp_directory_path() /
            "friendpushscreen-settings-store-test.ini";

        std::error_code error;
        std::filesystem::remove(path, error);

        FPS_EXPECT_TRUE(
            SavePreferencesUtf8(
                preferences,
                path));

        const UiPreferences loaded =
            LoadPreferences(
                path,
                UiPreferences{});

        FPS_EXPECT_EQ(
            loaded.connection_mode,
            ConnectionMode::DirectIpDdNs);
        FPS_EXPECT_EQ(
            loaded.host,
            std::string{"friend.example.net"});
        FPS_EXPECT_EQ(loaded.share_port, 42801u);
        FPS_EXPECT_EQ(loaded.remote_port, 43123u);
        FPS_EXPECT_TRUE(!loaded.show_cursor);
        FPS_EXPECT_EQ(loaded.receiver_volume, 42);

        std::ifstream input{path, std::ios::binary};
        const std::string persisted{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        FPS_EXPECT_TRUE(
            persisted.find("version=1") !=
            std::string::npos);

        std::filesystem::remove(path, error);
    }

    {
        UiPreferences fallback{};
        fallback.connection_mode =
            ConnectionMode::Tailscale;
        fallback.share_port = 42877;
        fallback.remote_port = 42888;

        const auto missing =
            std::filesystem::temp_directory_path() /
            "friendpushscreen-settings-store-missing.ini";

        std::error_code error;
        std::filesystem::remove(missing, error);

        const UiPreferences loaded =
            LoadPreferences(missing, fallback);

        FPS_EXPECT_EQ(
            loaded.connection_mode,
            ConnectionMode::Tailscale);
        FPS_EXPECT_EQ(loaded.share_port, 42877u);
        FPS_EXPECT_EQ(loaded.remote_port, 42888u);
    }

    {
        const UiPreferences legacy =
            ParsePreferences(
                "version=1\n"
                "port=42999\n");

        FPS_EXPECT_EQ(
            legacy.remote_port,
            42999u);
        FPS_EXPECT_EQ(
            legacy.share_port,
            42800u);
    }

    {
        const UiPreferences legacy_auto =
            ParsePreferences(
                "version=1\n"
                "connection_mode=auto\n");

        FPS_EXPECT_EQ(
            legacy_auto.connection_mode,
            ConnectionMode::DirectIpDdNs);

        const std::string migrated =
            SerializePreferences(legacy_auto);

        FPS_EXPECT_TRUE(
            migrated.find(
                "connection_mode=auto") ==
            std::string::npos);
        FPS_EXPECT_TRUE(
            migrated.find(
                "connection_mode=direct_ip_ddns") !=
            std::string::npos);
    }


    // Codec choices must persist independently, including mixed CPU/GPU paths.
    for (const auto* encoder_mode : {"gpu", "cpu"}) {
        for (const auto* decoder_mode : {"gpu", "cpu"}) {
            const std::string requested =
                std::string{"video_encoder_mode="} + encoder_mode + "\n" +
                "video_decoder_mode=" + decoder_mode + "\n" +
                "video_encoder_id=encoder-registration\n"
                "video_decoder_id=decoder-registration\n"
                "video_bitrate_mbps=12\n"
                "video_keyframe_interval=90\n"
                "video_low_latency=false\n";
            const auto saved = SerializePreferences(ParsePreferences(requested));
            for (const auto& line : {
                    std::string{"video_encoder_mode="} + encoder_mode + "\n",
                    std::string{"video_decoder_mode="} + decoder_mode + "\n",
                    std::string{"video_encoder_id=encoder-registration\n"},
                    std::string{"video_decoder_id=decoder-registration\n"},
                    std::string{"video_bitrate_mbps=12\n"},
                    std::string{"video_keyframe_interval=90\n"},
                    std::string{"video_low_latency=false\n"}}) {
                FPS_EXPECT_TRUE(saved.find(line) != std::string::npos);
            }
        }
    }
    {
        const auto saved = SerializePreferences(ParsePreferences(
            "video_encoder_mode=not-a-backend\n"
            "video_decoder_mode=not-a-backend\n"
            "video_bitrate_mbps=0\n"
            "video_keyframe_interval=999999\n"
            "video_low_latency=maybe\n"));
        FPS_EXPECT_TRUE(saved.find("video_encoder_mode=prefer_gpu\n") != std::string::npos);
        FPS_EXPECT_TRUE(saved.find("video_decoder_mode=prefer_gpu\n") != std::string::npos);
        FPS_EXPECT_TRUE(saved.find("video_bitrate_mbps=20\n") != std::string::npos);
        FPS_EXPECT_TRUE(saved.find("video_keyframe_interval=120\n") != std::string::npos);
        FPS_EXPECT_TRUE(saved.find("video_low_latency=true\n") != std::string::npos);
    }

    FPS_EXPECT_TRUE(
        !DefaultSettingsPath().empty());
}
