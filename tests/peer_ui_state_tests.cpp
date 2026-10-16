#include "ui/ui_state.h"
#include "ui/main_window.h"
#include "test_support.h"
using namespace fps::ui;
void RunPeerUiStateTests() {
    UiState s=MakeDefaultUiState(19045);PeerUiSnapshot p;
    p.active=true;p.status="Listening on 42801";p.own_fingerprint="own";p.peer_fingerprint="friend";
    ApplyPeerUiSnapshot(s,p);FPS_EXPECT_EQ(s.connection_state,ConnectionUiState::Listening);FPS_EXPECT_TRUE(s.transport_active);
    p.connecting=true;p.share_intent=true;ApplyPeerUiSnapshot(s,p);
    FPS_EXPECT_EQ(s.connection_state,ConnectionUiState::ConnectRequested);FPS_EXPECT_EQ(s.share_state,ShareUiState::ShareRequested);
    p.connected=true;p.connecting=false;p.receiving=true;ApplyPeerUiSnapshot(s,p);
    FPS_EXPECT_EQ(s.connection_state,ConnectionUiState::Connected);FPS_EXPECT_TRUE(s.receiving_video);
    p.share_intent=false;p.sharing=false;ApplyPeerUiSnapshot(s,p);
    FPS_EXPECT_TRUE(s.receiving_video);FPS_EXPECT_EQ(s.share_state,ShareUiState::NotSharing);
    p.share_intent=true;p.sharing=true;ApplyPeerUiSnapshot(s,p);FPS_EXPECT_EQ(s.share_state,ShareUiState::Sharing);
    FPS_EXPECT_EQ(s.own_fingerprint,std::string("own"));FPS_EXPECT_EQ(s.peer_fingerprint,std::string("friend"));
    p={};p.status="Unpaired peer";ApplyPeerUiSnapshot(s,p);
    FPS_EXPECT_TRUE(!s.receiving_video&&!s.transport_active);FPS_EXPECT_EQ(s.share_state,ShareUiState::NotSharing);
    FPS_EXPECT_EQ(s.peer_status,std::string("Unpaired peer"));
    FPS_EXPECT_TRUE(ShouldDeferUiCommand(UiCommandType::ToggleShareRequest));
    FPS_EXPECT_TRUE(ShouldDeferUiCommand(UiCommandType::ToggleConnectionRequest));
}
