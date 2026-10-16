#pragma once
#include "network/peer_identity.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>
namespace fps::network {
inline constexpr std::size_t kMaxControlBytes=64*1024;
inline constexpr std::size_t kMaxMediaBytes=8*1024*1024;
inline constexpr std::size_t kMaxMediaInFlight=2;
enum class TransportEventKind { Connected,Disconnected,Control,Video,MediaLost,Error };
struct TransportEvent {
    TransportEventKind kind{TransportEventKind::Error};
    std::uint64_t connection_id{0};
    std::vector<std::uint8_t> bytes;
    std::string detail;
};
struct TransportMetrics {
    std::uint64_t sent_control{0},received_control{0},sent_video{0},received_video{0};
    std::uint64_t sent_bytes{0},received_bytes{0},rejected_certificates{0},dropped_video{0};
    std::uint64_t send_media_bytes{0},receive_media_bytes{0},max_send_media_bytes{0},max_receive_media_bytes{0};
    std::uint32_t send_media_count{0},receive_media_count{0},max_send_media_count{0},max_receive_media_count{0};
};
enum class TransportSendResult { Sent,Backpressure,NotConnected,Invalid };
struct TransportConfig {
    std::filesystem::path data_directory;
    std::uint16_t listen_port{0};
    bool loopback_only{true};
    std::uint32_t media_deadline_ms{250};
};
class SecurePeerTransport {
public:
    SecurePeerTransport();
    ~SecurePeerTransport();
    SecurePeerTransport(const SecurePeerTransport&)=delete;
    SecurePeerTransport& operator=(const SecurePeerTransport&)=delete;
    bool Start(const PeerIdentity&,const PublicPeerIdentity&,TransportConfig,std::string& error);
    bool Connect(const std::string& host,std::uint16_t port,std::string& error);
    TransportSendResult SendControl(std::span<const std::uint8_t>,std::uint64_t connection_id=0);
    TransportSendResult SendVideo(std::vector<std::uint8_t>,std::uint64_t connection_id=0);
    std::vector<TransportEvent> PollEvents();
    TransportMetrics Metrics()const;
    std::uint16_t ListenPort()const;
    std::string RuntimeInfo()const;
    void Disconnect(std::uint64_t connection_id=0);
    void Stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
