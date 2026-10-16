#pragma once
#include "network/peer_protocol.h"
#include <map>
namespace fps::network {
enum class SessionActionKind {SendControl,CloseConnection,StartSender,StopSender,StartReceiver,StopReceiver,ForceKeyframe,RecoverReceiver,Reconnect,Error};
struct SessionAction {
    SessionActionKind kind{SessionActionKind::Error};
    std::uint64_t connection{};
    ControlMessage message;
    std::string detail;
};
struct PeerSessionState {
    bool connected{},sharing{},receiving{},recovering{};
    std::uint64_t connection{},token{},outgoing_generation{},incoming_generation{};
    VideoFormat outgoing_format,incoming_format;
};
class PeerSession {
public:
    PeerSession(std::string own_fingerprint,std::string peer_fingerprint,std::uint64_t boot_nonce);
    void OnConnected(std::uint64_t id,bool initiator,std::uint64_t now,std::uint64_t nonce);
    void OnDisconnected(std::uint64_t id,std::uint64_t now);
    bool OnControl(std::uint64_t id,const ControlMessage&,std::uint64_t now);
    bool AcceptVideo(std::uint64_t id,const VideoWireFrame&,std::uint64_t now);
    void ReceiverReady(std::uint64_t generation);
    void OnMediaLost(std::uint64_t id);
    void RestartOutgoing();
    void ReceiverRecovered(std::uint64_t generation);
    bool RequestShare(VideoFormat);
    void StopShare();
    void SetConnectionIntent(bool intent,std::uint64_t now);
    void Disconnect();
    void Tick(std::uint64_t now);
    std::vector<SessionAction> TakeActions();
    const PeerSessionState& State()const{return state_;}
private:
    struct Link {bool initiator{},ready{},closing{};std::uint64_t nonce{},peer_boot{},started{},last_seen{},last_ping{};};
    void Emit(SessionActionKind,std::uint64_t,const ControlMessage& = {},std::string={});
    void Send(std::uint64_t,ControlType,std::uint64_t generation=0,std::uint64_t value=0);
    void Activate(std::uint64_t);
    void ClearActive();
    void BeginShare(bool recovery=false);
    bool FailLink(std::uint64_t,const char*);
    std::string own_,peer_;
    std::uint64_t boot_{},generation_{},last_sequence_{},pending_generation_{},stopped_generation_{};
    bool share_intent_{},connect_intent_{},awaiting_ready_{},pending_recovery_{};
    std::uint32_t retry_{};
    std::uint64_t next_retry_{};
    VideoFormat desired_;
    PeerSessionState state_;
    std::map<std::uint64_t,Link> links_;
    std::vector<SessionAction> actions_;
};
}
