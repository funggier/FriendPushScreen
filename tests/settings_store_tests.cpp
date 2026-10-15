#include "test_support.h"

#include "ui/settings_store.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

void RunSettingsStoreTests() {
    using namespace fps::ui;

    UiPreferences preferences{};
    preferences.settings_visible = false;
    preferences.borderless = true;
    preferences.always_on_top = true;
    preferences.connection_mode =
        ConnectionMode::DirectIpDdNs;
    preferences.host = "friend.example.net";
    preferences.share_port = 42801;
    preferences.remote_port = 43123;
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
            ConnectionMode::Auto);
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

    FPS_EXPECT_TRUE(
        !DefaultSettingsPath().empty());
}
