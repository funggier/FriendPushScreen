#include "network/peer_session.h"
#include <algorithm>
#include <utility>
namespace fps::network {
PeerSession::PeerSession(std::string own,std::string peer,std::uint64_t boot):
    own_(std::move(own)),peer_(std::move(peer)),boot_(boot?boot:1){}
void PeerSession::Emit(SessionActionKind kind,std::uint64_t id,const ControlMessage& message,std::string detail) {
    if(actions_.size()<512)actions_.push_back({kind,id,message,std::move(detail)});
}
void PeerSession::Send(std::uint64_t id,ControlType type,std::uint64_t generation,std::uint64_t value) {
    ControlMessage m;m.type=type;m.session=state_.token;m.generation=generation;m.value=value;
    if(type==ControlType::VideoFormat)m.format=desired_;
    Emit(SessionActionKind::SendControl,id,m);
}
void PeerSession::OnConnected(std::uint64_t id,bool initiator,std::uint64_t now,std::uint64_t nonce) {
    if(!id||links_.contains(id)||links_.size()>=4){Emit(SessionActionKind::CloseConnection,id);return;}
    auto& link=links_[id];link.initiator=initiator;link.nonce=initiator?nonce:0;
    link.started=link.last_seen=link.last_ping=now;
    if(initiator) {
        if(!nonce){FailLink(id,"Missing session nonce");return;}
        ControlMessage hello;hello.session=boot_;hello.value=nonce;
        Emit(SessionActionKind::SendControl,id,hello);
    }
}
bool PeerSession::FailLink(std::uint64_t id,const char* detail) {
    auto found=links_.find(id);
    if(found!=links_.end()&&!found->second.closing){found->second.closing=true;Emit(SessionActionKind::CloseConnection,id);}
    Emit(SessionActionKind::Error,id,{},detail);
    if(state_.connection==id)ClearActive();
    return false;
}
void PeerSession::ClearActive() {
    const auto id=state_.connection;
    if(state_.sharing)Emit(SessionActionKind::StopSender,id);
    if(state_.receiving)Emit(SessionActionKind::StopReceiver,id);
    awaiting_ready_=false;state_={};last_sequence_=pending_generation_=stopped_generation_=0;
}
void PeerSession::Activate(std::uint64_t id) {
    auto& next=links_.at(id);
    if(state_.connection==id)return;
    if(state_.connected) {
        auto& previous=links_.at(state_.connection);
        const auto nextRank=std::pair{next.initiator?own_:peer_,next.nonce};
        const auto oldRank=std::pair{previous.initiator?own_:peer_,previous.nonce};
        if(nextRank>=oldRank){next.closing=true;Emit(SessionActionKind::CloseConnection,id);return;}
        previous.closing=true;Emit(SessionActionKind::CloseConnection,state_.connection);ClearActive();
    }
    state_.connected=true;state_.connection=id;state_.token=next.nonce;retry_=0;
    if(share_intent_)BeginShare();
}
void PeerSession::BeginShare() {
    if(!state_.connected||!share_intent_)return;
    if(state_.sharing)Emit(SessionActionKind::StopSender,state_.connection);
    if(++generation_==0)++generation_;
    state_.sharing=false;awaiting_ready_=true;state_.outgoing_generation=generation_;state_.outgoing_format=desired_;
    Send(state_.connection,ControlType::ShareStart,generation_);
    Send(state_.connection,ControlType::VideoFormat,generation_);

}
bool PeerSession::OnControl(std::uint64_t id,const ControlMessage& message,std::uint64_t now) {
    auto found=links_.find(id);if(found==links_.end()||found->second.closing)return false;
    auto& link=found->second;
    if(message.type==ControlType::Hello) {
        if(!message.session||!message.value||!(message.capabilities&kCapabilityH264Decode)||(message.capabilities&~3U)||
            (link.initiator&&link.nonce!=message.value))return FailLink(id,"Invalid peer capabilities or session nonce");
        if(link.ready)return link.peer_boot==message.session&&link.nonce==message.value;
        link.nonce=message.value;link.peer_boot=message.session;link.ready=true;link.last_seen=now;
        if(!link.initiator){ControlMessage reply;reply.session=boot_;reply.value=link.nonce;Emit(SessionActionKind::SendControl,id,reply);}
        Activate(id);return true;
    }
    if(!link.ready)return FailLink(id,"Control before capability handshake");
    if(id!=state_.connection||message.session!=state_.token)return true;
    link.last_seen=now;
    switch(message.type) {
    case ControlType::VideoConsumed:break; // Consumed credits belong to the active controller send window.
    case ControlType::VideoReady:
        if(awaiting_ready_&&message.generation==state_.outgoing_generation) {
            awaiting_ready_=false;state_.sharing=true;
            ControlMessage start;start.session=state_.token;start.generation=state_.outgoing_generation;start.format=desired_;
            Emit(SessionActionKind::StartSender,id,start);
        }
        break;
    case ControlType::ShareStart:
        if(!message.generation)return FailLink(id,"Missing stream generation");
        if(message.generation<=std::max({stopped_generation_,state_.incoming_generation,pending_generation_}))return true;
        if(state_.receiving)Emit(SessionActionKind::StopReceiver,id);
        stopped_generation_=std::max(stopped_generation_,state_.incoming_generation);
        state_.receiving=false;state_.incoming_generation=0;last_sequence_=0;pending_generation_=message.generation;break;
    case ControlType::VideoFormat:
        if(message.generation==state_.incoming_generation&&state_.receiving) {
            if(message.format!=state_.incoming_format)return FailLink(id,"Format changed without a new generation");
            return true;
        }
        if(message.generation!=pending_generation_||message.generation<=stopped_generation_)return true;
        if(!ValidateVideoFormat(message.format))return FailLink(id,"Unsupported peer video format");
        state_.incoming_format=message.format;state_.incoming_generation=message.generation;
        state_.receiving=true;pending_generation_=0;last_sequence_=0;
        Emit(SessionActionKind::StartReceiver,id,message);break;
    case ControlType::ShareStop:
        if(!message.generation||message.generation<std::max(state_.incoming_generation,pending_generation_))return true;
        stopped_generation_=std::max(stopped_generation_,message.generation);pending_generation_=0;
        if(state_.receiving)Emit(SessionActionKind::StopReceiver,id);
        state_.receiving=false;state_.incoming_generation=0;last_sequence_=0;break;
    case ControlType::KeyframeRequest:
        if(state_.sharing&&message.generation==state_.outgoing_generation)BeginShare();
        break;
    case ControlType::Ping:Send(id,ControlType::Pong,0,message.value);break;
    case ControlType::Pong:break;
    case ControlType::Goodbye:Emit(SessionActionKind::CloseConnection,id);link.closing=true;ClearActive();break;
    case ControlType::Error:Emit(SessionActionKind::Error,id,{},message.text);break;
    default:return FailLink(id,"Unsupported control message");
    }
    return true;
}
bool PeerSession::AcceptVideo(std::uint64_t id,const VideoWireFrame& frame,std::uint64_t now) {
    if(!state_.connected||!state_.receiving||id!=state_.connection||frame.session!=state_.token||
        frame.stream!=1||frame.generation!=state_.incoming_generation||frame.format!=state_.incoming_format||
        !frame.sequence||frame.sequence<=last_sequence_)return false;
    last_sequence_=frame.sequence;links_.at(id).last_seen=now;return true;
}
void PeerSession::ReceiverReady(std::uint64_t generation) {
    if(state_.connected&&state_.receiving&&generation==state_.incoming_generation)
        Send(state_.connection,ControlType::VideoReady,generation);
}
void PeerSession::OnMediaLost(std::uint64_t id) {
    if(!state_.connected||id!=state_.connection)return;
    if(state_.receiving){Emit(SessionActionKind::RecoverReceiver,id);Send(id,ControlType::KeyframeRequest,state_.incoming_generation);}
}
bool PeerSession::RequestShare(VideoFormat format) {
    if(!ValidateVideoFormat(format))return false;
    if(share_intent_&&desired_==format&&(state_.sharing||awaiting_ready_))return true;
    desired_=format;share_intent_=true;BeginShare();return true;
}
void PeerSession::StopShare() {
    share_intent_=false;
    if(state_.sharing||awaiting_ready_) {
        awaiting_ready_=false;
        Send(state_.connection,ControlType::ShareStop,state_.outgoing_generation);
        Emit(SessionActionKind::StopSender,state_.connection);state_.sharing=false;
    }
}
void PeerSession::SetConnectionIntent(bool intent,std::uint64_t now) {
    connect_intent_=intent;retry_=0;next_retry_=now+4000;
}
void PeerSession::OnDisconnected(std::uint64_t id,std::uint64_t now) {
    links_.erase(id);
    if(state_.connection==id)ClearActive();
    if(!state_.connected&&connect_intent_)next_retry_=now+std::min<std::uint64_t>(250ULL<<std::min(retry_,5U),8000);
}
void PeerSession::Disconnect() {
    connect_intent_=false;share_intent_=false;
    if(state_.connected)Send(state_.connection,ControlType::Goodbye);
    for(auto& [id,link]:links_)if(!link.closing){link.closing=true;Emit(SessionActionKind::CloseConnection,id);}
    ClearActive();links_.clear();
}
void PeerSession::Tick(std::uint64_t now) {
    for(auto& [id,link]:links_) {
        if(link.closing)continue;
        if(!link.ready&&now-link.started>3000){FailLink(id,"Capability handshake timed out");continue;}
        if(id==state_.connection&&link.ready) {
            if(now-link.last_seen>3500){link.closing=true;Emit(SessionActionKind::CloseConnection,id);ClearActive();next_retry_=now+250;}
            else if(now-link.last_ping>=1000){Send(id,ControlType::Ping,0,now);link.last_ping=now;}
        }
    }
    const bool pending=std::any_of(links_.begin(),links_.end(),[](auto& v){return !v.second.closing;});
    if(!state_.connected&&!pending&&connect_intent_&&now>=next_retry_) {
        if(retry_>=8){connect_intent_=false;Emit(SessionActionKind::Error,0,{},"Peer reconnect limit reached; press Connect to retry");}
        else {Emit(SessionActionKind::Reconnect,0);next_retry_=now+std::min<std::uint64_t>(250ULL<<std::min(retry_++,5U),8000);}
    }
}
std::vector<SessionAction> PeerSession::TakeActions(){auto out=std::move(actions_);actions_.clear();return out;}
}
