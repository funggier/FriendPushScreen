#include "ui/main_window.h"
#include "ui/settings_store.h"

#include "test_support.h"

void RunMainWindowLayoutTests() {
    using fps::ui::CalculateViewerViewport;
    using fps::ui::ShouldDeferUiCommand;
    using fps::ui::UiCommandType;

    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::SelectCaptureSource));
    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::ToggleCapturePreview));
    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::RefreshCaptureSources));
    FPS_EXPECT_TRUE(
        ShouldDeferUiCommand(
            UiCommandType::ToggleShowCursor));
    FPS_EXPECT_TRUE(
        !ShouldDeferUiCommand(
            UiCommandType::ToggleSettings));

    {
        const auto viewport =
            CalculateViewerViewport(
                1280,
                720,
                30);

        FPS_EXPECT_EQ(viewport.x, 0);
        FPS_EXPECT_EQ(viewport.y, 30);
        FPS_EXPECT_EQ(viewport.width, 1280);
        FPS_EXPECT_EQ(viewport.height, 690);
    }

    {
        const auto viewport =
            CalculateViewerViewport(
                1280,
                720,
                0);

        FPS_EXPECT_EQ(viewport.x, 0);
        FPS_EXPECT_EQ(viewport.y, 0);
        FPS_EXPECT_EQ(viewport.width, 1280);
        FPS_EXPECT_EQ(viewport.height, 720);
    }

    {
        const auto viewport =
            CalculateViewerViewport(
                640,
                20,
                30);

        FPS_EXPECT_EQ(viewport.x, 0);
        FPS_EXPECT_EQ(viewport.y, 20);
        FPS_EXPECT_EQ(viewport.width, 640);
        FPS_EXPECT_EQ(viewport.height, 0);
    }

    {
        const auto viewport =
            CalculateViewerViewport(
                -100,
                -50,
                30);

        FPS_EXPECT_EQ(viewport.width, 0);
        FPS_EXPECT_EQ(viewport.height, 0);
    }
    {
        fps::ui::MainWindow window;
        FPS_EXPECT_TRUE(window.Create(GetModuleHandleW(nullptr),SW_HIDE));
        if(window.Handle()) {
            auto state=fps::ui::MakeDefaultUiState(19045);
            state.connection_state=fps::ui::ConnectionUiState::Connected;
            state.peer_round_trip_ms=25;state.peer_frame_rates={59.8,60.0};
            window.ApplyUiState(state);
            for(int width:{640,1280}) {
                SetWindowPos(window.Handle(),nullptr,0,0,width,720,SWP_NOZORDER|SWP_NOACTIVATE);
                const auto status=FindWindowExW(window.Handle(),nullptr,L"Static",nullptr);
                const auto toggle=GetDlgItem(window.Handle(),2201);
                FPS_EXPECT_TRUE(status&&toggle);
                RECT statusBounds{},toggleBounds{};
                GetWindowRect(status,&statusBounds);GetWindowRect(toggle,&toggleBounds);
                FPS_EXPECT_TRUE(statusBounds.right<=toggleBounds.left);
            }
            DestroyWindow(window.Handle());
            MSG quit{};while(PeekMessageW(&quit,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)){}
        }
    }
    {
        std::cout << "HiddenStatusStripFillsViewerAndTabRestoresSettings\n";
        fps::ui::MainWindow window;
        FPS_EXPECT_TRUE(window.Create(GetModuleHandleW(nullptr),SW_HIDE));
        if(window.Handle()) {
            auto state=fps::ui::MakeStateFromPreferences(
                fps::ui::ParsePreferences("status_bar_visible=false\nsettings_visible=false\nborderless=true\n"),19045);
            window.ApplyUiState(state);
            const auto status=FindWindowExW(window.Handle(),nullptr,L"Static",nullptr);
            const auto toggle=GetDlgItem(window.Handle(),2201);
            const auto panel=FindWindowExW(window.Handle(),nullptr,L"FriendPushScreenSettingsPanel",nullptr);
            const auto statusCheck=GetDlgItem(panel,2131);
            FPS_EXPECT_TRUE(status&&toggle&&panel&&statusCheck);
            const auto visible=[](HWND h){return (GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE)!=0;};
            const auto expectFullView=[&]{
                RECT client{},render{};GetClientRect(window.Handle(),&client);
                GetWindowRect(window.RenderHandle(),&render);
                MapWindowPoints(nullptr,window.Handle(),reinterpret_cast<POINT*>(&render),2);
                FPS_EXPECT_EQ(render.left,0L);FPS_EXPECT_EQ(render.top,0L);
                FPS_EXPECT_EQ(render.right,client.right);FPS_EXPECT_EQ(render.bottom,client.bottom);
                FPS_EXPECT_TRUE(!visible(status)&&!visible(toggle));
            };
            expectFullView();
            FPS_EXPECT_TRUE(!visible(panel));
            window.SetUiCommandCallback([&](const fps::ui::UiCommand& command){
                if(command.type==UiCommandType::ToggleSettings) {
                    fps::ui::ToggleSettingsVisible(state);window.ApplyUiState(state);
                }
            });
            MSG tab{};tab.hwnd=window.RenderHandle();tab.message=WM_KEYDOWN;tab.wParam=VK_TAB;
            FPS_EXPECT_TRUE(window.HandleShortcut(tab));
            FPS_EXPECT_TRUE(state.preferences.settings_visible&&visible(panel));
            expectFullView();
            FPS_EXPECT_EQ(SendMessageW(statusCheck,BM_GETCHECK,0,0),static_cast<LRESULT>(BST_UNCHECKED));
            state.fullscreen=true;window.ApplyUiState(state);expectFullView();
            state.fullscreen=false;window.ApplyUiState(state);expectFullView();
            state.preferences=fps::ui::ParsePreferences("status_bar_visible=true\n");
            window.ApplyUiState(state);
            FPS_EXPECT_TRUE(visible(status)&&visible(toggle));
            FPS_EXPECT_EQ(SendMessageW(statusCheck,BM_GETCHECK,0,0),static_cast<LRESULT>(BST_CHECKED));
            RECT render{};GetWindowRect(window.RenderHandle(),&render);
            MapWindowPoints(nullptr,window.Handle(),reinterpret_cast<POINT*>(&render),2);
            FPS_EXPECT_EQ(render.top,30L);
            state.fullscreen=true;window.ApplyUiState(state);expectFullView();
            state.fullscreen=false;window.ApplyUiState(state);
            FPS_EXPECT_TRUE(visible(status)&&visible(toggle));
            DestroyWindow(window.Handle());
            MSG quit{};while(PeekMessageW(&quit,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)){}
        }
    }
}
