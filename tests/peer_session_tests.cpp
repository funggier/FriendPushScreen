#include "network/peer_session.h"
#include "test_support.h"
#include <algorithm>
using namespace fps::network;
namespace {
bool Has(const std::vector<SessionAction>& a,SessionActionKind k){return std::any_of(a.begin(),a.end(),[&](auto& v){return v.kind==k;});}
ControlMessage Hello(std::uint64_t token){ControlMessage h;h.session=99;h.value=token;return h;}
void Handshake(PeerSession& s,std::uint64_t id=1,bool client=true,std::uint64_t nonce=123,bool video_ready=true) {
    s.OnConnected(id,client,10,client?nonce:0);s.TakeActions();
    FPS_EXPECT_TRUE(s.OnControl(id,Hello(nonce),11));
    FPS_EXPECT_TRUE(s.State().connected);
    s.TakeActions();
    if(video_ready&&s.State().outgoing_generation){ControlMessage ready;ready.type=ControlType::VideoReady;ready.session=nonce;ready.generation=s.State().outgoing_generation;s.OnControl(id,ready,12);s.TakeActions();}
}
}
void RunPeerSessionTests() {
    VideoFormat f{640,480,640,480};
    PeerSession waiting("AA","BB",6);waiting.RequestShare(f);Handshake(waiting,1,true,123,false);
    FPS_EXPECT_TRUE(!waiting.State().sharing); // no encoded frame before the remote decoder is ready
    ControlMessage ready;ready.type=ControlType::VideoReady;ready.session=123;ready.generation=waiting.State().outgoing_generation;
    FPS_EXPECT_TRUE(waiting.OnControl(1,ready,12));FPS_EXPECT_TRUE(waiting.State().sharing);
    FPS_EXPECT_TRUE(Has(waiting.TakeActions(),SessionActionKind::StartSender));
    const auto unchanged_generation=waiting.State().outgoing_generation;
    ControlMessage lost;lost.type=ControlType::KeyframeRequest;lost.session=123;lost.generation=unchanged_generation;
    FPS_EXPECT_TRUE(waiting.OnControl(1,lost,13));
    const auto recovery_actions=waiting.TakeActions();
    FPS_EXPECT_TRUE(!Has(recovery_actions,SessionActionKind::ForceKeyframe));
    FPS_EXPECT_TRUE(Has(recovery_actions,SessionActionKind::StopSender));
    FPS_EXPECT_TRUE(waiting.State().outgoing_generation>unchanged_generation);
    FPS_EXPECT_TRUE(std::any_of(recovery_actions.begin(),recovery_actions.end(),[](const auto& action){return action.kind==SessionActionKind::SendControl&&action.message.type==ControlType::ShareStart&&action.message.value==1;}));
    FPS_EXPECT_TRUE(!waiting.State().sharing);
    PeerSession s("AA","BB",7);
    FPS_EXPECT_TRUE(s.RequestShare(f));FPS_EXPECT_TRUE(!s.State().sharing);
    Handshake(s);FPS_EXPECT_TRUE(s.State().sharing);const auto generation=s.State().outgoing_generation;
    ControlMessage start;start.type=ControlType::ShareStart;start.session=123;start.generation=9;
    FPS_EXPECT_TRUE(s.OnControl(1,start,12));
    auto format=start;format.type=ControlType::VideoFormat;format.format=f;
    FPS_EXPECT_TRUE(s.OnControl(1,format,13));FPS_EXPECT_TRUE(s.State().receiving);
    s.TakeActions();s.StopShare();const auto actions=s.TakeActions();
    FPS_EXPECT_TRUE(s.State().connected&&s.State().receiving&&!s.State().sharing);
    FPS_EXPECT_TRUE(Has(actions,SessionActionKind::StopSender));FPS_EXPECT_TRUE(!Has(actions,SessionActionKind::StopReceiver));
    VideoWireFrame v;v.session=123;v.generation=9;v.sequence=1;v.format=f;
    FPS_EXPECT_TRUE(s.AcceptVideo(1,v,15));FPS_EXPECT_TRUE(!s.AcceptVideo(1,v,16));
    v.sequence=2;v.generation=8;FPS_EXPECT_TRUE(!s.AcceptVideo(1,v,17));
    auto stop=start;stop.type=ControlType::ShareStop;stop.generation=8;
    FPS_EXPECT_TRUE(s.OnControl(1,stop,18));FPS_EXPECT_TRUE(s.State().receiving);
    stop.generation=9;FPS_EXPECT_TRUE(s.OnControl(1,stop,19));FPS_EXPECT_TRUE(!s.State().receiving);
    FPS_EXPECT_TRUE(s.OnControl(1,format,20));FPS_EXPECT_TRUE(!s.State().receiving); // late format cannot resurrect stopped stream
    FPS_EXPECT_TRUE(s.RequestShare(f));FPS_EXPECT_TRUE(s.State().outgoing_generation>generation);
    s.SetConnectionIntent(true,21);s.OnDisconnected(1,22);FPS_EXPECT_TRUE(!s.State().connected);
    s.Tick(300);FPS_EXPECT_TRUE(Has(s.TakeActions(),SessionActionKind::Reconnect));
    Handshake(s,2,true,456);v.session=123;v.generation=9;v.sequence=3;FPS_EXPECT_TRUE(!s.AcceptVideo(2,v,30));
    s.Disconnect();FPS_EXPECT_TRUE(!s.State().connected&&!s.State().sharing&&!s.State().receiving);
    s.TakeActions();s.Tick(100000);FPS_EXPECT_TRUE(!Has(s.TakeActions(),SessionActionKind::Reconnect));

    PeerSession receiver("BB","AA",8);Handshake(receiver,1,false,800);
    FPS_EXPECT_TRUE(!receiver.State().sharing);
    start.session=800;format.session=800;
    receiver.OnControl(1,start,12);receiver.OnControl(1,format,13);
    FPS_EXPECT_TRUE(receiver.State().receiving); // no local source or sharing required
    receiver.Tick(5000);FPS_EXPECT_TRUE(!receiver.State().connected);
    FPS_EXPECT_TRUE(Has(receiver.TakeActions(),SessionActionKind::CloseConnection));

    PeerSession simultaneous("AA","BB",9);
    Handshake(simultaneous,10,false,300); // peer B initiated first
    Handshake(simultaneous,20,true,400);  // peer A's connection wins at both ends
    FPS_EXPECT_EQ(simultaneous.State().connection,20ULL);
    FPS_EXPECT_EQ(simultaneous.State().token,400ULL);
    auto duplicate=Hello(300);simultaneous.OnControl(10,duplicate,50);
    FPS_EXPECT_EQ(simultaneous.State().connection,20ULL);
    PeerSession other("BB","AA",10);Handshake(other,10,true,300);Handshake(other,20,false,400);
    FPS_EXPECT_EQ(other.State().token,400ULL);
    PeerSession recovering("BB","AA",11);Handshake(recovering,1,false,900);
    start.session=900;start.generation=7;start.value=0;format=start;format.type=ControlType::VideoFormat;format.format=f;
    FPS_EXPECT_TRUE(recovering.OnControl(1,start,50));FPS_EXPECT_TRUE(recovering.OnControl(1,format,51));recovering.TakeActions();
    start.generation=8;start.value=1;
    FPS_EXPECT_TRUE(recovering.OnControl(1,start,52));
    const auto paused=recovering.TakeActions();
    FPS_EXPECT_TRUE(std::any_of(paused.begin(),paused.end(),[](const auto& action){return action.kind==SessionActionKind::StopReceiver&&action.message.value==1;}));
    format=start;format.type=ControlType::VideoFormat;format.format=f;
    FPS_EXPECT_TRUE(recovering.OnControl(1,format,53));
    start.generation=9;FPS_EXPECT_TRUE(recovering.OnControl(1,start,54));
    format.generation=9;format.format.coded_width=format.format.visible_width=642;
    FPS_EXPECT_TRUE(!recovering.OnControl(1,format,55)); // A recovery marker cannot conceal a source/format change.
}
