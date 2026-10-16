// Physical DDA/WGC feedback qualification against two explicitly owned app PIDs.
#include "capture/capture_source_catalog.h"
#include "capture/desktop_duplication_capture.h"
#include "capture/windows_graphics_capture.h"
#include "capture/feedback_protection.h"
#include <Windows.h>
#include <roapi.h>
#include <mfapi.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
using namespace fps::capture;
namespace {
struct OwnedWindows {std::array<DWORD,2> pids{};std::array<HWND,2> windows{};};
BOOL CALLBACK FindOwned(HWND window,LPARAM argument) {
    auto& owned=*reinterpret_cast<OwnedWindows*>(argument);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    wchar_t name[128]{};GetClassNameW(window,name,128);
    if(IsWindowVisible(window)&&std::wstring(name)==L"FriendPushScreenMainWindow")
        for(std::size_t i=0;i<2;++i)if(pid==owned.pids[i])owned.windows[i]=window;
    return TRUE;
}
LRESULT CALLBACK ReferenceProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
    if(message==WM_TIMER){InvalidateRect(window,nullptr,FALSE);return 0;}
    if(message==WM_PAINT){
        PAINTSTRUCT paint{};const auto dc=BeginPaint(window,&paint);RECT client{};GetClientRect(window,&client);
        FillRect(dc,&client,reinterpret_cast<HBRUSH>(GetClassLongPtrW(window,GCLP_HBRBACKGROUND)));
        RECT marker{0,0,8,8};const auto brush=CreateSolidBrush((GetTickCount64()/16)%2?RGB(240,20,20):RGB(20,20,240));
        FillRect(dc,&marker,brush);DeleteObject(brush);EndPaint(window,&paint);return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
void Pump(){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
struct Scene {
    HWND background{};std::array<HWND,2> windows{};std::array<RECT,2> original{};
    ~Scene(){
        for(std::size_t i=0;i<2;++i)if(IsWindow(windows[i]))SetWindowPos(windows[i],HWND_TOP,original[i].left,original[i].top,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
        if(background)DestroyWindow(background);
    }
};
std::vector<std::uint8_t> Readback(ID3D11Device* device,const CapturedFrame& frame) {
    if(!frame.texture)return {};
    D3D11_TEXTURE2D_DESC desc{};frame.texture->GetDesc(&desc);
    if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM)return {};
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging)))return {};
    ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);context->CopyResource(staging.Get(),frame.texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)))return {};
    std::vector<std::uint8_t> result(static_cast<std::size_t>(frame.width)*frame.height*4);
    for(std::uint32_t y=0;y<frame.height;++y)std::copy_n(static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(y)*mapped.RowPitch,static_cast<std::size_t>(frame.width)*4,result.data()+static_cast<std::size_t>(y)*frame.width*4);
    context->Unmap(staging.Get(),0);return result;
}
ScreenRect Rect(HWND window){RECT r{};GetWindowRect(window,&r);return {r.left,r.top,r.right,r.bottom};}
bool Capture(ICaptureSession& session,CapturedFrame& frame){
    const auto until=GetTickCount64()+3000;
    while(GetTickCount64()<until){Pump();if(session.TryAcquireFrame(frame))return true;Sleep(2);}return false;
}
bool Check(ID3D11Device* device,ICaptureSession& session,FeedbackProtection& protection,
    const CaptureSourceDescriptor& source,ScreenRect monitor,std::span<const ScreenRect> expected,bool requireRaw) {
    CapturedFrame frame;std::vector<std::uint8_t> before;
    // DWM may return an initial exclusion-transition frame. Establish the owned
    // reference scene before asserting masks; this is bounded and reported.
    const auto sceneDeadline=GetTickCount64()+3000;unsigned warmup=0;
    while(GetTickCount64()<sceneDeadline) {
        if(!Capture(session,frame)){std::cerr<<"No physical capture frame: "<<session.StatusText()<<'\n';return false;}
        before=Readback(device,frame);std::size_t green=0;
        for(std::size_t at=0;at+3<before.size();at+=64)if(before[at]==61&&before[at+1]==137&&before[at+2]==19)++green;
        if(green>10)break;
        ++warmup;Pump();Sleep(2);
    }
    if(warmup)std::cout<<"reference_scene_warmup_frames="<<warmup<<'\n';
    auto original=frame;
    std::string error;if(before.empty()||!protection.Apply(frame,source,error)){std::cerr<<"Mask failed: "<<error<<'\n';return false;}
    const auto after=Readback(device,frame),unchanged=Readback(device,original);
    if(after.size()!=before.size()||before!=unchanged){std::cerr<<"Source texture mutated\n";return false;}
    const auto masks=ClipFeedbackMasks(monitor,expected,frame.width,frame.height);
    if(masks.size()<2){std::cerr<<"Both app rectangles must intersect selected monitor\n";return false;}
    std::size_t blackFailures=0,rawNonblack=0,background=0;
    for(const auto& r:masks)for(auto y=r.top;y<r.bottom;++y)for(auto x=r.left;x<r.right;++x){
        const auto at=(static_cast<std::size_t>(y)*frame.width+x)*4;
        if(after[at]||after[at+1]||after[at+2])++blackFailures;
        if(before[at]||before[at+1]||before[at+2])++rawNonblack;
    }
    for(std::uint32_t y=8;y<frame.height;y+=16)for(std::uint32_t x=8;x<frame.width;x+=16){
        if(std::any_of(masks.begin(),masks.end(),[&](const auto& r){return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;}))continue;
        const auto at=(static_cast<std::size_t>(y)*frame.width+x)*4;
        if(before[at]==61&&before[at+1]==137&&before[at+2]==19){
            if(!std::equal(before.begin()+at,before.begin()+at+4,after.begin()+at)){std::cerr<<"Background changed\n";return false;}++background;
        }
    }
    std::cout<<"captured="<<frame.width<<'x'<<frame.height<<" mask_rects="<<masks.size()
        <<" nonblack_raw_pixels="<<rawNonblack<<" black_failures="<<blackFailures
        <<" preserved_reference_points="<<background<<" source_unchanged="<<(before==unchanged)<<'\n';
    return !blackFailures&&background>10&&(!requireRaw||rawNonblack>1000);
}
}
int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"usage: FeedbackProbe dda|wgc pidA pidB fallback|affinity\n";return 2;}
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto initialized=RoInitialize(RO_INIT_MULTITHREADED);if(FAILED(initialized)){std::cerr<<"WinRT init failed\n";return 2;}
    MFStartup(MF_VERSION);
    OwnedWindows owned;owned.pids={static_cast<DWORD>(std::stoul(argv[2])),static_cast<DWORD>(std::stoul(argv[3]))};EnumWindows(FindOwned,reinterpret_cast<LPARAM>(&owned));
    if(!owned.windows[0]||!owned.windows[1]||owned.pids[0]==owned.pids[1]){std::cerr<<"Two owned app windows required\n";return 2;}
    const bool fallback=std::string(argv[4])=="fallback";
    for(const auto window:owned.windows){DWORD affinity=0;if(!GetWindowDisplayAffinity(window,&affinity)||affinity!=(fallback?0U:0x11U)){std::cerr<<"Unexpected actual app affinity="<<affinity<<'\n';return 2;}}
    const auto catalog=BuildCaptureSourceCatalog(nullptr);
    const auto found=std::find_if(catalog.begin(),catalog.end(),[](const auto& s){return s.kind==CaptureSourceKind::Monitor&&s.primary&&s.availability==CaptureAvailability::Available;});
    if(found==catalog.end()){std::cerr<<"Primary physical monitor unavailable\n";return 2;}
    const auto source=*found;MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(MonitorFromPoint({0,0},MONITOR_DEFAULTTOPRIMARY),&info))return 2;
    const auto m=info.rcMonitor;const ScreenRect monitor{m.left,m.top,m.right,m.bottom};
    WNDCLASSW wc{};wc.lpfnWndProc=ReferenceProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FPSPhysicalFeedbackReference";wc.hbrBackground=CreateSolidBrush(RGB(19,137,61));RegisterClassW(&wc);
    Scene scene;scene.windows=owned.windows;
    for(std::size_t i=0;i<2;++i)GetWindowRect(scene.windows[i],&scene.original[i]);
    scene.background=CreateWindowExW(0,wc.lpszClassName,L"Owned feedback reference",WS_POPUP|WS_VISIBLE,m.left,m.top,m.right-m.left,m.bottom-m.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!scene.background)return 2;
    SetWindowPos(scene.background,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    SetTimer(scene.background,1,16,nullptr);UpdateWindow(scene.background);for(const auto window:owned.windows)SetWindowPos(window,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    DwmFlush();Pump();
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)))return 2;
    if(std::string(argv[1])=="wgc") {
        // The product WGC backend captures windows. It does not implement monitor capture.
        auto windowSource=source;windowSource.kind=CaptureSourceKind::Window;
        windowSource.native_handle=reinterpret_cast<std::uintptr_t>(scene.background);
        WindowsGraphicsCaptureSession windowSession(windowSource,device.Get());windowSession.SetCursorCaptureEnabled(false);
        if(!windowSession.Start()){std::cerr<<"WGC window start: "<<windowSession.StatusText()<<" hr="<<std::hex<<windowSession.LastError()<<std::dec<<'\n';return 2;}
        bool ok=true;FeedbackProtection protection;std::string error;
        for(unsigned pass=0;pass<2;++pass) {
            if(pass)SetWindowPos(owned.windows[0],HWND_TOP,scene.original[0].left+32,scene.original[0].top+16,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
            DwmFlush();CapturedFrame frame;if(!Capture(windowSession,frame)){std::cerr<<"No WGC window frame\n";return 1;}
            const auto before=Readback(device.Get(),frame);
            if(!protection.Apply(frame,windowSource,error)){std::cerr<<error<<'\n';return 1;}
            const auto after=Readback(device.Get(),frame);std::size_t reference=0;
            for(std::size_t at=0;at+3<before.size();at+=4)if(before[at]==61&&before[at+1]==137&&before[at+2]==19)++reference;
            for(const auto window:owned.windows){
                auto selfSource=windowSource;selfSource.native_handle=reinterpret_cast<std::uintptr_t>(window);
                ok=!protection.Apply(frame,selfSource,error)&&error.find("viewer window")!=std::string::npos&&ok;
            }
            ok=before==after&&reference>static_cast<std::size_t>(frame.width)*frame.height/2&&ok;
            std::cout<<"WGC_window_reference_pixels="<<reference<<" unchanged="<<(before==after)<<" both_viewers_rejected="<<ok<<'\n';
        }
        windowSession.Stop();std::cout<<(ok?"PHYSICAL_FEEDBACK_PASS":"PHYSICAL_FEEDBACK_FAIL")<<'\n';return ok?0:1;
    }
    std::unique_ptr<ICaptureSession> session;
    if(std::string(argv[1])=="dda")session=std::make_unique<DesktopDuplicationCaptureSession>(source,device.Get());
    else if(std::string(argv[1])=="wgc")session=std::make_unique<WindowsGraphicsCaptureSession>(source,device.Get());
    else return 2;
    session->SetCursorCaptureEnabled(false);if(!session->Start()){std::cerr<<"Capture start failed: "<<session->StatusText()<<'\n';return 2;}
    FeedbackProtection protection;std::array<ScreenRect,2> rects{Rect(owned.windows[0]),Rect(owned.windows[1])};
    bool ok=Check(device.Get(),*session,protection,source,monitor,rects,fallback);
    // Record the old rectangle immediately before moving. The next capture tests its 250 ms history.
    CapturedFrame history;std::string error;if(!Capture(*session,history)||!protection.Apply(history,source,error))ok=false;
    const auto old=Rect(owned.windows[0]);const auto began=GetTickCount64();
    SetWindowPos(owned.windows[0],HWND_TOP,static_cast<int>(old.left)+32,static_cast<int>(old.top)+16,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
    DwmFlush();std::array<ScreenRect,3> moved{old,Rect(owned.windows[0]),Rect(owned.windows[1])};
    ok=Check(device.Get(),*session,protection,source,monitor,moved,false)&&ok;
    std::cout<<"movement_check_ms="<<GetTickCount64()-began<<" backend="<<argv[1]<<" affinity="<<argv[4]<<'\n';
    if(GetTickCount64()-began>250)ok=false;
    session->Stop();std::cout<<(ok?"PHYSICAL_FEEDBACK_PASS":"PHYSICAL_FEEDBACK_FAIL")<<'\n';return ok?0:1;
}
