#include "graphics/preview_renderer.h"

#include "test_support.h"
#include "ui/main_window.h"
#include "ui/theme.h"
#include <commctrl.h>
#include <iostream>
#include <array>
#include <vector>

#include <cmath>

namespace {
LRESULT CALLBACK CountCodecReloads(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR data) {
    if(message==LB_RESETCONTENT)++*reinterpret_cast<unsigned*>(data);
    return DefSubclassProc(window,message,wparam,lparam);
}
BOOL CALLBACK FindListChildren(HWND child,LPARAM data) {
    wchar_t name[32]{};GetClassNameW(child,name,32);
    if(lstrcmpiW(name,L"LISTBOX")==0)reinterpret_cast<std::vector<HWND>*>(data)->push_back(child);
    return TRUE;
}
BOOL CALLBACK FindThreadLists(HWND top,LPARAM data){EnumChildWindows(top,FindListChildren,data);return TRUE;}
LRESULT CALLBACK CountSettingsText(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR,DWORD_PTR data) {
    if(message==WM_SETTEXT)++*reinterpret_cast<unsigned*>(data);
    return DefSubclassProc(window,message,wparam,lparam);
}
}
void RunPreviewRendererTests() {
    using fps::graphics::CalculateAspectFit;

    {
        const auto fit =
            CalculateAspectFit(
                1920,
                1080,
                1920,
                1080);

        FPS_EXPECT_TRUE(
            std::abs(fit.x) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(fit.y) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.width -
                1920.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.height -
                1080.0F) < 0.01F);
    }

    {
        const auto fit =
            CalculateAspectFit(
                1264,
                691,
                2560,
                1440);

        FPS_EXPECT_TRUE(
            std::abs(
                fit.height -
                691.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.width -
                1228.4445F) < 0.1F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.x -
                17.7778F) < 0.1F);
        FPS_EXPECT_TRUE(
            std::abs(fit.y) < 0.01F);
    }

    {
        const auto fit =
            CalculateAspectFit(
                1280,
                720,
                1080,
                1920);

        FPS_EXPECT_TRUE(
            std::abs(
                fit.width -
                405.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.height -
                720.0F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(
                fit.x -
                437.5F) < 0.01F);
        FPS_EXPECT_TRUE(
            std::abs(fit.y) < 0.01F);
    }

    {
        const auto fit =
            CalculateAspectFit(
                0,
                720,
                1920,
                1080);

        FPS_EXPECT_TRUE(
            fit.width == 0.0F &&
            fit.height == 0.0F &&
            fit.scale == 0.0);
    }
    {
        fps::ui::MainWindow window;FPS_EXPECT_TRUE(window.Create(GetModuleHandleW(nullptr),SW_HIDE));
        if(window.Handle()) {
            HWND hide=GetDlgItem(window.Handle(),2201);FPS_EXPECT_TRUE(hide!=nullptr);
            if(hide) {
                RECT rect{};GetClientRect(hide,&rect);
                HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth=std::max<LONG>(16,rect.right);info.bmiHeader.biHeight=-std::max<LONG>(16,rect.bottom);
                info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
                void* pixels=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
                HGDIOBJ previous=SelectObject(dc,bitmap);RECT area{0,0,info.bmiHeader.biWidth,-info.bmiHeader.biHeight};
                HBRUSH marker=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&area,marker);DeleteObject(marker);
                SendMessageW(hide,WM_ERASEBKGND,reinterpret_cast<WPARAM>(dc),0);
                const auto color=GetPixel(dc,5,5);
                std::cout<<"hide_settings_erase_rgb="<<static_cast<unsigned>(GetRValue(color))<<','<<static_cast<unsigned>(GetGValue(color))<<','<<static_cast<unsigned>(GetBValue(color))<<'\n';
                FPS_EXPECT_EQ(color,fps::ui::theme::kButton);
                SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
                unsigned textChanges=0;SetWindowSubclass(hide,CountSettingsText,991,reinterpret_cast<DWORD_PTR>(&textChanges));
                fps::ui::UiState stable;window.ApplyUiState(stable);window.ApplyUiState(stable);
                FPS_EXPECT_EQ(textChanges,0U);
                RemoveWindowSubclass(hide,CountSettingsText,991);
            }
            {
                HWND panel=FindWindowExW(window.Handle(),nullptr,L"FriendPushScreenSettingsPanel",nullptr);
                FPS_EXPECT_TRUE(panel!=nullptr);
                fps::ui::UiState stable;window.ApplyUiState(stable);
                const std::array<HWND,3> buttons{GetDlgItem(panel,2102),GetDlgItem(panel,2121),GetDlgItem(panel,2122)};
                unsigned captions=0,reloads=0;std::vector<HWND> lists;
                EnumThreadWindows(GetCurrentThreadId(),FindThreadLists,reinterpret_cast<LPARAM>(&lists));
                for(const auto button:buttons){ValidateRect(button,nullptr);SetWindowSubclass(button,CountSettingsText,992,reinterpret_cast<DWORD_PTR>(&captions));}
                for(const auto list:lists)SetWindowSubclass(list,CountCodecReloads,993,reinterpret_cast<DWORD_PTR>(&reloads));
                stable.peer_status="Authenticated peer | sent 120 received 120";window.ApplyUiState(stable);
                unsigned dirty=0;for(const auto button:buttons)if(GetUpdateRect(button,nullptr,FALSE))++dirty;
                std::cout<<"settings_status_update captions="<<captions<<" codec_reloads="<<reloads<<" dirty_buttons="<<dirty<<'\n';
                FPS_EXPECT_EQ(captions,0U);FPS_EXPECT_EQ(reloads,0U);FPS_EXPECT_EQ(dirty,0U);
                for(const auto button:buttons)RemoveWindowSubclass(button,CountSettingsText,992);
                for(const auto list:lists)RemoveWindowSubclass(list,CountCodecReloads,993);
            }
            unsigned callbacks=0;window.SetResizeCallback([&](UINT,UINT){++callbacks;});
            fps::ui::UiState state;window.ApplyUiState(state);const auto initial=callbacks;
            state.peer_status="Authenticated peer connected | receiving H.264";window.ApplyUiState(state);
            window.ApplyUiState(state);
            FPS_EXPECT_EQ(callbacks,initial); // Status changes must not resize/present the peer viewer.
            RECT bounds{};GetWindowRect(window.Handle(),&bounds);
            SetWindowPos(window.Handle(),nullptr,0,0,bounds.right-bounds.left+40,bounds.bottom-bounds.top+30,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
            FPS_EXPECT_TRUE(callbacks>initial);
            {
                fps::graphics::D3D11Device device;const bool initialized=device.Initialize(window.RenderHandle());
                FPS_EXPECT_TRUE(initialized);
                if(initialized) {
                    DXGI_SWAP_CHAIN_DESC1 before{};FPS_EXPECT_TRUE(SUCCEEDED(device.SwapChain()->GetDesc1(&before)));
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> held;
                    FPS_EXPECT_TRUE(SUCCEEDED(device.SwapChain()->GetBuffer(0,IID_PPV_ARGS(&held))));
                    auto* target=device.RenderTarget();
                    FPS_EXPECT_TRUE(device.Resize(before.Width,before.Height));
                    FPS_EXPECT_TRUE(device.RenderTarget()==target);
                    held.Reset();
                    FPS_EXPECT_TRUE(device.Resize(before.Width+8,before.Height+8));
                    DXGI_SWAP_CHAIN_DESC1 after{};FPS_EXPECT_TRUE(SUCCEEDED(device.SwapChain()->GetDesc1(&after)));
                    FPS_EXPECT_EQ(after.Width,before.Width+8);FPS_EXPECT_EQ(after.Height,before.Height+8);
                }
            }
            DestroyWindow(window.Handle());
            MSG quit{};PeekMessageW(&quit,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE);
        }
    }
}
