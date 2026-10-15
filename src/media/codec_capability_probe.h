#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <string>
#include <vector>

namespace fps::media {

diagnostics::CodecCapabilityInfo NormalizeCodecCapability(
    diagnostics::CodecKind codec,
    diagnostics::CodecDirection direction,
    std::string name,
    bool hardware_evidence,
    bool software_evidence);

std::vector<diagnostics::CodecCapabilityInfo> ProbeCodecCapabilities();

}  // namespace fps::media
