#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <string>

namespace fps::diagnostics {

std::string ProbeStateToString(ProbeState state);
std::string FormatDiagnosticsText(
    const DiagnosticsSnapshot& snapshot,
    bool include_window_titles = false);

}  // namespace fps::diagnostics
