#include "test_support.h"
#include "network/peer_session.h"
#include "ui/ui_state.h"
#include <algorithm>
#include <iostream>
using namespace fps;
namespace {
void Connect(network::PeerSession& s,std::uint64_t id=1,std::uint64_t token=123) {
    s.OnConnected(id,true,10,token);s.TakeActions();
    network::ControlMessage hello;hello.session=99;hello.value=token;
    FPS_EXPECT_TRUE(s.OnControl(id,hello,11));s.TakeActions();
}
network::ControlMessage Pong(std::uint64_t stamp,std::uint64_t token=123) {
    network::ControlMessage m;m.type=network::ControlType::Pong;m.session=token;m.value=stamp;return m;
}
void PingUsesOnlyMatchingFreshSessionReplies() {
    std::cout<<"PingUsesOnlyMatchingFreshSessionReplies\n";
    network::PeerSession s("AA","BB",1);Connect(s);
    FPS_EXPECT_TRUE(!s.State().round_trip_ms);
    s.Tick(1010);auto actions=s.TakeActions();
    FPS_EXPECT_EQ(std::count_if(actions.begin(),actions.end(),[](const auto& a){return a.kind==network::SessionActionKind::SendControl&&a.message.type==network::ControlType::Ping&&a.message.value==1010;}),1);
    s.OnControl(1,Pong(1010),1035);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{25});
    s.OnControl(1,Pong(1010),1040);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{25});
    s.Tick(2010);s.TakeActions();
    s.OnControl(1,Pong(2010,999),2030);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{25});
    s.OnControl(1,Pong(9999),2040);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{25});
    s.OnControl(1,Pong(2010),2060);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{50});
    auto keepalive=Pong(77);keepalive.type=network::ControlType::Ping;s.OnControl(1,keepalive,5000);
    s.Tick(5061);FPS_EXPECT_TRUE(s.State().connected&&!s.State().round_trip_ms);
    s.Disconnect();FPS_EXPECT_TRUE(!s.State().round_trip_ms);
    Connect(s,2,456);s.Tick(1010);s.TakeActions();
    s.OnControl(2,Pong(1010),1020);FPS_EXPECT_TRUE(!s.State().round_trip_ms);
    s.OnControl(2,Pong(1010,456),1010);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{0});
}
void DelayedPongDoesNotOverwriteNewerRtt() {
    std::cout<<"DelayedPongDoesNotOverwriteNewerRtt\n";
    network::PeerSession s("AA","BB",2);Connect(s);
    s.Tick(1010);s.Tick(2010);s.Tick(3010);s.TakeActions();
    s.OnControl(1,Pong(1010),3210);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{2200});
    s.OnControl(1,Pong(3010),3220);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{210});
    s.OnControl(1,Pong(2010),3230);FPS_EXPECT_EQ(s.State().round_trip_ms,std::optional<std::uint64_t>{210});
}
void FrameRatesUseElapsedCountersAndResetIndependently() {
    std::cout<<"FrameRatesUseElapsedCountersAndResetIndependently\n";
    ui::PeerFrameRateMeter meter;ui::PeerFrameCounters c{7,1,2,100,200,true,true};
    auto r=meter.Update(100,c);FPS_EXPECT_TRUE(!r.send_fps&&!r.view_fps);
    c.sent=110;c.viewed=210;r=meter.Update(500,c);FPS_EXPECT_TRUE(!r.send_fps&&!r.view_fps);
    c.sent=160;c.viewed=230;r=meter.Update(1100,c);
    FPS_EXPECT_EQ(r.send_fps,std::optional<double>{60});FPS_EXPECT_EQ(r.view_fps,std::optional<double>{30});
    c.sent=220;r=meter.Update(2100,c);FPS_EXPECT_EQ(r.send_fps,std::optional<double>{60});FPS_EXPECT_EQ(r.view_fps,std::optional<double>{0});
    c.send_generation=3;c.sent=0;r=meter.Update(2200,c);FPS_EXPECT_TRUE(!r.send_fps);FPS_EXPECT_EQ(r.view_fps,std::optional<double>{0});
    c.sent=30;c.viewed=260;r=meter.Update(3200,c);FPS_EXPECT_EQ(r.send_fps,std::optional<double>{30});FPS_EXPECT_TRUE(r.view_fps&&*r.view_fps>27&&*r.view_fps<28);
    c.session=8;r=meter.Update(3300,c);FPS_EXPECT_TRUE(!r.send_fps&&!r.view_fps);
    c.session=9;c.sent=c.viewed=0;meter.Update(4000,c);c.sent=c.viewed=45;r=meter.Update(5500,c);
    FPS_EXPECT_EQ(r.send_fps,std::optional<double>{30});FPS_EXPECT_EQ(r.view_fps,std::optional<double>{30});
    c.sent=1;r=meter.Update(6500,c);FPS_EXPECT_TRUE(!r.send_fps);FPS_EXPECT_EQ(r.view_fps,std::optional<double>{0});
    c.sending=c.viewing=false;r=meter.Update(6501,c);FPS_EXPECT_TRUE(!r.send_fps&&!r.view_fps);
    c.session=0;r=meter.Update(6502,c);FPS_EXPECT_TRUE(!r.send_fps&&!r.view_fps);
}
void PeerStatusFormatsMeasuredValuesAndUnavailable() {
    std::cout<<"PeerStatusFormatsMeasuredValuesAndUnavailable\n";
    ui::UiState state;const auto dash=std::wstring(L"\u2014");
    auto text=ui::BuildPeerMetricsText(state);
    FPS_EXPECT_TRUE(text.find(L"Ping: "+dash)!=std::wstring::npos);
    FPS_EXPECT_TRUE(text.find(L"FPS Send: "+dash)!=std::wstring::npos);
    state.connection_state=ui::ConnectionUiState::Connected;state.peer_round_trip_ms=25;state.peer_frame_rates={59.8,30.0};
    text=ui::BuildPeerMetricsText(state);FPS_EXPECT_TRUE(text.find(L"Ping: 25 ms")!=std::wstring::npos);
    FPS_EXPECT_TRUE(text.find(L"FPS Send: 59.8 / View: 30.0")!=std::wstring::npos);
    state.peer_round_trip_ms=0;FPS_EXPECT_TRUE(ui::BuildPeerMetricsText(state).find(L"Ping: <1 ms")!=std::wstring::npos);
    state.connection_state=ui::ConnectionUiState::Disconnected;
    FPS_EXPECT_TRUE(ui::BuildPeerMetricsText(state).find(L"25 ms")==std::wstring::npos);
}
}
void RunPeerStatusMetricsTests() {
    PingUsesOnlyMatchingFreshSessionReplies();DelayedPongDoesNotOverwriteNewerRtt();
    FrameRatesUseElapsedCountersAndResetIndependently();PeerStatusFormatsMeasuredValuesAndUnavailable();
}
