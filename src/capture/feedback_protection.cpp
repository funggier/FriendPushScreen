#include "capture/feedback_protection.h"
#include <algorithm>
#include <array>
#include <cwchar>
#include <limits>
namespace fps::capture {
namespace {
bool Geometry(const CapturedFrame& f,std::span<const PixelRect> masks,std::string& error) {
    if(!f.IsValid()||f.width>4096||f.height>4096||static_cast<std::uint64_t>(f.width)*f.height>3840ULL*2160) {
        error="Feedback protection: unsupported capture geometry";return false;
    }
    for(auto r:masks)if(r.left>=r.right||r.top>=r.bottom||r.right>f.width||r.bottom>f.height) {
        error="Feedback protection: invalid mask rectangle";return false;
    }return true;
}
struct MonitorSearch {std::wstring name;HMONITOR monitor{};};
BOOL CALLBACK MatchMonitor(HMONITOR monitor,HDC,LPRECT,LPARAM parameter) {
    auto& search=*reinterpret_cast<MonitorSearch*>(parameter);MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(GetMonitorInfoW(monitor,&info)&&search.name==info.szDevice){search.monitor=monitor;return FALSE;}return TRUE;
}
HMONITOR FindCaptureMonitor(const CaptureSourceDescriptor& source) {
    if(source.native_id.empty()||source.native_id.size()>255)return nullptr;
    wchar_t name[256]{};
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.native_id.c_str(),-1,name,256))return nullptr;
    MonitorSearch search{name};EnumDisplayMonitors(nullptr,nullptr,MatchMonitor,reinterpret_cast<LPARAM>(&search));return search.monitor;
}
BOOL CALLBACK ViewerRect(HWND window,LPARAM parameter) {
    if(!IsWindowVisible(window)||IsIconic(window)||!IsFriendPushViewer(window))return TRUE;
    RECT rect{};if(GetWindowRect(window,&rect)&&rect.right>rect.left&&rect.bottom>rect.top) {
        auto& result=*reinterpret_cast<std::vector<ScreenRect>*>(parameter);
        if(result.size()<32)result.push_back({rect.left,rect.top,rect.right,rect.bottom});
    }return TRUE;
}
}
std::vector<PixelRect> ClipFeedbackMasks(ScreenRect capture,std::span<const ScreenRect> windows,std::uint32_t width,std::uint32_t height) {
    std::vector<PixelRect> result;
    if(capture.left<INT32_MIN||capture.top<INT32_MIN||capture.right>INT32_MAX||capture.bottom>INT32_MAX) return result;
    const auto cw=capture.right-capture.left,ch=capture.bottom-capture.top;
    if(cw<=0||ch<=0||cw>1000000||ch>1000000||!width||!height||width>4096||height>4096)return result;
    for(auto r:windows) {
        const auto left=std::max(capture.left,r.left),right=std::min(capture.right,r.right);
        const auto top=std::max(capture.top,r.top),bottom=std::min(capture.bottom,r.bottom);
        if(left>=right||top>=bottom)continue;
        PixelRect clipped{static_cast<std::uint32_t>((left-capture.left)*width/cw),
            static_cast<std::uint32_t>((top-capture.top)*height/ch),
            static_cast<std::uint32_t>(((right-capture.left)*width+cw-1)/cw),
            static_cast<std::uint32_t>(((bottom-capture.top)*height+ch-1)/ch)};
        if(std::find(result.begin(),result.end(),clipped)==result.end())result.push_back(clipped);
    }return result;
}
bool MaskFeedbackPixels(CapturedFrame& frame,std::span<const PixelRect> masks,std::string& error) {
    if(!Geometry(frame,masks,error))return false;
    const auto pitch=frame.row_pitch?frame.row_pitch:frame.width*4;
    if(pitch<frame.width*4||static_cast<std::uint64_t>(pitch)*frame.height>frame.bgra.size()) {
        error="Feedback protection: truncated BGRA pixels";return false;
    }
    for(auto r:masks)for(auto y=r.top;y<r.bottom;++y)for(auto x=r.left;x<r.right;++x) {
        const auto offset=static_cast<std::size_t>(y)*pitch+x*4;
        frame.bgra[offset]=0;frame.bgra[offset+1]=0;frame.bgra[offset+2]=0;frame.bgra[offset+3]=255;
    }
    if(frame.cursor.visible)for(auto r:masks) {
        const auto left=static_cast<std::int64_t>(frame.cursor.x),top=static_cast<std::int64_t>(frame.cursor.y);
        if(left<r.right&&left+frame.cursor.width>r.left&&top<r.bottom&&top+frame.cursor.height>r.top){frame.cursor={};break;}
    }
    return true;
}
bool IsFriendPushViewer(HWND window) {
    if(!window)return false;window=GetAncestor(window,GA_ROOT);
    std::array<wchar_t,128> name{};
    return GetClassNameW(window,name.data(),static_cast<int>(name.size()))&&std::wcscmp(name.data(),L"FriendPushScreenMainWindow")==0;
}
bool ConfigureCaptureExclusion(HWND window) {
    constexpr DWORD exclude=0x11;
    DWORD actual=0;
    return SetWindowDisplayAffinity(window,exclude)&&GetWindowDisplayAffinity(window,&actual)&&actual==exclude;
}
bool FeedbackProtection::ApplyMasks(CapturedFrame& frame,std::span<const PixelRect> masks,std::string& error) {
    if(!Geometry(frame,masks,error))return false;
    if(masks.empty())return true;
    if(!frame.HasGpuTexture())return MaskFeedbackPixels(frame,masks,error);
    Microsoft::WRL::ComPtr<ID3D11Device> device;frame.texture->GetDevice(&device);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
    D3D11_TEXTURE2D_DESC desc{};frame.texture->GetDesc(&desc);
    if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||desc.Width<frame.width||desc.Height<frame.height||desc.SampleDesc.Count!=1) {
        error="Feedback protection: unsupported GPU texture";return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Device> previous;if(masked_)masked_->GetDevice(&previous);
    if(!masked_||width_!=frame.width||height_!=frame.height||previous.Get()!=device.Get()) {
        masked_.Reset();black_.Reset();width_=frame.width;height_=frame.height;
        desc.Width=width_;desc.Height=height_;desc.MipLevels=1;desc.ArraySize=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;desc.CPUAccessFlags=0;desc.MiscFlags=0;
        std::vector<std::uint8_t> black(static_cast<std::size_t>(width_)*height_*4);
        for(std::size_t i=3;i<black.size();i+=4)black[i]=255;
        D3D11_SUBRESOURCE_DATA initial{black.data(),width_*4,0};
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&masked_))||FAILED(device->CreateTexture2D(&desc,&initial,&black_))) {
            error="Feedback protection: mask texture creation failed";return false;
        }
    }
    const D3D11_BOX full{0,0,0,frame.width,frame.height,1};
    context->CopySubresourceRegion(masked_.Get(),0,0,0,0,frame.texture.Get(),0,&full);
    for(auto r:masks) {
        const D3D11_BOX box{0,0,0,r.right-r.left,r.bottom-r.top,1};
        context->CopySubresourceRegion(masked_.Get(),0,r.left,r.top,0,black_.Get(),0,&box);
    }
    frame.texture=masked_;frame.bgra.clear();frame.row_pitch=0;
    if(frame.cursor.visible)for(auto r:masks) {
        const auto left=static_cast<std::int64_t>(frame.cursor.x),top=static_cast<std::int64_t>(frame.cursor.y);
        if(left<r.right&&left+frame.cursor.width>r.left&&top<r.bottom&&top+frame.cursor.height>r.top){frame.cursor={};break;}
    }
    return true;
}
bool FeedbackProtection::Apply(CapturedFrame& frame,const CaptureSourceDescriptor& source,std::string& error) {
    if(source.kind==CaptureSourceKind::Window&&IsFriendPushViewer(reinterpret_cast<HWND>(source.native_handle))) {
        error="Cannot share a FriendPushScreen viewer window: select another window or monitor";return false;
    }
    if(source.kind!=CaptureSourceKind::Monitor)return true;
    MONITORINFO info{sizeof(info)};
    if(!GetMonitorInfoW(FindCaptureMonitor(source),&info)) {
        error="Feedback protection: capture monitor no longer exists";return false;
    }
    std::vector<ScreenRect> current;EnumWindows(ViewerRect,reinterpret_cast<LPARAM>(&current));
    const auto now=GetTickCount64();
    std::erase_if(history_,[&](const auto& entry){return now-entry.first>250;});
    for(auto rect:current) {
        const bool known=std::any_of(history_.begin(),history_.end(),[&](auto& h){return h.second==rect;});
        if(!known)history_.push_back({now,rect});
        else for(auto& h:history_)if(h.second==rect)h.first=now;
    }
    if(history_.size()>128){error="Feedback protection: viewer movement history limit";return false;}
    std::vector<ScreenRect> all;for(auto& h:history_)all.push_back(h.second);
    const auto& r=info.rcMonitor;
    const auto masks=ClipFeedbackMasks({r.left,r.top,r.right,r.bottom},all,frame.width,frame.height);
    return ApplyMasks(frame,masks,error);
}
}
