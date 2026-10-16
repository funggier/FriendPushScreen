#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <roapi.h>
#include <psapi.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include "capture/capture_source_catalog.h"
#include "capture/desktop_duplication_capture.h"
#include "capture/windows_graphics_capture.h"
#include "capture/media_foundation_capture.h"
#include "graphics/preview_renderer.h"
#include "media/video_local_loop.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <future>
using namespace fps;
namespace {
graphics::PreviewRenderer* preview=nullptr;
bool preview_ready=false;
unsigned fixture_frame=0,fixture_variant=0;
std::string fixture_label;
LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_CLOSE){PostQuitMessage(0);return 0;}
    if(msg==WM_SIZE && preview && preview_ready)preview->Resize(LOWORD(lp),HIWORD(lp));
    return DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK FixtureProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT area{};GetClientRect(hwnd,&area);
        HBRUSH background=CreateSolidBrush(RGB(24,36,48));FillRect(dc,&area,background);DeleteObject(background);
        const COLORREF colors[]{RGB(255,0,0),RGB(0,255,0),RGB(0,0,255),RGB(255,255,255)};
        for(int i=0;i<4;++i) {
            RECT stripe{i*480,0,(i+1)*480,120};HBRUSH brush=CreateSolidBrush(colors[(i+fixture_variant)%4]);FillRect(dc,&stripe,brush);DeleteObject(brush);
        }
        RECT moving{static_cast<LONG>((fixture_frame*12)%1600),400,static_cast<LONG>((fixture_frame*12)%1600+320),720};
        HBRUSH brush=CreateSolidBrush(RGB(220,170,80));FillRect(dc,&moving,brush);DeleteObject(brush);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
        std::wstring text=L"FPS-006 real WGC source 1920x1080 | "+std::wstring(fixture_label.begin(),fixture_label.end())+L" | frame "+std::to_wstring(fixture_frame);
        TextOutW(dc,24,160,text.c_str(),static_cast<int>(text.size()));
        // The border marks the entire source extent so cropping is visible.
        HBRUSH border=CreateSolidBrush(RGB(255,255,255));FrameRect(dc,&area,border);DeleteObject(border);
        EndPaint(hwnd,&ps);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
template<class F> bool Timed(const char* stage,F&& action) {
    const auto start=std::chrono::steady_clock::now();
    const bool result=action();
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(ms>=100)std::cout<<"STALL stage="<<stage<<" ms="<<ms<<"\n"<<std::flush;
    return result;
}
ULONGLONG FileTimeValue(const FILETIME& time) {return (static_cast<ULONGLONG>(time.dwHighDateTime)<<32)|time.dwLowDateTime;}
ULONGLONG CpuTime() {FILETIME a{},b{},c{},d{};GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d);return FileTimeValue(c)+FileTimeValue(d);}
}
int main(int argc,char** argv) {
    std::string source_kind="window";double seconds=10;bool software=false;bool require_1080=false;bool fixture_only=false;
    for(int n=1;n<argc;++n) {
        const std::string arg=argv[n];
        if(arg=="--source" && n+1<argc)source_kind=argv[++n];
        else if(arg=="--seconds" && n+1<argc)seconds=std::stod(argv[++n]);
        else if(arg=="--software")software=true;
        else if(arg=="--fixture-only")fixture_only=true;
        else if(arg=="--fixture-label"&&n+1<argc)fixture_label=argv[++n];
        else if(arg=="--fixture-variant"&&n+1<argc)fixture_variant=static_cast<unsigned>(std::stoul(argv[++n]))%4;
        else if(arg=="--require-1080p60")require_1080=true;
        else {std::cerr<<"Unknown argument "<<arg<<"\n";return 2;}
    }
    struct TimerResolution { TimerResolution(){timeBeginPeriod(1);} ~TimerResolution(){timeEndPeriod(1);} } timer_resolution;
    const HRESULT ro=RoInitialize(RO_INIT_MULTITHREADED);
    if(FAILED(ro) && ro!=RPC_E_CHANGED_MODE)return 2;
    HINSTANCE instance=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=WindowProc;wc.lpszClassName=L"FPS006CaptureLoopPreview";RegisterClassW(&wc);
    WNDCLASSW fixture_class=wc;fixture_class.lpfnWndProc=FixtureProc;fixture_class.lpszClassName=L"FPS006MovingFixture";RegisterClassW(&fixture_class);
    HWND fixture=nullptr;
    std::jthread fixture_thread;
    if(source_kind=="window" || source_kind=="monitor") {
        std::promise<HWND> created;
        auto ready=created.get_future();
        // The capture producer must keep its own clock while encode/decode/present runs.
        fixture_thread=std::jthread([instance,signal=std::move(created)](std::stop_token stop) mutable {
            const auto title=fixture_label.empty()?std::wstring(L"FPS-006 1080p moving source"):std::wstring(fixture_label.begin(),fixture_label.end());
            HWND own=CreateWindowExW(0,L"FPS006MovingFixture",title.c_str(),
                WS_POPUP|WS_VISIBLE,0,0,1920,1080,nullptr,nullptr,instance,nullptr);
            if(own){UpdateWindow(own);DwmFlush();}
            signal.set_value(own);
            auto deadline=std::chrono::steady_clock::now();
            while(own && !stop.stop_requested()) {
                MSG message{};
                while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                if(std::chrono::steady_clock::now()>=deadline){
                    ++fixture_frame;InvalidateRect(own,nullptr,FALSE);UpdateWindow(own);
                    deadline+=std::chrono::microseconds(16667);
                }
                Sleep(1);
            }
            if(own)DestroyWindow(own);
        });
        fixture=ready.get();
        if(!fixture)return 2;
    }
    if(fixture_only) {
        if(!fixture)return 2;
        std::cout<<"FIXTURE_READY hwnd="<<reinterpret_cast<std::uintptr_t>(fixture)<<"\n"<<std::flush;
        const auto until=std::chrono::steady_clock::now()+std::chrono::duration<double>(seconds);
        while(std::chrono::steady_clock::now()<until && IsWindow(fixture))Sleep(20);
        fixture_thread.request_stop();fixture_thread.join();
        if(SUCCEEDED(ro))RoUninitialize();
        return 0;
    }
    HWND window=CreateWindowExW(WS_EX_NOACTIVATE,wc.lpszClassName,L"FPS-006 H.264 DECODED local loop",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,1600,850,800,520,nullptr,nullptr,instance,nullptr);
    int result=1;
    {
        graphics::PreviewRenderer renderer;preview=&renderer;
        if(!renderer.Initialize(window)){std::cerr<<"Renderer init failed\n";return 2;}
        preview_ready=true;RECT client{};GetClientRect(window,&client);renderer.Resize(client.right,client.bottom);
        auto catalog=capture::BuildCaptureSourceCatalog(window);
        for(const auto& item:catalog)std::cout<<"source "<<item.id<<" "<<item.display_name<<" "<<item.width<<"x"<<item.height<<"\n";
        std::unique_ptr<capture::ICaptureSession> capture_session;
        capture::CaptureSourceDescriptor selected;
        if(fixture && source_kind=="window") {
            selected.kind=capture::CaptureSourceKind::Window;selected.native_handle=reinterpret_cast<std::uintptr_t>(fixture);
            selected.id=capture::MakeWindowSourceId(selected.native_handle);selected.display_name="FPS-006 moving fixture";
            selected.width=1920;selected.height=1080;
            capture_session=std::make_unique<capture::WindowsGraphicsCaptureSession>(selected,renderer.Device());
        } else {
            auto kind=source_kind=="device"?capture::CaptureSourceKind::VideoDevice:capture::CaptureSourceKind::Monitor;
            auto found=std::find_if(catalog.begin(),catalog.end(),[&](const auto& s){return s.kind==kind && s.availability==capture::CaptureAvailability::Available;});
            if(kind==capture::CaptureSourceKind::Monitor) {
                auto fullhd=std::find_if(catalog.begin(),catalog.end(),[&](const auto& s){
                    return s.kind==kind && s.width==1920 && s.height==1080 &&
                        s.availability==capture::CaptureAvailability::Available;});
                if(fullhd!=catalog.end())found=fullhd;
            }
            if(found==catalog.end()){std::cerr<<"Requested capture source unavailable\n";return 2;}
            selected=*found;
            if(fixture) {
                wchar_t device_name[64]{};
                MultiByteToWideChar(CP_UTF8,0,selected.native_id.c_str(),-1,device_name,64);
                DEVMODEW mode{};mode.dmSize=sizeof(mode);
                if(EnumDisplaySettingsExW(device_name,ENUM_CURRENT_SETTINGS,&mode,0))
                    SetWindowPos(fixture,HWND_TOP,mode.dmPosition.x,mode.dmPosition.y,
                        static_cast<int>(selected.width),static_cast<int>(selected.height),SWP_NOACTIVATE|SWP_SHOWWINDOW);
                UpdateWindow(fixture);DwmFlush();
            }
            if(kind==capture::CaptureSourceKind::VideoDevice)
                capture_session=std::make_unique<capture::MediaFoundationCaptureSession>(selected);
            else capture_session=std::make_unique<capture::DesktopDuplicationCaptureSession>(selected,renderer.Device());
        }
        capture_session->SetCursorCaptureEnabled(false);
        if(!capture_session->Start()){std::cerr<<"Capture start "<<capture_session->StatusText()<<" hr="<<std::hex<<capture_session->LastError()<<"\n";return 2;}
        std::cout<<"PID="<<GetCurrentProcessId()<<" selected="<<selected.display_name<<" backend="<<source_kind<<"\n"<<std::flush;
        media::VideoLocalLoop loop;bool initialized=false,failed=false,quit=false;
        auto start=std::chrono::steady_clock::now();auto last_progress=start;
        auto stream_start=start;bool first_frame=true;
        ULONGLONG cpu_start=CpuTime();SIZE_T max_private=0,min_private=MAXSIZE_T,max_ws=0;
        std::uint64_t source_gaps=0,last_source_seq=0;
        unsigned width=0,height=0;
        const auto sink=[&](const capture::CapturedFrame& frame){return Timed("present",[&]{return renderer.RenderFrame(frame);});};
        while(!quit) {
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
                if(message.message==WM_QUIT){quit=true;break;}
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            auto now=std::chrono::steady_clock::now();
            const auto rendered_before=loop.Metrics().rendered_frames;
            capture::CapturedFrame frame;
            const bool acquired=Timed("capture",[&]{return capture_session->TryAcquireFrame(frame);}) && frame.IsValid();
            if(acquired) {
                if(last_source_seq && frame.sequence>last_source_seq+1)source_gaps+=frame.sequence-last_source_seq-1;
                last_source_seq=frame.sequence;
                if(first_frame) {
                    stream_start=std::chrono::steady_clock::now();cpu_start=CpuTime();first_frame=false;
                    width=frame.width;height=frame.height;
                    media::VideoCodecConfig config;config.width=(width+1)&~1U;config.height=(height+1)&~1U;
                    if(!loop.Initialize(renderer.Device(),config,software?media::VideoBackendPolicy::ForceSoftware:
                        media::VideoBackendPolicy::RequireHardware)) {failed=true;break;}
                    initialized=true;
                    std::cout<<"ENCODER "<<loop.EncoderInfo().candidate.name<<" surfaces="<<loop.EncoderInfo().surface_io
                        <<" async="<<loop.EncoderInfo().asynchronous<<"\nDECODER "<<loop.DecoderInfo().candidate.name
                        <<" surfaces="<<loop.DecoderInfo().surface_io<<"\n"<<std::flush;
                }
                if(!Timed("push",[&]{return loop.Push(frame,sink);})){failed=true;break;}
            }
            if(initialized && !Timed("poll",[&]{return loop.Poll(sink);})){failed=true;break;}
            if(capture_session->State()==capture::CaptureSessionState::Error ||
                capture_session->State()==capture::CaptureSessionState::SourceLost) {failed=true;break;}
            now=std::chrono::steady_clock::now();
            if(now-last_progress>=std::chrono::seconds(1)) {
                PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
                if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory))) {
                    max_private=std::max(max_private,memory.PrivateUsage);min_private=std::min(min_private,memory.PrivateUsage);
                    max_ws=std::max(max_ws,memory.WorkingSetSize);
                }
                const auto& m=loop.Metrics();
                std::cout<<"PROGRESS seconds="<<std::chrono::duration<double>(now-stream_start).count()
                    <<" capture="<<m.capture_frames<<" encoded="<<m.encoded_access_units<<" decoded="<<m.decoded_frames
                    <<" rendered="<<m.rendered_frames<<" queue="<<m.current_queue_depth<<" private="<<memory.PrivateUsage<<"\n"<<std::flush;
                last_progress=now;
            }
            if(!first_frame && std::chrono::duration<double>(now-stream_start).count()>=seconds)break;
            if(first_frame && now-start>std::chrono::seconds(10)){failed=true;break;}
            // Present already paces rendered frames; only wait when both capture and codec are idle.
            if(!acquired && loop.Metrics().rendered_frames==rendered_before)Sleep(1);
        }
        capture_session->Stop();
        if(initialized && !failed && !loop.Drain(sink))failed=true;
        const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-stream_start).count();
        const auto& m=loop.Metrics();const auto tail=loop.TailLatency();
        SYSTEM_INFO system{};GetSystemInfo(&system);
        const double cpu=static_cast<double>(CpuTime()-cpu_start)/10000000.0/elapsed/system.dwNumberOfProcessors*100;
        const double fps=static_cast<double>(m.rendered_frames)/elapsed;
        std::cout<<"RESULT {\"width\":"<<width<<",\"height\":"<<height<<",\"seconds\":"<<elapsed
            <<",\"capture\":"<<m.capture_frames<<",\"submitted\":"<<m.encoder_submissions<<",\"encoded\":"<<m.encoded_access_units
            <<",\"decoded\":"<<m.decoded_frames<<",\"rendered\":"<<m.rendered_frames<<",\"fps\":"<<fps
            <<",\"source_sequence_gaps\":"<<source_gaps<<",\"drops\":"<<m.dropped_frames<<",\"recovery_resets\":"<<m.recovery_resets
            <<",\"queue_max\":"<<m.max_queue_depth<<",\"queue_final\":"<<m.current_queue_depth
            <<",\"encode_avg_ms\":"<<m.encode_latency.Average100ns()/10000.0<<",\"encode_p95_ms\":"<<tail.encode_p95_ms
            <<",\"encode_max_ms\":"<<m.encode_latency.max_100ns/10000.0
            <<",\"decode_avg_ms\":"<<m.decode_latency.Average100ns()/10000.0<<",\"decode_p95_ms\":"<<tail.decode_p95_ms
            <<",\"decode_max_ms\":"<<m.decode_latency.max_100ns/10000.0
            <<",\"e2e_avg_ms\":"<<m.end_to_end_latency.Average100ns()/10000.0<<",\"e2e_p95_ms\":"<<tail.end_to_end_p95_ms
            <<",\"e2e_max_ms\":"<<m.end_to_end_latency.max_100ns/10000.0
            <<",\"cpu_percent_all_cores\":"<<cpu<<",\"private_min\":"<<min_private<<",\"private_max\":"<<max_private
            <<",\"working_set_max\":"<<max_ws<<",\"encoder_gpu\":"<<loop.EncoderInfo().gpu_frames
            <<",\"decoder_gpu\":"<<loop.DecoderInfo().gpu_frames<<",\"cpu_capture_frames\":"<<loop.CpuInputFrames()
            <<",\"padded_frames\":"<<loop.PaddedFrames()<<",\"encoded_bytes\":"<<loop.EncodedBytes()
            <<",\"bitrate_mbps\":"<<loop.EncodedBytes()*8.0/elapsed/1000000.0<<"}\n";
        if(failed)std::cerr<<"LOOP ERROR "<<loop.Error()<<" capture="<<capture_session->StatusText()<<"\n";
        bool pass=!failed && initialized && m.rendered_frames>0 && m.encoded_access_units==m.encoder_submissions &&
            m.decoded_frames==m.rendered_frames && m.dropped_frames==0 && m.current_queue_depth==0 && elapsed>=seconds*0.95;
        if(require_1080)pass=pass && width==1920 && height==1080 && fps>=59.0;
        std::cout<<(pass?"CAPTURE_LOOP_PASS":"CAPTURE_LOOP_FAIL")<<"\n"<<std::flush;
        result=pass?0:1;
        preview_ready=false;preview=nullptr;
    }
    if(window)DestroyWindow(window);
    fixture_thread.request_stop();
    if(fixture_thread.joinable())fixture_thread.join();
    if(SUCCEEDED(ro))RoUninitialize();
    return result;
}
