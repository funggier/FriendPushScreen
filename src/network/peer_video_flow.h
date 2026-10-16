#pragma once
#include "network/peer_protocol.h"
#include <map>
namespace fps::network {
// Return a slot only when the peer has decoded or discarded that access unit.
// The shared credit also remains with MsQuic until its own send completes.
class PeerVideoSendWindow {
public:
    void Reset(std::uint64_t session,std::uint64_t generation) {
        pending_.clear();session_=session;generation_=generation;last_sequence_=0;
    }
    bool CanSubmit()const{return session_&&generation_&&pending_.size()<kMaxMediaInFlight;}
    bool Submit(std::uint64_t sequence,std::uint64_t now,std::shared_ptr<void> credit) {
        if(!CanSubmit()||sequence<=last_sequence_)return false;
        pending_.emplace(sequence,PendingUnit{now,std::move(credit)});last_sequence_=sequence;return true;
    }
    bool Acknowledge(const ControlMessage& ack) {
        if(ack.type!=ControlType::VideoConsumed||ack.session!=session_||ack.generation!=generation_)return false;
        return pending_.erase(ack.value)!=0;
    }
    bool Expire(std::uint64_t now) {
        bool expired=false;
        for(auto it=pending_.begin();it!=pending_.end();) {
            if(now>=it->second.sent&&now-it->second.sent>=250){it=pending_.erase(it);expired=true;}
            else ++it;
        }
        return expired;
    }
    std::size_t Pending()const{return pending_.size();}
private:
    struct PendingUnit {std::uint64_t sent{};std::shared_ptr<void> credit;};
    std::uint64_t session_{},generation_{},last_sequence_{};
    std::map<std::uint64_t,PendingUnit> pending_;
};
}
