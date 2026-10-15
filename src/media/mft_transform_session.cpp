#include "media/mft_transform_session.h"
#include <d3d10.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
namespace fps::media {
using Microsoft::WRL::ComPtr;
MftTransformSession::~MftTransformSession() { Stop(); }
bool MftTransformSession::Check(HRESULT hr,const char* stage) {
    if(FAILED(hr)) { hr_=hr; stage_=stage; return false; }
    return true;
}
std::string MftTransformSession::Error() const {
    char code[24]{}; std::snprintf(code,sizeof(code),"0x%08X",static_cast<unsigned>(hr_));
    return stage_+" ("+code+")";
}
void MftTransformSession::Stop() {
    if(transform_) {
        transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM,0);
        transform_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH,0);
        transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING,0);
    }
    if(activation_) activation_->ShutdownObject();
    events_.Reset(); transform_.Reset(); activation_.Reset(); manager_.Reset();
    if(mf_started_) MFShutdown();
    if(com_started_) CoUninitialize();
    mf_started_=false; com_started_=false;
    asynchronous_=false; surface_io_=false; draining_=false; drained_=false;
    input_credits_=0; output_credits_=0;
}
bool MftTransformSession::Open(const MftVideoCodecDescriptor& descriptor,
    ID3D11Device* device,bool use_surfaces) {
    Stop(); hr_=S_OK; stage_.clear();
    HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    com_started_=SUCCEEDED(co);
    if(co!=RPC_E_CHANGED_MODE && !Check(co,"CoInitializeEx")) return false;
    if(!Check(MFStartup(MF_VERSION),"MFStartup")) return false;
    mf_started_=true;
    if(!Check(ActivateMftVideoCodecDescriptor(descriptor,activation_,transform_),"Activate")) return false;
    ComPtr<IMFAttributes> attributes;
    if(SUCCEEDED(transform_->GetAttributes(&attributes))) {
        UINT32 async=0, aware=0;
        attributes->GetUINT32(MF_TRANSFORM_ASYNC,&async);
        attributes->GetUINT32(MF_SA_D3D11_AWARE,&aware);
        asynchronous_=async!=0;
        if(asynchronous_ && !Check(attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK,TRUE),"AsyncUnlock")) return false;
        if(use_surfaces && device && aware) {
            ComPtr<ID3D11DeviceContext> context; device->GetImmediateContext(&context);
            ComPtr<ID3D10Multithread> multithread;
            if(SUCCEEDED(context.As(&multithread))) multithread->SetMultithreadProtected(TRUE);
            if(!manager_.Initialize(device)) return Check(manager_.LastError(),"DXGI manager");
            if(!Check(transform_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                reinterpret_cast<ULONG_PTR>(manager_.Get())),"SetD3DManager")) return false;
            surface_io_=true;
        }
    }
    if(asynchronous_ && !Check(transform_.As(&events_),"EventGenerator")) return false;
    // Current codec wrappers support a single input/output stream with stream ID 0.
    DWORD in_count=0,out_count=0;
    if(!Check(transform_->GetStreamCount(&in_count,&out_count),"GetStreamCount")) return false;
    if(in_count!=1 || out_count!=1) return Check(E_NOTIMPL,"Multi-stream MFT");
    DWORD in_id=0,out_id=0;
    HRESULT ids=transform_->GetStreamIDs(1,&in_id,1,&out_id);
    if(ids!=E_NOTIMPL && (!Check(ids,"GetStreamIDs") || in_id!=0 || out_id!=0))
        return Check(E_NOTIMPL,"Nonzero stream IDs");
    return true;
}
bool MftTransformSession::SetTypes(IMFMediaType* input,IMFMediaType* output,bool encoder) {
    if(!input || !output) return Check(E_INVALIDARG,"Types");
    output->GetGUID(MF_MT_SUBTYPE,&output_subtype_);
    if(encoder) {
        return Check(transform_->SetOutputType(0,output,0),"SetEncoderOutput") &&
            Check(transform_->SetInputType(0,input,0),"SetEncoderInput");
    }
    return Check(transform_->SetInputType(0,input,0),"SetDecoderInput") &&
        Check(transform_->SetOutputType(0,output,0),"SetDecoderOutput");
}
bool MftTransformSession::Start() {
    return Check(transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,0),"BeginStreaming") &&
        Check(transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM,0),"StartStream");
}
bool MftTransformSession::Events() {
    if(!asynchronous_) return true;
    for(unsigned n=0;n<512;++n) {
        ComPtr<IMFMediaEvent> event;
        HRESULT hr=events_->GetEvent(MF_EVENT_FLAG_NO_WAIT,&event);
        if(hr==MF_E_NO_EVENTS_AVAILABLE) return true;
        if(!Check(hr,"GetEvent")) return false;
        HRESULT status=S_OK; MediaEventType type=MEUnknown;
        if(!Check(event->GetStatus(&status),"EventStatus") || !Check(status,"MFT event") ||
            !Check(event->GetType(&type),"EventType")) return false;
        if(GetEnvironmentVariableW(L"FPS_CODEC_TRACE",nullptr,0)) std::cerr << "event="<<static_cast<int>(type)<<" credits="<<input_credits_<<","<<output_credits_<<"\\n";
        if(type==METransformNeedInput) ++input_credits_;
        else if(type==METransformHaveOutput) ++output_credits_;
        else if(type==METransformDrainComplete) drained_=true;
        else if(type==MEError) return Check(E_FAIL,"MEError");
        if(input_credits_>64 || output_credits_>64) return Check(E_UNEXPECTED,"MFT event bounds");
    }
    return Check(E_UNEXPECTED,"MFT event flood");
}
bool MftTransformSession::SelectOutput() {
    for(DWORD n=0;n<128;++n) {
        ComPtr<IMFMediaType> type;
        HRESULT hr=transform_->GetOutputAvailableType(0,n,&type);
        if(hr==MF_E_NO_MORE_TYPES) break;
        if(!Check(hr,"GetOutputAvailableType")) return false;
        GUID subtype{};
        if(SUCCEEDED(type->GetGUID(MF_MT_SUBTYPE,&subtype)) && IsEqualGUID(subtype,output_subtype_) &&
            SUCCEEDED(transform_->SetOutputType(0,type.Get(),0))) return true;
    }
    return Check(MF_E_INVALIDMEDIATYPE,"NegotiateOutput");
}
bool MftTransformSession::Output(std::vector<ComPtr<IMFSample>>& samples) {
    for(unsigned attempt=0;attempt<4;++attempt) {
        MFT_OUTPUT_STREAM_INFO info{};
        if(!Check(transform_->GetOutputStreamInfo(0,&info),"GetOutputStreamInfo")) return false;
        ComPtr<IMFSample> provided;
        if((info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)==0) {
            ComPtr<IMFMediaBuffer> buffer;
            if(!Check(MFCreateSample(&provided),"OutputSample") ||
                !Check(MFCreateAlignedMemoryBuffer(std::max<DWORD>(info.cbSize,1),
                    info.cbAlignment ? info.cbAlignment-1 : 0,&buffer),"OutputBuffer") ||
                !Check(provided->AddBuffer(buffer.Get()),"OutputAddBuffer")) return false;
        }
        MFT_OUTPUT_DATA_BUFFER output{}; output.pSample=provided.Get();
        DWORD status=0; HRESULT hr=transform_->ProcessOutput(0,1,&output,&status);
        if(GetEnvironmentVariableW(L"FPS_CODEC_TRACE",nullptr,0)) std::cerr<<"ProcessOutput hr="<<std::hex<<hr<<" flags="<<info.dwFlags<<" size="<<info.cbSize<<" outputstatus="<<output.dwStatus<<" streamstatus="<<status<<std::dec<<"\\n";
        if(output.pEvents) output.pEvents->Release();
        ComPtr<IMFSample> produced;
        if(output.pSample && output.pSample!=provided.Get()) produced.Attach(output.pSample);
        else produced=provided;
        if(hr==MF_E_TRANSFORM_STREAM_CHANGE) {
            if(!SelectOutput()) return false;
            // Async stream-change consumes its HaveOutput credit. The next
            // ProcessOutput must wait for a new event, including on Intel MFT.
            if(asynchronous_) return true;
            continue;
        }
        if(hr==MF_E_TRANSFORM_NEED_MORE_INPUT && !asynchronous_) { hr_=hr; return false; }
        if(!Check(hr,"ProcessOutput")) return false;
        if(produced && (output.dwStatus & MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE)==0)
            samples.push_back(std::move(produced));
        hr_=S_OK;
        return true;
    }
    return Check(MF_E_TRANSFORM_STREAM_CHANGE,"RepeatedStreamChange");
}
bool MftTransformSession::Poll(std::vector<ComPtr<IMFSample>>& samples) {
    if(!transform_) return Check(E_UNEXPECTED,"Poll stopped");
    if(asynchronous_) {
        for(unsigned pass=0;pass<64;++pass) {
            if(!Events()) return false;
            if(output_credits_==0) return true;
            while(output_credits_) { --output_credits_; if(!Output(samples)) return false; }
        }
        return Check(E_UNEXPECTED,"Output flood");
    }
    for(unsigned n=0;n<64;++n) {
        if(Output(samples)) continue;
        if(hr_==MF_E_TRANSFORM_NEED_MORE_INPUT) { hr_=S_OK; return true; }
        return false;
    }
    return Check(E_UNEXPECTED,"Sync output flood");
}
bool MftTransformSession::Submit(IMFSample* sample,std::vector<ComPtr<IMFSample>>& output) {
    if(draining_ || drained_) return Check(E_UNEXPECTED,"Submit after drain");
    const ULONGLONG deadline=GetTickCount64()+2000;
    do {
        if(!Poll(output)) return false;
        if(!asynchronous_ || input_credits_>0) {
            HRESULT hr=transform_->ProcessInput(0,sample,0);
            if(GetEnvironmentVariableW(L"FPS_CODEC_TRACE",nullptr,0)) std::cerr<<"ProcessInput hr="<<std::hex<<hr<<std::dec<<" credits="<<input_credits_<<","<<output_credits_<<"\\n";
            if(SUCCEEDED(hr)) {
                if(asynchronous_) --input_credits_;
                return Poll(output);
            }
            if(hr!=MF_E_NOTACCEPTING) return Check(hr,"ProcessInput");
            if(asynchronous_) return Check(hr,"Input credit rejected");
        }
        Sleep(1);
    } while(GetTickCount64()<deadline);
    return Check(HRESULT_FROM_WIN32(ERROR_TIMEOUT),"Input backpressure timeout");
}
bool MftTransformSession::Drain(std::vector<ComPtr<IMFSample>>& output) {
    if(drained_) return true;
    if(!Check(transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM,0),"EndOfStream") ||
        !Check(transform_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN,0),"Drain")) return false;
    draining_=true; input_credits_=0;
    if(!asynchronous_) { if(!Poll(output)) return false; drained_=true; return true; }
    const ULONGLONG deadline=GetTickCount64()+3000;
    while(GetTickCount64()<deadline) {
        if(!Poll(output)) return false;
        if(drained_) return true;
        Sleep(1);
    }
    return Check(HRESULT_FROM_WIN32(ERROR_TIMEOUT),"Drain timeout");
}
bool MftTransformSession::Flush() {
    if(!Check(transform_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH,0),"Flush")) return false;
    input_credits_=0; output_credits_=0; draining_=false; drained_=false;
    if(asynchronous_) {
        ComPtr<IMFMediaEvent> ignored;
        while(events_->GetEvent(MF_EVENT_FLAG_NO_WAIT,&ignored)==S_OK) ignored.Reset();
    }
    return Check(transform_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM,0),"RestartStream");
}
}
