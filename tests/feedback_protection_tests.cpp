#include "capture/feedback_protection.h"
#include "test_support.h"
#include "capture/capture_source_catalog.h"
#include "platform/windows/monitor_probe.h"
#include <array>
using namespace fps::capture;
void RunFeedbackProtectionTests() {
    const std::array<ScreenRect,4> windows{{{-1950,-50,-1800,150},{-1700,20,-1500,100},{0,0,80,80},{-2500,0,-2400,100}}};
    const auto masks=ClipFeedbackMasks({-1920,0,0,1080},windows,1920,1080);
    FPS_EXPECT_EQ(masks.size(),2ULL);
    if(masks.size()==2){FPS_EXPECT_EQ(masks[0],(PixelRect{0,0,120,150}));FPS_EXPECT_EQ(masks[1],(PixelRect{220,20,420,100}));}
    const std::array<ScreenRect,1> portrait{{{0,0,101,103}}};
    FPS_EXPECT_EQ(ClipFeedbackMasks({0,0,101,103},portrait,202,206),(std::vector<PixelRect>{{0,0,202,206}}));
    FPS_EXPECT_TRUE(ClipFeedbackMasks({0,0,0,1},portrait,1,1).empty());
    CapturedFrame frame;frame.width=5;frame.height=3;frame.row_pitch=24;frame.bgra.assign(72,0x7f);
    const std::array<PixelRect,2> overlapping{{{1,0,4,2},{3,1,5,3}}};std::string error;
    FPS_EXPECT_TRUE(MaskFeedbackPixels(frame,overlapping,error));
    FPS_EXPECT_EQ(frame.bgra[0],static_cast<std::uint8_t>(0x7f));
    FPS_EXPECT_EQ(frame.bgra[4],static_cast<std::uint8_t>(0));
    FPS_EXPECT_EQ(frame.bgra[7],static_cast<std::uint8_t>(255));
    FPS_EXPECT_EQ(frame.bgra[20],static_cast<std::uint8_t>(0x7f)); // row padding untouched
    FPS_EXPECT_EQ(frame.bgra[24+16],static_cast<std::uint8_t>(0));
    const auto before=frame.bgra;
    FPS_EXPECT_TRUE(!MaskFeedbackPixels(frame,std::array<PixelRect,1>{{{0,0,6,3}}},error));
    FPS_EXPECT_EQ(frame.bgra,before); // validate every rectangle before mutating
    frame.bgra.resize(4);FPS_EXPECT_TRUE(!MaskFeedbackPixels(frame,overlapping,error));
    const HWND viewer=CreateWindowExW(0,L"STATIC",L"feedback-test",WS_POPUP,0,0,10,10,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    FPS_EXPECT_TRUE(viewer!=nullptr);
    if(viewer){FPS_EXPECT_TRUE(!IsFriendPushViewer(viewer));DestroyWindow(viewer);}

    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FriendPushScreenMainWindow";
    const auto atom=RegisterClassW(&wc);
    const HWND self=CreateWindowExW(0,wc.lpszClassName,L"self-viewer-test",WS_POPUP,0,0,10,10,nullptr,nullptr,wc.hInstance,nullptr);
    FPS_EXPECT_TRUE(self&&IsFriendPushViewer(self));
    if(self) {
        CaptureSourceDescriptor source;source.kind=CaptureSourceKind::Window;source.native_handle=reinterpret_cast<std::uintptr_t>(self);
        FeedbackProtection protection;FPS_EXPECT_TRUE(!protection.Apply(frame,source,error));
        DestroyWindow(self);
    }
    if(atom)UnregisterClassW(wc.lpszClassName,wc.hInstance);

    // Production monitor descriptors carry a display name, not an HMONITOR.
    const auto monitorSources=BuildCaptureSourceCatalogFromProbes(fps::platform::windows::ProbeMonitors(),{},{});
    // Headless CI may expose no monitor; the physical probe is a separate gate.
    if(!monitorSources.empty()) {
        auto monitor=monitorSources.front();monitor.native_handle=0;
        CapturedFrame tiny;tiny.width=tiny.height=2;tiny.row_pitch=8;tiny.bgra.assign(16,0x44);
        FeedbackProtection monitorProtection;
        FPS_EXPECT_TRUE(monitorProtection.Apply(tiny,monitor,error));
        monitor.native_id="missing-monitor";monitor.native_handle=reinterpret_cast<std::uintptr_t>(MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY));
        FPS_EXPECT_TRUE(!monitorProtection.Apply(tiny,monitor,error));
    }

    Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
    if(FAILED(hr))hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context);
    FPS_EXPECT_TRUE(SUCCEEDED(hr));if(FAILED(hr))return;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=8;desc.Height=6;desc.MipLevels=1;desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    std::array<std::uint8_t,8*6*4> pixels;pixels.fill(0x55);
    D3D11_SUBRESOURCE_DATA initial{pixels.data(),32,0};CapturedFrame gpu;gpu.width=8;gpu.height=6;
    hr=device->CreateTexture2D(&desc,&initial,&gpu.texture);FPS_EXPECT_TRUE(SUCCEEDED(hr));if(FAILED(hr))return;
    auto original=gpu.texture;
    FeedbackProtection protector;const std::array<PixelRect,1> region{{{2,1,7,5}}};
    FPS_EXPECT_TRUE(protector.ApplyMasks(gpu,region,error));FPS_EXPECT_TRUE(gpu.texture.Get()!=original.Get());
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    hr=device->CreateTexture2D(&desc,nullptr,&staging);FPS_EXPECT_TRUE(SUCCEEDED(hr));if(FAILED(hr))return;
    context->CopyResource(staging.Get(),gpu.texture.Get());D3D11_MAPPED_SUBRESOURCE map{};
    hr=context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map);FPS_EXPECT_TRUE(SUCCEEDED(hr));if(FAILED(hr))return;
    for(unsigned y=0;y<6;++y)for(unsigned x=0;x<8;++x) {
        const auto* pixel=static_cast<const std::uint8_t*>(map.pData)+y*map.RowPitch+x*4;
        const bool masked=x>=2&&x<7&&y>=1&&y<5;
        FPS_EXPECT_EQ(pixel[0],static_cast<std::uint8_t>(masked?0:0x55));
        FPS_EXPECT_EQ(pixel[3],static_cast<std::uint8_t>(masked?255:0x55));
    }context->Unmap(staging.Get(),0);
    context->CopyResource(staging.Get(),original.Get());
    hr=context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map);FPS_EXPECT_TRUE(SUCCEEDED(hr));
    if(SUCCEEDED(hr)){FPS_EXPECT_EQ(static_cast<const std::uint8_t*>(map.pData)[map.RowPitch+8],static_cast<std::uint8_t>(0x55));context->Unmap(staging.Get(),0);}
}
