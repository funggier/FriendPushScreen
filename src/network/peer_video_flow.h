#pragma once
#include "network/peer_protocol.h"
#include <map>
#include <algorithm>
namespace fps::network {
// Return a slot only when the peer has decoded or discarded that access unit.
// The shared credit also remains with MsQuic until its own send completes.
class PeerVideoSendWindow {
public:
    void Reset(std::uint64_t session,std::uint64_t generation) {
        pending_.clear();session_=session;generation_=generation;last_sequence_=0;expired_=false;
    }
    bool CanSubmit()const{return session_&&generation_&&!expired_&&pending_.size()<kMaxMediaInFlight;}
    bool Submit(std::uint64_t sequence,std::uint64_t now,std::shared_ptr<void> credit) {
        if(!CanSubmit()||sequence<=last_sequence_)return false;
        pending_.emplace(sequence,PendingUnit{now,std::move(credit)});last_sequence_=sequence;return true;
    }
    bool Acknowledge(const ControlMessage& ack) {
        if(ack.type!=ControlType::VideoConsumed||ack.session!=session_||ack.generation!=generation_)return false;
        return pending_.erase(ack.value)!=0;
    }
    // A timeout is a recovery signal, never permission to reuse remote capacity.
    bool Expire(std::uint64_t now) {
        if(expired_)return false;
        if(OldestAge(now)<250)return false;
        expired_=true;return true;
    }
    std::uint64_t OldestAge(std::uint64_t now)const {
        std::uint64_t oldest=0;
        for(const auto& [sequence,unit]:pending_) {
            (void)sequence;if(now>=unit.sent)oldest=std::max(oldest,now-unit.sent);
        }
        return oldest;
    }
    std::size_t Pending()const{return pending_.size();}
private:
    struct PendingUnit {std::uint64_t sent{};std::shared_ptr<void> credit;};
    std::uint64_t session_{},generation_{},last_sequence_{};
    bool expired_{};
    std::map<std::uint64_t,PendingUnit> pending_;
};
}
