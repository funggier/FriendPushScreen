#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <vector>

namespace fps::platform::windows {

std::vector<diagnostics::GraphicsAdapterInfo> ProbeDxgiAdapters();

}  // namespace fps::platform::windows
