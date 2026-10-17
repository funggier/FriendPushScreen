#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace fps::network {
struct PeerHost {
    bool is_ipv4{};
    std::array<std::uint8_t,4> ipv4{};
    std::string dns_name;
};
// Pure syntax/destination validation; no network or namespace-provider call.
bool ParsePeerHost(std::string_view host,PeerHost& result,std::string& error);
}
