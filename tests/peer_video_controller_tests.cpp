#include "app/peer_video_controller.h"
#include "test_support.h"
using namespace fps::app;
void RunPeerVideoControllerTests() {
    const auto root=std::filesystem::current_path()/("fps-controller-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    {
        PeerVideoController a,b;std::string error;
        const bool initialized=a.Initialize(nullptr,root/"a",error)&&b.Initialize(nullptr,root/"b",error);
        FPS_EXPECT_TRUE(initialized);if(!initialized){std::filesystem::remove_all(root);return;}
        FPS_EXPECT_TRUE(!a.Listen(0,error)); // explicit trust required before opening any listener
        FPS_EXPECT_TRUE(a.AuthorizePeer(*b.Identity(),error)&&b.AuthorizePeer(*a.Identity(),error));
        FPS_EXPECT_TRUE(!a.AuthorizePeer(*a.Identity(),error));
        FPS_EXPECT_TRUE(a.Listen(0,error)&&b.Listen(0,error));
        const auto sink=[](const fps::capture::CapturedFrame&){return true;};const auto clear=[]{};
        auto until=GetTickCount64()+5000;
        while(GetTickCount64()<until&&(!a.Snapshot().listen_port||!b.Snapshot().listen_port)){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        auto as=a.Snapshot(),bs=b.Snapshot();
        FPS_EXPECT_TRUE(as.listen_port&&bs.listen_port&&as.listen_port!=bs.listen_port);
        FPS_EXPECT_TRUE(a.Connect("127.0.0.1",bs.listen_port,0,error)&&b.Connect("127.0.0.1",as.listen_port,0,error));
        until=GetTickCount64()+5000;
        while(GetTickCount64()<until&&(!a.Snapshot().connected||!b.Snapshot().connected)){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_TRUE(a.Snapshot().connected&&b.Snapshot().connected);
        until=GetTickCount64()+100;
        while(GetTickCount64()<until){a.Poll(sink,clear);b.Poll(sink,clear);Sleep(2);}
        FPS_EXPECT_EQ(a.Snapshot().session,b.Snapshot().session);
        FPS_EXPECT_TRUE(a.StartShare(error));a.StopShare();FPS_EXPECT_TRUE(a.Snapshot().connected);
        FPS_EXPECT_TRUE(!a.Snapshot().wants_share&&!a.Snapshot().sharing);
        a.Disconnect();b.Disconnect();
        FPS_EXPECT_TRUE(!a.Snapshot().connected&&!a.Snapshot().wants_share);
    }
    std::filesystem::remove_all(root);
}
