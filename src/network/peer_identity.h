#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace fps::network {
inline constexpr std::size_t kMaxPairingBytes = 64 * 1024;
struct PublicPeerIdentity {
    std::vector<std::uint8_t> certificate_der;
    std::string stable_name;
    std::string fingerprint;
};
std::string Sha256(std::span<const std::uint8_t>);
std::optional<PublicPeerIdentity> ParsePeerBundle(std::span<const std::uint8_t>, std::string& error);
std::vector<std::uint8_t> EncodePeerBundle(const PublicPeerIdentity&);
bool ValidateTrustedPeer(const PublicPeerIdentity& own, const PublicPeerIdentity& peer, std::string& error);
bool WritePublicBundle(const std::filesystem::path&, const PublicPeerIdentity&, std::string& error);
std::optional<PublicPeerIdentity> ReadPublicBundle(const std::filesystem::path&, std::string& error);
bool SaveTrustedPeer(const std::filesystem::path&, const PublicPeerIdentity&, std::string& error);
class PeerIdentity {
public:
    static std::unique_ptr<PeerIdentity> LoadOrCreate(const std::filesystem::path& directory, std::string& error);
    ~PeerIdentity();
    PeerIdentity(const PeerIdentity&) = delete;
    PeerIdentity& operator=(const PeerIdentity&) = delete;
    const PublicPeerIdentity& Public() const { return public_; }
    std::span<const std::uint8_t> PfxBytes() const { return pfx_; }
private:
    PeerIdentity() = default;
    PublicPeerIdentity public_;
    std::vector<std::uint8_t> pfx_;
};
}
