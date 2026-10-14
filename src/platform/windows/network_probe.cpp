#include "platform/windows/network_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace fps::platform::windows {
namespace {

std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }

    const int length = static_cast<int>(wcslen(value));
    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        length,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            length,
            output.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }
    return output;
}

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

bool ContainsTailscaleName(const NetworkAdapterCandidate& candidate) {
    const std::string combined =
        LowerAscii(candidate.name + " " + candidate.description);
    return combined.find("tailscale") != std::string::npos;
}

bool IsTailscaleCgnatAddress(const std::string& address) {
    unsigned int a = 0;
    unsigned int b = 0;
    unsigned int c = 0;
    unsigned int d = 0;
    if (sscanf_s(
            address.c_str(),
            "%u.%u.%u.%u",
            &a,
            &b,
            &c,
            &d) != 4) {
        return false;
    }

    return a == 100 && b >= 64 && b <= 127 &&
           c <= 255 && d <= 255;
}

bool HasTailscaleAddress(const NetworkAdapterCandidate& candidate) {
    if (!candidate.tunnel) {
        return false;
    }

    return std::any_of(
        candidate.ipv4_addresses.begin(),
        candidate.ipv4_addresses.end(),
        [](const std::string& address) {
            return IsTailscaleCgnatAddress(address);
        });
}

std::string Ipv4ToString(const SOCKADDR_IN& address) {
    char buffer[INET_ADDRSTRLEN]{};
    const auto& bytes = address.sin_addr.S_un.S_un_b;
    const int written = sprintf_s(
        buffer,
        "%u.%u.%u.%u",
        static_cast<unsigned int>(bytes.s_b1),
        static_cast<unsigned int>(bytes.s_b2),
        static_cast<unsigned int>(bytes.s_b3),
        static_cast<unsigned int>(bytes.s_b4));
    if (written <= 0) {
        return {};
    }
    return buffer;
}

}  // namespace

diagnostics::NetworkInfo AnalyzeNetworkAdapters(
    const std::vector<NetworkAdapterCandidate>& candidates) {
    diagnostics::NetworkInfo info{};
    info.result.name = "network";
    info.result.error_code = 0;
    info.result.state = candidates.empty()
        ? diagnostics::ProbeState::Unavailable
        : diagnostics::ProbeState::Available;

    bool tailscale_present = false;

    for (const auto& candidate : candidates) {
        diagnostics::NetworkAdapterInfo adapter{};
        adapter.name = candidate.name;
        adapter.description = candidate.description;
        adapter.operational = candidate.operational;
        adapter.loopback = candidate.loopback;
        adapter.tunnel = candidate.tunnel;
        adapter.ipv4_addresses = candidate.ipv4_addresses;
        adapter.link_speed_bps = candidate.link_speed_bps;
        info.adapters.push_back(std::move(adapter));

        tailscale_present =
            tailscale_present ||
            ContainsTailscaleName(candidate) ||
            HasTailscaleAddress(candidate);
    }

    info.tailscale = tailscale_present
        ? diagnostics::TailscalePresence::Present
        : diagnostics::TailscalePresence::Absent;
    return info;
}

std::optional<std::size_t> SelectPrimaryPhysicalAdapterIndex(
    const std::vector<diagnostics::NetworkAdapterInfo>& adapters) {
    for (std::size_t index = 0; index < adapters.size(); ++index) {
        const auto& adapter = adapters[index];
        if (adapter.operational && !adapter.loopback && !adapter.tunnel) {
            return index;
        }
    }
    return std::nullopt;
}

diagnostics::NetworkInfo NetworkInfoFromFailure(long error_code) {
    diagnostics::NetworkInfo info{};
    info.tailscale = diagnostics::TailscalePresence::Unknown;
    info.result = diagnostics::ProbeResult{
        "network",
        diagnostics::ProbeState::Error,
        error_code,
        "GetAdaptersAddresses failed"
    };
    return info;
}

diagnostics::NetworkInfo ProbeNetworkState() {
    ULONG buffer_size = 16 * 1024;
    std::vector<unsigned char> buffer(buffer_size);

    auto* addresses =
        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG result = GetAdaptersAddresses(
        AF_UNSPEC,
        GAA_FLAG_INCLUDE_PREFIX,
        nullptr,
        addresses,
        &buffer_size);

    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buffer_size);
        addresses =
            reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result = GetAdaptersAddresses(
            AF_UNSPEC,
            GAA_FLAG_INCLUDE_PREFIX,
            nullptr,
            addresses,
            &buffer_size);
    }

    if (result != NO_ERROR) {
        return NetworkInfoFromFailure(static_cast<long>(result));
    }

    std::vector<NetworkAdapterCandidate> candidates;

    for (auto* current = addresses;
         current != nullptr;
         current = current->Next) {
        NetworkAdapterCandidate candidate{};
        candidate.name = WideToUtf8(current->FriendlyName);
        candidate.description = WideToUtf8(current->Description);
        candidate.operational =
            current->OperStatus == IfOperStatusUp;
        candidate.loopback =
            current->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
        candidate.tunnel =
            current->IfType == IF_TYPE_TUNNEL;
        candidate.link_speed_bps =
            static_cast<std::uint64_t>(current->TransmitLinkSpeed);

        for (auto* unicast = current->FirstUnicastAddress;
             unicast != nullptr;
             unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr == nullptr ||
                unicast->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            const auto* ipv4 = reinterpret_cast<const SOCKADDR_IN*>(
                unicast->Address.lpSockaddr);
            std::string address = Ipv4ToString(*ipv4);
            if (!address.empty()) {
                candidate.ipv4_addresses.push_back(std::move(address));
            }
        }

        candidates.push_back(std::move(candidate));
    }

    return AnalyzeNetworkAdapters(candidates);
}

}  // namespace fps::platform::windows
