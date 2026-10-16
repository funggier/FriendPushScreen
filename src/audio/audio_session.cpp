#include "audio/audio_session.h"
#include <algorithm>
namespace fps::audio {
namespace {
bool Valid(const AudioControl& m) {
    const auto type=static_cast<unsigned>(m.kind),source=static_cast<unsigned>(m.source),state=static_cast<unsigned>(m.state);
    if(!m.session||type<1||type>5||source>3||state>4)return false;
    if(m.kind==AudioControlKind::Capabilities)return !m.generation&&!source&&!state&&m.endpoint_hash.empty();
    if(!m.generation)return false;
    if(m.kind==AudioControlKind::SourceSelected) {
        if(source<1||source>3||state)return false;
        if(source==3)return m.endpoint_hash.empty();
        return m.endpoint_hash.size()==64&&std::all_of(m.endpoint_hash.begin(),m.endpoint_hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F');});
    }
    return !source&&m.endpoint_hash.empty()&&(m.kind==AudioControlKind::StreamState||!state);
}
void Put(std::vector<std::uint8_t>& b,std::uint64_t n,unsigned length){for(unsigned i=length;i>0;--i)b.push_back(static_cast<std::uint8_t>(n>>((i-1)*8)));}
std::uint64_t Get(std::span<const std::uint8_t> b,unsigned offset,unsigned length){std::uint64_t value=0;for(unsigned i=0;i<length;++i)value=(value<<8)|b[offset+i];return value;}
}
bool IsAudioControl(std::span<const std::uint8_t> b){return b.size()>=4&&b[0]=='F'&&b[1]=='P'&&b[2]=='A'&&b[3]=='C';}
std::vector<std::uint8_t> EncodeAudioControl(const AudioControl& m) {
    if(!Valid(m))return {};
    std::vector<std::uint8_t> b{'F','P','A','C',1,static_cast<std::uint8_t>(m.kind),static_cast<std::uint8_t>(m.source),static_cast<std::uint8_t>(m.state)};
    Put(b,m.session,8);Put(b,m.generation,8);Put(b,48000,4);Put(b,2,2);Put(b,960,2);Put(b,128000,4);
    b.push_back(static_cast<std::uint8_t>(m.endpoint_hash.size()));b.insert(b.end(),3,0);
    b.insert(b.end(),m.endpoint_hash.begin(),m.endpoint_hash.end());return b;
}
std::optional<AudioControl> DecodeAudioControl(std::span<const std::uint8_t> b,std::string& error) {
    error="Invalid audio control record/profile";
    if(b.size()<40||b.size()>104||!IsAudioControl(b)||b[4]!=1||b[37]||b[38]||b[39]||
       b[36]+40ULL!=b.size()||Get(b,24,4)!=48000||Get(b,28,2)!=2||Get(b,30,2)!=960||Get(b,32,4)!=128000)return {};
    AudioControl m;m.kind=static_cast<AudioControlKind>(b[5]);m.source=static_cast<AudioSourceKind>(b[6]);m.state=static_cast<AudioState>(b[7]);
    m.session=Get(b,8,8);m.generation=Get(b,16,8);m.endpoint_hash.assign(b.begin()+40,b.end());
    if(!Valid(m))return {};error.clear();return m;
}
void AudioPeerSession::Bind(std::uint64_t session) {
    if(state_.session==session)return;state_={};state_.session=session;pending_.clear();
    if(session){AudioControl m;m.session=session;pending_.push_back(m);}
}
void AudioPeerSession::SelectSource(AudioSourceKind kind,const std::string& endpoint) {
    if(!state_.session)return;
    state_.outgoing_source=kind;state_.outgoing_ready=false;
    state_.outgoing_generation=++generation_;
    std::erase_if(pending_,[](const auto& m){return m.kind!=AudioControlKind::Capabilities&&m.kind!=AudioControlKind::Ready;});
    if(kind==AudioSourceKind::None){SetOutgoingState(AudioState::Disabled);return;}
    AudioControl source;source.kind=AudioControlKind::SourceSelected;source.session=state_.session;source.generation=generation_;
    source.source=kind;source.endpoint_hash=endpoint;
    if(!Valid(source)){state_.outgoing_source=AudioSourceKind::None;SetOutgoingState(AudioState::Error);return;}
    pending_.push_back(source);
    AudioControl format;format.kind=AudioControlKind::Format;format.session=state_.session;format.generation=generation_;pending_.push_back(format);
    SetOutgoingState(AudioState::Starting);
}
void AudioPeerSession::SetOutgoingState(AudioState state) {
    if(!state_.session||!state_.outgoing_generation)return;
    if(state==AudioState::Disabled||state==AudioState::SourceLost||state==AudioState::Error)state_.outgoing_ready=false;
    std::erase_if(pending_,[&](const auto& m){return m.kind==AudioControlKind::StreamState&&m.generation==state_.outgoing_generation;});
    AudioControl m;m.kind=AudioControlKind::StreamState;m.session=state_.session;m.generation=state_.outgoing_generation;m.state=state;
    if(pending_.size()<16)pending_.push_back(m);
}
bool AudioPeerSession::OnControl(const AudioControl& m) {
    if(!Valid(m)||!state_.session||m.session!=state_.session)return false;
    if(m.kind==AudioControlKind::Capabilities){state_.peer_capable=true;return true;}
    if(!state_.peer_capable)return false;
    if(m.kind==AudioControlKind::Ready) {
        if(m.generation!=state_.outgoing_generation||state_.outgoing_source==AudioSourceKind::None)return false;
        state_.outgoing_ready=true;return true;
    }
    if(m.kind==AudioControlKind::SourceSelected) {
        if(m.generation<=state_.selected_generation)return false;
        state_.selected_generation=m.generation;state_.incoming_generation=0;
        state_.incoming_ready=state_.format_pending=false;state_.incoming_source=m.source;
        state_.remote_endpoint=m.endpoint_hash;state_.remote_state=AudioState::Starting;return true;
    }
    if(m.kind==AudioControlKind::Format) {
        if(m.generation!=state_.selected_generation||state_.incoming_source==AudioSourceKind::None)return false;
        if(state_.incoming_generation==m.generation)return true;
        state_.incoming_generation=m.generation;state_.format_pending=true;return true;
    }
    if(m.kind==AudioControlKind::StreamState) {
        if(m.state==AudioState::Disabled&&m.generation>=state_.selected_generation) {
            state_.selected_generation=m.generation;state_.incoming_generation=0;state_.incoming_source=AudioSourceKind::None;
            state_.remote_endpoint.clear();state_.incoming_ready=state_.format_pending=false;state_.remote_state=m.state;return true;
        }
        if(m.generation!=state_.selected_generation)return false;state_.remote_state=m.state;
        if(m.state==AudioState::SourceLost||m.state==AudioState::Error)state_.incoming_ready=state_.format_pending=false;
        return true;
    }
    return false;
}
void AudioPeerSession::ReceiverReady() {
    if(!state_.format_pending||!state_.incoming_generation)return;
    state_.format_pending=false;state_.incoming_ready=true;
    AudioControl ready;ready.kind=AudioControlKind::Ready;ready.session=state_.session;ready.generation=state_.incoming_generation;
    if(pending_.size()<16)pending_.push_back(ready);
}
std::vector<AudioControl> AudioPeerSession::TakeControls(){std::vector<AudioControl> result;result.swap(pending_);return result;}
}
