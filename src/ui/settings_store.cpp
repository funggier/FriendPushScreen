#include "ui/settings_store.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>

namespace fps::ui {
namespace {

std::string BoolText(bool value) {
    return value ? "true" : "false";
}

const char* ConnectionModeText(
    ConnectionMode mode) {
    switch (mode) {
    case ConnectionMode::Auto:
        return "auto";
    case ConnectionMode::Lan:
        return "lan";
    case ConnectionMode::DirectIpDdNs:
        return "direct_ip_ddns";
    case ConnectionMode::Tailscale:
        return "tailscale";
    }
    return "auto";
}

const char* AudioSourceText(
    AudioSourceKind source) {
    switch (source) {
    case AudioSourceKind::None:
        return "none";
    case AudioSourceKind::System:
        return "system";
    case AudioSourceKind::Device:
        return "device";
    case AudioSourceKind::Application:
        return "application";
    }
    return "none";
}

std::string SanitizeSingleLine(
    std::string value) {
    std::string safe;
    safe.reserve(value.size());

    for (const char ch : value) {
        if (ch == '\r' || ch == '\n') {
            continue;
        }
        safe.push_back(ch);
    }

    return safe;
}

bool ParseBool(
    std::string_view value,
    bool& output) {
    if (value == "true") {
        output = true;
        return true;
    }
    if (value == "false") {
        output = false;
        return true;
    }
    return false;
}

bool ParseUnsigned(
    std::string_view value,
    std::uint32_t& output) {
    if (value.empty()) {
        return false;
    }

    std::uint32_t parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);

    if (result.ec != std::errc{} ||
        result.ptr != value.data() + value.size()) {
        return false;
    }

    output = parsed;
    return true;
}

bool ParseInteger(
    std::string_view value,
    int& output) {
    if (value.empty()) {
        return false;
    }

    int parsed = 0;
    const auto result = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);

    if (result.ec != std::errc{} ||
        result.ptr != value.data() + value.size()) {
        return false;
    }

    output = parsed;
    return true;
}

bool ParseConnectionMode(
    std::string_view value,
    ConnectionMode& output) {
    if (value == "auto") {
        output = ConnectionMode::Auto;
        return true;
    }
    if (value == "lan") {
        output = ConnectionMode::Lan;
        return true;
    }
    if (value == "direct_ip_ddns") {
        output = ConnectionMode::DirectIpDdNs;
        return true;
    }
    if (value == "tailscale") {
        output = ConnectionMode::Tailscale;
        return true;
    }
    return false;
}

bool ParseAudioSource(
    std::string_view value,
    AudioSourceKind& output) {
    if (value == "none") {
        output = AudioSourceKind::None;
        return true;
    }
    if (value == "system") {
        output = AudioSourceKind::System;
        return true;
    }
    if (value == "device") {
        output = AudioSourceKind::Device;
        return true;
    }
    if (value == "application") {
        output = AudioSourceKind::Application;
        return true;
    }
    return false;
}

std::string_view TrimTrailingCarriageReturn(
    std::string_view value) {
    if (!value.empty() &&
        value.back() == '\r') {
        value.remove_suffix(1);
    }
    return value;
}

}  // namespace

std::string SerializePreferences(
    const UiPreferences& preferences) {
    std::ostringstream out;

    out << "version=1\n";
    out << "settings_visible="
        << BoolText(preferences.settings_visible)
        << "\n";
    out << "borderless="
        << BoolText(preferences.borderless)
        << "\n";
    out << "always_on_top="
        << BoolText(preferences.always_on_top)
        << "\n";
    out << "connection_mode="
        << ConnectionModeText(
               preferences.connection_mode)
        << "\n";
    out << "host="
        << SanitizeSingleLine(preferences.host)
        << "\n";
    out << "port="
        << preferences.port
        << "\n";
    out << "audio_source="
        << AudioSourceText(
               preferences.audio_source)
        << "\n";
    out << "receiver_muted="
        << BoolText(preferences.receiver_muted)
        << "\n";
    out << "receiver_volume="
        << preferences.receiver_volume
        << "\n";

    return out.str();
}

UiPreferences ParsePreferences(
    std::string_view text,
    UiPreferences fallback) {
    UiPreferences result = fallback;

    std::size_t offset = 0;
    while (offset <= text.size()) {
        const std::size_t newline =
            text.find('\n', offset);

        std::string_view line =
            newline == std::string_view::npos
                ? text.substr(offset)
                : text.substr(
                      offset,
                      newline - offset);

        line = TrimTrailingCarriageReturn(line);

        if (!line.empty()) {
            const std::size_t separator =
                line.find('=');

            if (separator !=
                std::string_view::npos) {
                const std::string_view key =
                    line.substr(0, separator);
                const std::string_view value =
                    line.substr(separator + 1);

                if (key == "settings_visible") {
                    bool parsed = false;
                    if (ParseBool(value, parsed)) {
                        result.settings_visible = parsed;
                    }
                } else if (key == "borderless") {
                    bool parsed = false;
                    if (ParseBool(value, parsed)) {
                        result.borderless = parsed;
                    }
                } else if (key == "always_on_top") {
                    bool parsed = false;
                    if (ParseBool(value, parsed)) {
                        result.always_on_top = parsed;
                    }
                } else if (key == "connection_mode") {
                    ConnectionMode parsed{};
                    if (ParseConnectionMode(
                            value,
                            parsed)) {
                        result.connection_mode = parsed;
                    }
                } else if (key == "host") {
                    result.host =
                        SanitizeSingleLine(
                            std::string{value});
                } else if (key == "port") {
                    std::uint32_t parsed = 0;
                    if (ParseUnsigned(value, parsed) &&
                        parsed >= 1 &&
                        parsed <= 65535) {
                        result.port = parsed;
                    }
                } else if (key == "audio_source") {
                    AudioSourceKind parsed{};
                    if (ParseAudioSource(
                            value,
                            parsed)) {
                        result.audio_source = parsed;
                    }
                } else if (key == "receiver_muted") {
                    bool parsed = false;
                    if (ParseBool(value, parsed)) {
                        result.receiver_muted = parsed;
                    }
                } else if (key == "receiver_volume") {
                    int parsed = 0;
                    if (ParseInteger(value, parsed) &&
                        parsed >= 0 &&
                        parsed <= 100) {
                        result.receiver_volume = parsed;
                    }
                }
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        offset = newline + 1;
    }

    return result;
}

bool SavePreferencesUtf8(
    const UiPreferences& preferences,
    const std::filesystem::path& path) {
    std::error_code error;

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(
            parent,
            error);
        if (error) {
            return false;
        }
    }

    std::filesystem::path temporary = path;
    temporary += L".tmp";

    {
        std::ofstream output{
            temporary,
            std::ios::binary |
                std::ios::trunc};
        if (!output) {
            return false;
        }

        const std::string text =
            SerializePreferences(preferences);
        output.write(
            text.data(),
            static_cast<std::streamsize>(
                text.size()));

        if (!output.good()) {
            return false;
        }
    }

    std::filesystem::remove(path, error);
    error.clear();

    std::filesystem::rename(
        temporary,
        path,
        error);

    if (error) {
        std::filesystem::remove(
            temporary,
            error);
        return false;
    }

    return true;
}

UiPreferences LoadPreferences(
    const std::filesystem::path& path,
    UiPreferences fallback) {
    std::ifstream input{
        path,
        std::ios::binary};
    if (!input) {
        return fallback;
    }

    const std::string text{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};

    if (!input.good() && !input.eof()) {
        return fallback;
    }

    return ParsePreferences(
        text,
        std::move(fallback));
}

std::filesystem::path DefaultSettingsPath() {
    char* local_app_data = nullptr;
    std::size_t size = 0;

    if (_dupenv_s(
            &local_app_data,
            &size,
            "LOCALAPPDATA") != 0 ||
        local_app_data == nullptr ||
        size <= 1) {
        std::free(local_app_data);
        return std::filesystem::path{
            "FriendPushScreen-settings.ini"};
    }

    const std::filesystem::path base{
        local_app_data};
    std::free(local_app_data);

    return base /
        "FriendPushScreen" /
        "settings.ini";
}

}  // namespace fps::ui
