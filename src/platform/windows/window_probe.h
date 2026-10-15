#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>
#include <vector>

namespace fps::platform::windows {

struct WindowCandidate {
    HWND handle{nullptr};
    HWND owner{nullptr};
    bool visible{false};
    std::string title;
    bool tool_window{false};
};

bool ShouldIncludeWindow(const WindowCandidate& candidate, HWND self_root);
std::vector<diagnostics::WindowInfo> ProbeVisibleTopLevelWindows(HWND self_root);

}  // namespace fps::platform::windows
