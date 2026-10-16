#include "ui/settings_store.h"

#include "platform/windows/runtime_paths.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>

namespace fps::ui {
namespace {

const char* VideoModeText(media::VideoBackendPolicy mode) {
    switch (mode) {
    case media::VideoBackendPolicy::RequireHardware: return "gpu";
    case media::VideoBackendPolicy::ForceSoftware: return "cpu";
    default: return "prefer_gpu";
    }
}
bool ParseVideoMode(std::string_view value, media::VideoBackendPolicy& mode) {
    if (value == "gpu") mode = media::VideoBackendPolicy::RequireHardware;
    else if (value == "cpu") mode = media::VideoBackendPolicy::ForceSoftware;
    else if (value == "prefer_gpu") mode = media::VideoBackendPolicy::PreferHardware;
    else return false;
    return true;
}

std::string BoolText(bool value) {
    return value ? "true" : "false";
}

const char* ConnectionModeText(
    ConnectionMode mode) {
    switch (mode) {
    case ConnectionMode::Lan:
        return "lan";
    case ConnectionMode::DirectIpDdNs:
        return "direct_ip_ddns";
    case ConnectionMode::Tailscale:
        return "tailscale";
    }
    return "direct_ip_ddns";
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
        // Legacy FPS-004 value. Auto mode was removed;
        // migrate it to the explicit Direct IP / DDNS path.
        output = ConnectionMode::DirectIpDdNs;
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
    const auto video = IsValidVideoPreferences(preferences.video) ?
        preferences.video : VideoPreferences{};
    out << "video_encoder_mode=" << VideoModeText(video.encoder_mode) << "\n";
    out << "video_decoder_mode=" << VideoModeText(video.decoder_mode) << "\n";
    out << "video_encoder_id=" << video.encoder_id << "\n";
    out << "video_decoder_id=" << video.decoder_id << "\n";
    out << "video_bitrate_mbps=" << video.bitrate_mbps << "\n";
    out << "video_keyframe_interval=" << video.keyframe_interval << "\n";
    out << "video_low_latency=" << BoolText(video.low_latency) << "\n";
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
    out << "share_port="
        << preferences.share_port
        << "\n";
    out << "remote_port="
        << preferences.remote_port
        << "\n";
    out << "show_cursor="
        << BoolText(preferences.show_cursor)
        << "\n";
    out << "audio_source="
        << AudioSourceText(
               preferences.audio_source)
        << "\n";
    out << "audio_source_id=" << SanitizeSingleLine(preferences.audio_source_id).substr(0,1024) << "\n";
    out << "receiver_output_id=" << SanitizeSingleLine(preferences.receiver_output_id).substr(0,1024) << "\n";
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

                if (key == "video_encoder_mode") {
                    ParseVideoMode(value, result.video.encoder_mode);
                } else if (key == "video_decoder_mode") {
                    ParseVideoMode(value, result.video.decoder_mode);
                } else if (key == "video_encoder_id" || key == "video_decoder_id") {
                    auto video = result.video;
                    auto& id = key == "video_encoder_id" ? video.encoder_id : video.decoder_id;
                    id = std::string{value};
                    if (IsValidVideoPreferences(video)) result.video = std::move(video);
                } else if (key == "video_bitrate_mbps" || key == "video_keyframe_interval") {
                    std::uint32_t parsed = 0;
                    if (ParseUnsigned(value, parsed)) {
                        auto video = result.video;
                        if (key == "video_bitrate_mbps") video.bitrate_mbps = parsed;
                        else video.keyframe_interval = parsed;
                        if (IsValidVideoPreferences(video)) result.video = std::move(video);
                    }
                } else if (key == "video_low_latency") {
                    ParseBool(value, result.video.low_latency);
                } else if (key == "settings_visible") {
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
                } else if (key == "share_port") {
                    std::uint32_t parsed = 0;
                    if (ParseUnsigned(value, parsed) &&
                        parsed >= 1 &&
                        parsed <= 65535) {
                        result.share_port = parsed;
                    }
                } else if (
                    key == "remote_port" ||
                    key == "port") {
                    std::uint32_t parsed = 0;
                    if (ParseUnsigned(value, parsed) &&
                        parsed >= 1 &&
                        parsed <= 65535) {
                        result.remote_port = parsed;
                    }
                } else if (key == "show_cursor") {
                    bool parsed = false;
                    if (ParseBool(value, parsed)) {
                        result.show_cursor = parsed;
                    }
                } else if (key == "audio_source") {
                    AudioSourceKind parsed{};
                    if (ParseAudioSource(
                            value,
                            parsed)) {
                        result.audio_source = parsed;
                    }
                } else if (key == "audio_source_id" && value.size()<=1024) {
                    result.audio_source_id=std::string(value);
                } else if (key == "receiver_output_id" && value.size()<=1024) {
                    result.receiver_output_id=std::string(value);
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
    return platform::windows::
        DefaultDataRoot() /
        "settings.ini";
}

}  // namespace fps::ui
