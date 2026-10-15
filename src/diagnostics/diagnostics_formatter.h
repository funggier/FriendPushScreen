#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <filesystem>
#include <string>

namespace fps::diagnostics {

std::string ProbeStateToString(ProbeState state);
std::string FormatDiagnosticsText(
    const DiagnosticsSnapshot& snapshot,
    bool include_window_titles = false);

bool SaveDiagnosticsUtf8(
    const DiagnosticsSnapshot& snapshot,
    const std::filesystem::path& path);

}  // namespace fps::diagnostics
