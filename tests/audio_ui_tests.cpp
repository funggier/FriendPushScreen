#include "test_support.h"
#include "ui/settings_panel.h"
#include "ui/settings_store.h"
#include "ui/main_window.h"
#include <iostream>
void RunAudioUiTests() {
    using namespace fps::ui;
    std::cout<<"AudioOutputPreferencesPersist\n";
    UiPreferences prefs;prefs.audio_source=AudioSourceKind::Device;
    prefs.audio_source_id="render:test-source";prefs.receiver_output_id="test-render-output";
    prefs.receiver_muted=true;prefs.receiver_volume=37;
    auto round=ParsePreferences(SerializePreferences(prefs));
    FPS_EXPECT_EQ(round.audio_source_id,prefs.audio_source_id);
    FPS_EXPECT_EQ(round.receiver_output_id,prefs.receiver_output_id);
    FPS_EXPECT_EQ(round.receiver_volume,37);FPS_EXPECT_TRUE(round.receiver_muted);
    FPS_EXPECT_TRUE(ShouldDeferUiCommand(UiCommandType::SelectAudioOutput));
    std::cout<<"AudioOutputChoiceUsesStableEndpointId\n";
    const auto parent=CreateWindowExW(0,L"STATIC",L"Audio UI test",WS_OVERLAPPEDWINDOW,0,0,600,900,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    {
        SettingsPanel panel;FPS_EXPECT_TRUE(panel.Create(parent,GetModuleHandleW(nullptr)));
        UiState s=MakeDefaultUiState(19045);
        s.audio_outputs.push_back({"test-render-output","Owned test output",false});
        s.audio_sources.push_back({"none","No Audio",{},fps::audio::AudioSourceKind::None,true});
        panel.ApplyState(s);
        FPS_EXPECT_TRUE(GetDlgItem(panel.Handle(),2130)!=nullptr);
        UiCommand selected;bool invoked=false;
        panel.SetCommandCallback([&](const UiCommand& command){selected=command;invoked=true;});
        SendMessageW(panel.Handle(),WM_APP+0x41,6,1);
        FPS_EXPECT_TRUE(invoked);
        FPS_EXPECT_EQ(selected.type,UiCommandType::SelectAudioOutput);
        FPS_EXPECT_EQ(selected.text,std::string("test-render-output"));
        invoked=false;SendMessageW(panel.Handle(),WM_APP+0x41,6,999);
        FPS_EXPECT_TRUE(!invoked);
        DestroyWindow(parent);
    }
}
