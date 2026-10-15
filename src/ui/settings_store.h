#pragma once

#include "ui/ui_state.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace fps::ui {

std::string SerializePreferences(
    const UiPreferences& preferences);

UiPreferences ParsePreferences(
    std::string_view text,
    UiPreferences fallback = {});

bool SavePreferencesUtf8(
    const UiPreferences& preferences,
    const std::filesystem::path& path);

UiPreferences LoadPreferences(
    const std::filesystem::path& path,
    UiPreferences fallback = {});

std::filesystem::path DefaultSettingsPath();

}  // namespace fps::ui
