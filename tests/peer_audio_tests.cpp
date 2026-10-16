#include "test_support.h"
#include "audio/audio_session.h"
#include "network/secure_peer_transport.h"
#include <iostream>
using namespace fps::audio;
namespace {
void Transfer(AudioPeerSession& from,AudioPeerSession& to) {
    for(const auto& control:from.TakeControls()){auto wire=EncodeAudioControl(control);std::string error;auto message=DecodeAudioControl(wire,error);FPS_EXPECT_TRUE(message);if(message)FPS_EXPECT_TRUE(to.OnControl(*message));}
}
void AudioControlIsStrictAndReadyIsGenerationBound() {
    std::cout<<"AudioControlIsStrictAndReadyIsGenerationBound\n";
    AudioControl c;c.session=9;auto wire=EncodeAudioControl(c);std::string error;
    FPS_EXPECT_TRUE(IsAudioControl(wire));FPS_EXPECT_TRUE(DecodeAudioControl(wire,error));
    auto truncated=wire;if(!truncated.empty())truncated.pop_back();FPS_EXPECT_TRUE(!DecodeAudioControl(truncated,error));
    auto extra=wire;extra.push_back(0);FPS_EXPECT_TRUE(!DecodeAudioControl(extra,error));
    auto version=wire;if(version.size()>4)version[4]=99;FPS_EXPECT_TRUE(!DecodeAudioControl(version,error));
    c.session=0;FPS_EXPECT_TRUE(EncodeAudioControl(c).empty());
    AudioPeerSession a,b;a.Bind(9);b.Bind(9);Transfer(a,b);Transfer(b,a);
    FPS_EXPECT_TRUE(a.State().peer_capable&&b.State().peer_capable);
    a.SelectSource(AudioSourceKind::RenderLoopback,std::string(64,'A'));Transfer(a,b);
    FPS_EXPECT_TRUE(!a.State().outgoing_ready&&b.State().format_pending);
    b.ReceiverReady();Transfer(b,a);FPS_EXPECT_TRUE(a.State().outgoing_ready);
    const auto previous=a.State().outgoing_generation;
    a.SelectSource(AudioSourceKind::CaptureEndpoint,{});FPS_EXPECT_TRUE(a.State().outgoing_generation>previous&&!a.State().outgoing_ready);
    AudioControl stale;stale.kind=AudioControlKind::Ready;stale.session=9;stale.generation=previous;
    FPS_EXPECT_TRUE(!a.OnControl(stale));Transfer(a,b);b.ReceiverReady();Transfer(b,a);
    FPS_EXPECT_TRUE(a.State().outgoing_ready);
    stale.session=10;stale.generation=a.State().outgoing_generation;FPS_EXPECT_TRUE(!a.OnControl(stale));
    a.SelectSource(AudioSourceKind::None,{});Transfer(a,b);
    FPS_EXPECT_TRUE(!a.State().outgoing_ready&&!b.State().incoming_ready);
    FPS_EXPECT_EQ(b.State().remote_state,AudioState::Disabled);
    a.Bind(10);FPS_EXPECT_TRUE(!a.State().peer_capable&&!a.State().outgoing_ready&&a.State().incoming_generation==0);
    AudioControl invalid;invalid.kind=AudioControlKind::Format;invalid.session=10;invalid.generation=42;
    FPS_EXPECT_TRUE(!a.OnControl(invalid)); // No source/capabilities first.
}
}
void RunPeerAudioTests() {
    AudioControlIsStrictAndReadyIsGenerationBound();
    fps::network::SecurePeerTransport t;
    FPS_EXPECT_EQ(t.SendAudio(std::vector<std::uint8_t>(1025)),fps::network::TransportSendResult::Invalid);
}
