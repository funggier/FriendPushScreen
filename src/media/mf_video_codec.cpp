#include "media/mf_video_codec.h"
#include "media/mft_transform_session.h"
#include "media/mft_video_media_type.h"
#include "graphics/d3d11_video_converter.h"
#include <codecapi.h>
#include <icodecapi.h>
#include <mferror.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <map>
namespace fps::media {
using Microsoft::WRL::ComPtr;
namespace {
bool CodecValue(IMFTransform* transform,const GUID& key,ULONG value,bool boolean=false) {
    ComPtr<ICodecAPI> api;
    if(FAILED(transform->QueryInterface(IID_PPV_ARGS(&api)))) return false;
    VARIANT v{}; v.vt=static_cast<VARTYPE>(boolean ? VT_BOOL : VT_UI4);
    if(boolean) v.boolVal=value ? VARIANT_TRUE : VARIANT_FALSE; else v.ulVal=value;
    return SUCCEEDED(api->SetValue(&key,&v));
}
bool LowLatency(IMFTransform* transform) {
    ComPtr<IMFAttributes> attributes;
    if(SUCCEEDED(transform->GetAttributes(&attributes))) attributes->SetUINT32(MF_LOW_LATENCY,TRUE);
    return CodecValue(transform,CODECAPI_AVLowLatencyMode,TRUE,true);
}
void ColorType(IMFMediaType* type) {
    type->SetUINT32(MF_MT_YUV_MATRIX,MFVideoTransferMatrix_BT709);
    type->SetUINT32(MF_MT_VIDEO_PRIMARIES,MFVideoPrimaries_BT709);
    type->SetUINT32(MF_MT_TRANSFER_FUNCTION,MFVideoTransFunc_709);
    type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE,MFNominalRange_16_235);
}
std::vector<MftVideoCodecDescriptor> Candidates(VideoCodecDirection direction,VideoBackendPolicy policy) {
    auto list=EnumerateMftVideoCodecDescriptors(VideoCodecKind::H264,direction);
    std::erase_if(list,[&](const auto& d) {
        return (policy==VideoBackendPolicy::ForceSoftware && d.candidate.classification!=VideoBackendClassification::Software) ||
            (policy==VideoBackendPolicy::RequireHardware && d.candidate.classification!=VideoBackendClassification::Hardware);
    });
    std::stable_sort(list.begin(),list.end(),[](const auto& a,const auto& b) {
        return a.candidate.classification==VideoBackendClassification::Hardware &&
            b.candidate.classification!=VideoBackendClassification::Hardware;
    });
    return list;
}
bool MemorySample(const void* bytes,DWORD size,ComPtr<IMFSample>& sample) {
    ComPtr<IMFMediaBuffer> buffer;
    if(FAILED(MFCreateMemoryBuffer(size,&buffer))) return false;
    BYTE* data=nullptr;
    if(FAILED(buffer->Lock(&data,nullptr,nullptr))) return false;
    std::memcpy(data,bytes,size);
    buffer->Unlock();
    return SUCCEEDED(buffer->SetCurrentLength(size)) && SUCCEEDED(MFCreateSample(&sample)) &&
        SUCCEEDED(sample->AddBuffer(buffer.Get()));
}
bool Idr(const std::vector<std::uint8_t>& bytes) {
    for(size_t n=0;n+3<bytes.size();++n) {
        if(bytes[n]==0 && bytes[n+1]==0 && bytes[n+2]==1 && (bytes[n+3]&31)==5) return true;
    }
    return false;
}
}
struct MftVideoEncoder::Impl {
    MftTransformSession session;
    VideoCodecConfig config;
    VideoCodecRuntimeInfo info;
    graphics::D3D11VideoConverter converter;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> readback;
    struct Slot { ComPtr<ID3D11Texture2D> texture; bool busy{false}; };
    std::array<Slot,8> slots;
    struct Pending { NormalizedVideoTiming timing; size_t slot; ComPtr<IMFSample> lease; };
    std::map<LONGLONG,Pending> pending;
    bool running{false};
    bool Collect(std::vector<ComPtr<IMFSample>>& samples,std::vector<EncodedAccessUnit>& units) {
        for(auto& sample:samples) {
            LONGLONG pts=0,duration=0;
            if(!session.Check(sample->GetSampleTime(&pts),"Encoded time")) return false;
            auto found=pending.find(pts);
            if(found==pending.end()) return session.Check(E_UNEXPECTED,"Unknown encoded timestamp");
            ComPtr<IMFMediaBuffer> buffer;
            if(!session.Check(sample->ConvertToContiguousBuffer(&buffer),"Encoded buffer")) return false;
            BYTE* data=nullptr; DWORD length=0;
            if(!session.Check(buffer->Lock(&data,nullptr,&length),"Encoded lock")) return false;
            EncodedAccessUnit unit;
            unit.bytes.assign(data,data+length); buffer->Unlock();
            unit.sequence=found->second.timing.codec_sequence;
            unit.source_sequence=found->second.timing.source_sequence;
            unit.timestamp_100ns=pts;
            unit.duration_100ns=SUCCEEDED(sample->GetSampleDuration(&duration)) ?
                duration : found->second.timing.duration_100ns;
            UINT32 clean=FALSE; sample->GetUINT32(MFSampleExtension_CleanPoint,&clean);
            unit.keyframe=clean!=FALSE || Idr(unit.bytes);
            slots[found->second.slot].busy=false;
            pending.erase(found);
            if(unit.bytes.empty()) return session.Check(E_UNEXPECTED,"Empty access unit");
            units.push_back(std::move(unit));
        }
        return true;
    }
};
MftVideoEncoder::MftVideoEncoder():impl_(std::make_unique<Impl>()){}
MftVideoEncoder::~MftVideoEncoder(){impl_->session.Stop();}
void MftVideoEncoder::Stop(){impl_->session.Stop();impl_=std::make_unique<Impl>();}
bool MftVideoEncoder::Initialize(ID3D11Device* device,const VideoCodecConfig& config,VideoBackendPolicy policy) {
    Stop(); auto& p=*impl_; p.config=config; p.device=device;
    if(!device || !IsValidVideoCodecConfig(config) || config.codec!=VideoCodecKind::H264 ||
        config.width%2 || config.height%2) return p.session.Check(E_INVALIDARG,"Encoder config");
    device->GetImmediateContext(&p.context);
    if(!p.converter.Initialize(device)) return p.session.Check(p.converter.LastError(),"Video converter");
    for(auto& slot:p.slots) if(!p.converter.CreateNv12Texture(config.width,config.height,slot.texture))
        return p.session.Check(p.converter.LastError(),"NV12 pool");
    auto list=Candidates(VideoCodecDirection::Encode,policy);
    if(list.empty()) return p.session.Check(MF_E_TOPO_CODEC_NOT_FOUND,"No H264 encoder");
    for(const auto& descriptor:list) {
        bool hardware=descriptor.candidate.classification==VideoBackendClassification::Hardware;
        if(!p.session.Open(descriptor,device,hardware)) continue;
        p.info={}; p.info.candidate=descriptor.candidate;
        p.info.asynchronous=p.session.Async(); p.info.surface_io=p.session.SurfaceIo();
        if(hardware && !p.session.SurfaceIo()) { p.session.Check(E_NOTIMPL,"Hardware surface path unavailable"); continue; }
        p.info.low_latency_applied=config.low_latency && LowLatency(p.session.Get());
        CodecValue(p.session.Get(),CODECAPI_AVEncCommonMeanBitRate,config.target_bitrate_bps);
        CodecValue(p.session.Get(),CODECAPI_AVEncMPVGOPSize,config.keyframe_interval_frames);
        CodecValue(p.session.Get(),CODECAPI_AVEncMPVDefaultBPictureCount,0);
        ComPtr<IMFMediaType> input,output;
        if(!p.session.Check(CreateMftVideoMediaType(MakeEncoderInputTypeSpec(config),&input),"Encoder input type") ||
           !p.session.Check(CreateMftVideoMediaType(MakeEncoderOutputTypeSpec(config),&output),"Encoder output type")) continue;
        input->SetUINT32(MF_MT_DEFAULT_STRIDE,config.width);
        input->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES,TRUE);
        input->SetUINT32(MF_MT_SAMPLE_SIZE,config.width*config.height*3/2);
        output->SetUINT32(MF_MT_MPEG2_PROFILE,eAVEncH264VProfile_Main);
        ColorType(input.Get()); ColorType(output.Get());
        if(p.session.SetTypes(input.Get(),output.Get(),true) && p.session.Start()) {
            p.running=true; return true;
        }
    }
    return false;
}
bool MftVideoEncoder::Poll(std::vector<EncodedAccessUnit>& output) {
    auto& p=*impl_; std::vector<ComPtr<IMFSample>> samples;
    return p.running && p.session.Poll(samples) && p.Collect(samples,output);
}
bool MftVideoEncoder::Encode(const capture::CapturedFrame& frame,const NormalizedVideoTiming& timing,
    std::vector<EncodedAccessUnit>& output) {
    auto& p=*impl_;
    if(!p.running || !frame.IsValid() || frame.width!=p.config.width || frame.height!=p.config.height ||
        timing.duration_100ns<=0 || p.pending.contains(timing.timestamp_100ns))
        return p.session.Check(E_INVALIDARG,"Encoder frame");
    if(!Poll(output)) return false;
    auto free=std::find_if(p.slots.begin(),p.slots.end(),[](const auto& slot){return !slot.busy;});
    const ULONGLONG deadline=GetTickCount64()+2000;
    while(free==p.slots.end() && GetTickCount64()<deadline) {
        if(!Poll(output)) return false;
        free=std::find_if(p.slots.begin(),p.slots.end(),[](const auto& slot){return !slot.busy;});
        if(free==p.slots.end()) Sleep(1);
    }
    if(free==p.slots.end()) return p.session.Check(HRESULT_FROM_WIN32(ERROR_TIMEOUT),"NV12 pool exhausted");
    if(!p.converter.ConvertBgraFrameToNv12(frame,free->texture.Get()))
        return p.session.Check(p.converter.LastError(),"BGRA to NV12");
    ComPtr<IMFSample> sample;
    if(p.session.SurfaceIo()) {
        ComPtr<IMFMediaBuffer> buffer;
        if(!p.session.Check(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),free->texture.Get(),0,FALSE,&buffer),"DXGI input buffer") ||
           !p.session.Check(MFCreateSample(&sample),"Input sample") ||
           !p.session.Check(sample->AddBuffer(buffer.Get()),"Input buffer")) return false;
        ++p.info.gpu_frames;
    } else {
        if(!p.readback) {
            D3D11_TEXTURE2D_DESC desc{}; free->texture->GetDesc(&desc);
            desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            if(!p.session.Check(p.device->CreateTexture2D(&desc,nullptr,&p.readback),"NV12 staging")) return false;
        }
        p.context->CopyResource(p.readback.Get(),free->texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(!p.session.Check(p.context->Map(p.readback.Get(),0,D3D11_MAP_READ,0,&mapped),"NV12 readback")) return false;
        std::vector<BYTE> bytes(static_cast<size_t>(p.config.width)*p.config.height*3/2);
        const auto* src=static_cast<const BYTE*>(mapped.pData);
        for(UINT row=0;row<p.config.height*3/2;++row)
            std::memcpy(bytes.data()+static_cast<size_t>(row)*p.config.width,
                src+static_cast<size_t>(row)*mapped.RowPitch,p.config.width);
        p.context->Unmap(p.readback.Get(),0);
        if(!MemorySample(bytes.data(),static_cast<DWORD>(bytes.size()),sample)) return p.session.Check(E_FAIL,"CPU NV12 sample");
        ++p.info.cpu_frames;
    }
    if(!p.session.Check(sample->SetSampleTime(timing.timestamp_100ns),"SetInputTime") ||
       !p.session.Check(sample->SetSampleDuration(timing.duration_100ns),"SetInputDuration")) return false;
    const size_t slot=static_cast<size_t>(free-p.slots.begin()); free->busy=true;
    p.pending.emplace(timing.timestamp_100ns,Impl::Pending{timing,slot,sample});
    std::vector<ComPtr<IMFSample>> samples;
    return p.session.Submit(sample.Get(),samples) && p.Collect(samples,output);
}
bool MftVideoEncoder::RequestKeyframe() {
    auto& p=*impl_;
    if(!p.running) return p.session.Check(E_UNEXPECTED,"Keyframe stopped");
    if(!CodecValue(p.session.Get(),CODECAPI_AVEncVideoForceKeyFrame,1))
        return p.session.Check(E_NOTIMPL,"Force keyframe");
    return true;
}
bool MftVideoEncoder::Drain(std::vector<EncodedAccessUnit>& output) {
    auto& p=*impl_; std::vector<ComPtr<IMFSample>> samples;
    if(!p.running || !p.session.Drain(samples) || !p.Collect(samples,output)) return false;
    p.running=false;
    return p.pending.empty() || p.session.Check(E_UNEXPECTED,"Encoder missing drained frames");
}
const VideoCodecRuntimeInfo& MftVideoEncoder::Info()const{return impl_->info;}
std::string MftVideoEncoder::Error()const{return impl_->session.Error();}
struct MftVideoDecoder::Impl {
    MftTransformSession session;
    VideoCodecConfig config;
    VideoCodecRuntimeInfo info;
    graphics::D3D11VideoConverter converter;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> cpu_nv12;
    std::map<LONGLONG,EncodedAccessUnit> pending;
    bool running{false};
    bool Collect(std::vector<ComPtr<IMFSample>>& samples,std::vector<capture::CapturedFrame>& frames) {
        for(auto& sample:samples) {
            LONGLONG pts=0;
            if(!session.Check(sample->GetSampleTime(&pts),"Decoded time")) return false;
            auto found=pending.find(pts);
            if(found==pending.end()) return session.Check(E_UNEXPECTED,"Unknown decoded timestamp");
            ComPtr<IMFMediaType> type;
            if(!session.Check(session.Get()->GetOutputCurrentType(0,&type),"Decoder output type")) return false;
            UINT32 width=0,height=0,stride=config.width;
            if(!session.Check(MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&width,&height),"Decoded size")) return false;
            const UINT32 coded_height=height;
            if(width!=config.width || height!=config.height) {
                MFVideoArea visible{}; UINT32 bytes=0;
                HRESULT aperture=type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                    reinterpret_cast<UINT8*>(&visible),sizeof(visible),&bytes);
                if(FAILED(aperture)) aperture=type->GetBlob(MF_MT_GEOMETRIC_APERTURE,
                    reinterpret_cast<UINT8*>(&visible),sizeof(visible),&bytes);
                if(FAILED(aperture) || bytes!=sizeof(visible) || width<config.width || height<config.height ||
                    visible.OffsetX.value!=0 || visible.OffsetX.fract!=0 ||
                    visible.OffsetY.value!=0 || visible.OffsetY.fract!=0 ||
                    visible.Area.cx!=static_cast<LONG>(config.width) ||
                    visible.Area.cy!=static_cast<LONG>(config.height))
                    return session.Check(MF_E_INVALIDMEDIATYPE,"Unexpected decoded aperture");
                width=config.width; height=config.height;
            }
            type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stride);
            ComPtr<IMFMediaBuffer> buffer;
            if(!session.Check(sample->GetBufferByIndex(0,&buffer),"Decoded buffer")) return false;
            ComPtr<IMFDXGIBuffer> dxgi;
            ComPtr<ID3D11Texture2D> nv12;
            UINT subresource=0;
            if(SUCCEEDED(buffer.As(&dxgi))) {
                if(!session.Check(dxgi->GetResource(IID_PPV_ARGS(&nv12)),"Decoded texture") ||
                   !session.Check(dxgi->GetSubresourceIndex(&subresource),"Decoded slice")) return false;
                ++info.gpu_frames;
            } else {
                if(!cpu_nv12 && !converter.CreateNv12Texture(width,height,cpu_nv12))
                    return session.Check(converter.LastError(),"CPU decoder upload texture");
                BYTE* bytes=nullptr; DWORD size=0;
                if(!session.Check(buffer->Lock(&bytes,nullptr,&size),"Decoded CPU lock")) return false;
                bool valid=stride>=width && static_cast<std::uint64_t>(stride)*coded_height*3/2<=size;
                if(valid) {
                    std::vector<BYTE> compact(static_cast<size_t>(width)*height*3/2);
                    for(UINT row=0;row<height;++row)
                        std::memcpy(compact.data()+static_cast<size_t>(row)*width,
                            bytes+static_cast<size_t>(row)*stride,width);
                    for(UINT row=0;row<height/2;++row)
                        std::memcpy(compact.data()+static_cast<size_t>(height+row)*width,
                            bytes+static_cast<size_t>(coded_height+row)*stride,width);
                    context->UpdateSubresource(cpu_nv12.Get(),0,nullptr,compact.data(),width,0);
                }
                buffer->Unlock();
                if(!valid) return session.Check(E_INVALIDARG,"Decoded CPU layout");
                nv12=cpu_nv12; ++info.cpu_frames;
            }
            capture::CapturedFrame frame;
            frame.width=width; frame.height=height; frame.sequence=found->second.sequence;
            frame.timestamp_100ns=pts;
            if(!converter.CreateBgraTexture(width,height,frame.texture) ||
               !converter.ConvertNv12TextureToBgra(nv12.Get(),subresource,width,height,frame.texture.Get()))
                return session.Check(converter.LastError(),"NV12 to BGRA");
            // Keep the decoder's tracked sample lease until the GPU copy/processor
            // completes; retaining the texture alone does not lease its array slice.
            D3D11_QUERY_DESC query_desc{D3D11_QUERY_EVENT,0};
            ComPtr<ID3D11Query> fence;
            if(!session.Check(device->CreateQuery(&query_desc,&fence),"Decode copy fence")) return false;
            context->End(fence.Get());
            const ULONGLONG deadline=GetTickCount64()+2000;
            HRESULT done=S_FALSE;
            while((done=context->GetData(fence.Get(),nullptr,0,0))==S_FALSE && GetTickCount64()<deadline) Sleep(0);
            if(done==S_FALSE) return session.Check(HRESULT_FROM_WIN32(ERROR_TIMEOUT),"Decode copy timeout");
            if(!session.Check(done,"Decode copy complete")) return false;
            pending.erase(found);
            frames.push_back(std::move(frame));
        }
        return true;
    }
};
MftVideoDecoder::MftVideoDecoder():impl_(std::make_unique<Impl>()){}
MftVideoDecoder::~MftVideoDecoder(){impl_->session.Stop();}
void MftVideoDecoder::Stop(){impl_->session.Stop();impl_=std::make_unique<Impl>();}
bool MftVideoDecoder::Initialize(ID3D11Device* device,const VideoCodecConfig& config,bool force_software) {
    Stop(); auto& p=*impl_; p.config=config; p.device=device;
    if(!device || !IsValidVideoCodecConfig(config) || config.codec!=VideoCodecKind::H264 ||
        config.width%2 || config.height%2) return p.session.Check(E_INVALIDARG,"Decoder config");
    device->GetImmediateContext(&p.context);
    if(!p.converter.Initialize(device)) return p.session.Check(p.converter.LastError(),"Decoder converter");
    for(const auto& descriptor:Candidates(VideoCodecDirection::Decode,force_software ?
        VideoBackendPolicy::ForceSoftware:VideoBackendPolicy::PreferHardware)) {
        if(!p.session.Open(descriptor,device,!force_software)) continue;
        p.info={}; p.info.candidate=descriptor.candidate;
        p.info.asynchronous=p.session.Async(); p.info.surface_io=p.session.SurfaceIo();
        p.info.low_latency_applied=config.low_latency && LowLatency(p.session.Get());
        if(force_software) CodecValue(p.session.Get(),CODECAPI_AVDecVideoAcceleration_H264,0,true);
        ComPtr<IMFMediaType> input,output;
        if(!p.session.Check(CreateMftVideoMediaType(MakeDecoderInputTypeSpec(config),&input),"Decoder input type") ||
           !p.session.Check(CreateMftVideoMediaType(MakeDecoderOutputTypeSpec(config),&output),"Decoder output type")) continue;
        ColorType(input.Get()); ColorType(output.Get());
        if(p.session.SetTypes(input.Get(),output.Get(),false) && p.session.Start()) {
            p.running=true; return true;
        }
    }
    return false;
}
bool MftVideoDecoder::Decode(const EncodedAccessUnit& unit,std::vector<capture::CapturedFrame>& frames) {
    auto& p=*impl_;
    if(!p.running || unit.bytes.empty() || unit.bytes.size()>MAXDWORD ||
        unit.codec!=VideoCodecKind::H264 || p.pending.size()>=32 || p.pending.contains(unit.timestamp_100ns))
        return p.session.Check(E_INVALIDARG,"Decoder input");
    ComPtr<IMFSample> sample;
    if(!MemorySample(unit.bytes.data(),static_cast<DWORD>(unit.bytes.size()),sample))
        return p.session.Check(E_FAIL,"Compressed input sample");
    sample->SetSampleTime(unit.timestamp_100ns); sample->SetSampleDuration(unit.duration_100ns);
    sample->SetUINT32(MFSampleExtension_CleanPoint,unit.keyframe);
    auto metadata=unit; metadata.bytes.clear(); p.pending.emplace(unit.timestamp_100ns,std::move(metadata));
    std::vector<ComPtr<IMFSample>> samples;
    return p.session.Submit(sample.Get(),samples) && p.Collect(samples,frames);
}
bool MftVideoDecoder::Poll(std::vector<capture::CapturedFrame>& frames) {
    auto& p=*impl_; std::vector<ComPtr<IMFSample>> samples;
    return p.running && p.session.Poll(samples) && p.Collect(samples,frames);
}
bool MftVideoDecoder::Drain(std::vector<capture::CapturedFrame>& frames) {
    auto& p=*impl_; std::vector<ComPtr<IMFSample>> samples;
    if(!p.running || !p.session.Drain(samples) || !p.Collect(samples,frames)) return false;
    p.running=false;
    return p.pending.empty() || p.session.Check(E_UNEXPECTED,"Decoder missing drained frames");
}
bool MftVideoDecoder::Flush() {
    auto& p=*impl_; p.pending.clear(); p.running=true; return p.session.Flush();
}
const VideoCodecRuntimeInfo& MftVideoDecoder::Info()const{return impl_->info;}
std::string MftVideoDecoder::Error()const{return impl_->session.Error();}
}
