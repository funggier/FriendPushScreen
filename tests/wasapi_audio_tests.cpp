#include "test_support.h"
#include "audio/wasapi_audio.h"
#include <Windows.h>
#include <iostream>
using namespace fps::audio;
void RunWasapiAudioTests() {
    std::cout<<"NativeAudioNoSourceDoesNotActivateDevice\n";
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    WasapiAudioSource source;std::string error;
    FPS_EXPECT_TRUE(source.Start("none",error));
    FPS_EXPECT_EQ(source.State(),AudioState::Disabled);
    FPS_EXPECT_TRUE(source.LoopbackEndpoint().empty());
    std::vector<AudioPcmChunk> chunks;FPS_EXPECT_TRUE(source.Poll(chunks,error));FPS_EXPECT_TRUE(chunks.empty());
    source.Stop();source.Stop();
    std::cout<<"NativeAudioInvalidEndpointFails\n";
    FPS_EXPECT_TRUE(!source.Start("render:__fps_missing_endpoint__",error));FPS_EXPECT_TRUE(!error.empty());
    FPS_EXPECT_EQ(source.State(),AudioState::Error);
    FPS_EXPECT_TRUE(!source.Start("application:1",error));FPS_EXPECT_TRUE(error.find("process")!=std::string::npos);
    WasapiAudioRenderer output;
    FPS_EXPECT_TRUE(!output.Start("__fps_missing_endpoint__",error));FPS_EXPECT_TRUE(!error.empty());
    FPS_EXPECT_TRUE(!output.Active());
    FPS_EXPECT_TRUE(!output.Queue(std::vector<float>(1920),error));FPS_EXPECT_TRUE(!error.empty());
    output.Stop();output.Stop();
    if(SUCCEEDED(com))CoUninitialize();
}
