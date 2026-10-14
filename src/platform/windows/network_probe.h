#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fps::platform::windows {

struct NetworkAdapterCandidate {
    std::string name;
    std::string description;
    bool operational{false};
    bool loopback{false};
    bool tunnel{false};
    std::vector<std::string> ipv4_addresses;
    std::uint64_t link_speed_bps{0};
};

diagnostics::NetworkInfo AnalyzeNetworkAdapters(
    const std::vector<NetworkAdapterCandidate>& candidates);

std::optional<std::size_t> SelectPrimaryPhysicalAdapterIndex(
    const std::vector<diagnostics::NetworkAdapterInfo>& adapters);

diagnostics::NetworkInfo NetworkInfoFromFailure(long error_code);
diagnostics::NetworkInfo ProbeNetworkState();

}  // namespace fps::platform::windows
