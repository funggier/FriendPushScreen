#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include "audio/wasapi_audio.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
namespace fps::audio {
using Microsoft::WRL::ComPtr;
namespace {
std::string Utf8(const wchar_t* value) {
    if(!value)return {};const int n=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);
    if(n<=1)return {};std::string text(static_cast<std::size_t>(n),'\0');
    WideCharToMultiByte(CP_UTF8,0,value,-1,text.data(),n,nullptr,nullptr);text.pop_back();return text;
}
std::wstring Wide(const std::string& value) {
    if(value.empty()||value.size()>1024||value.find_first_of("\r\n")!=std::string::npos)return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(n<=0)return {};std::wstring text(static_cast<std::size_t>(n),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),text.data(),n);return text;
}
std::string Hr(const char* operation,HRESULT hr){std::ostringstream out;out<<operation<<" (0x"<<std::hex<<static_cast<unsigned long>(hr)<<')';return out.str();}
bool Enumerator(ComPtr<IMMDeviceEnumerator>& devices,std::string& error) {
    const auto hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices));
    if(FAILED(hr)){error=Hr("Audio device enumeration failed",hr);return false;}return true;
}
std::string Id(IMMDevice* device) {LPWSTR raw=nullptr;if(FAILED(device->GetId(&raw)))return {};auto value=Utf8(raw);CoTaskMemFree(raw);return value;}
std::string Name(IMMDevice* device) {
    ComPtr<IPropertyStore> store;if(FAILED(device->OpenPropertyStore(STGM_READ,&store)))return "Windows audio endpoint";
    PROPVARIANT name;PropVariantInit(&name);const auto hr=store->GetValue(PKEY_Device_FriendlyName,&name);
    auto text=SUCCEEDED(hr)&&name.vt==VT_LPWSTR?Utf8(name.pwszVal):std::string("Windows audio endpoint");
    PropVariantClear(&name);return text;
}
bool OpenEndpoint(const std::string& id,EDataFlow flow,ComPtr<IMMDevice>& device,std::string& error) {
    ComPtr<IMMDeviceEnumerator> devices;if(!Enumerator(devices,error))return false;HRESULT hr=E_INVALIDARG;
    if(id.empty())hr=devices->GetDefaultAudioEndpoint(flow,eConsole,&device);
    else {const auto wide=Wide(id);if(!wide.empty())hr=devices->GetDevice(wide.c_str(),&device);}
    if(FAILED(hr)){error=Hr("Audio endpoint unavailable",hr);return false;}
    ComPtr<IMMEndpoint> endpoint;EDataFlow actual=eAll;DWORD state=0;
    if(FAILED(device.As(&endpoint))||FAILED(endpoint->GetDataFlow(&actual))||actual!=flow||
       FAILED(device->GetState(&state))||state!=DEVICE_STATE_ACTIVE){error="Audio endpoint is inactive or has the wrong direction";device.Reset();return false;}
    return true;
}
WAVEFORMATEXTENSIBLE Format() {
    WAVEFORMATEXTENSIBLE f{};f.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;f.Format.nChannels=kAudioChannels;
    f.Format.nSamplesPerSec=kAudioRate;f.Format.wBitsPerSample=32;f.Format.nBlockAlign=kAudioChannels*sizeof(float);
    f.Format.nAvgBytesPerSec=f.Format.nSamplesPerSec*f.Format.nBlockAlign;f.Format.cbSize=22;
    f.Samples.wValidBitsPerSample=32;f.dwChannelMask=SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;f.SubFormat=KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;return f;
}
bool Client(IMMDevice* endpoint,bool loopback,ComPtr<IAudioClient>& client,HANDLE event,AudioPcmFormat& format,std::string& error) {
    auto hr=endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf()));
    if(FAILED(hr)){error=Hr("WASAPI activation failed",hr);return false;}
    WAVEFORMATEX* native=nullptr;hr=client->GetMixFormat(&native);
    if(FAILED(hr)){error=Hr("WASAPI mix format failed",hr);return false;}
    format.native_rate=native->nSamplesPerSec;format.native_channels=native->nChannels;CoTaskMemFree(native);
    auto requested=Format();
    const DWORD flags=AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|
        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY|AUDCLNT_STREAMFLAGS_NOPERSIST|(loopback?AUDCLNT_STREAMFLAGS_LOOPBACK:0);
    hr=client->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,200000,0,&requested.Format,nullptr);
    if(FAILED(hr)){error=Hr("WASAPI shared 48kHz stereo conversion unsupported",hr);return false;}
    hr=client->SetEventHandle(event);if(FAILED(hr)){error=Hr("WASAPI event setup failed",hr);return false;}return true;
}
}
std::int64_t AudioNow100ns() {LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);return (now.QuadPart/frequency.QuadPart)*10000000+(now.QuadPart%frequency.QuadPart)*10000000/frequency.QuadPart;}
std::string ResolveOutputEndpoint(const std::string& id,std::string& error) {ComPtr<IMMDevice> device;error.clear();return OpenEndpoint(id,eRender,device,error)?Id(device.Get()):std::string{};}
AudioCatalog EnumerateAudioDevices(std::uint32_t build) {
    AudioCatalog catalog;catalog.sources.push_back({"none","No Audio",{},AudioSourceKind::None,true});
    catalog.process_reason=build<20348?"Application Audio requires Windows build 20348+":"Process loopback API is available; application capture backend is not qualified";
    ComPtr<IMMDeviceEnumerator> devices;if(!Enumerator(devices,catalog.error))return catalog;
    ComPtr<IMMDevice> defaultRender;std::string defaultId;
    if(SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender,eConsole,&defaultRender)))defaultId=Id(defaultRender.Get());
    catalog.sources.push_back({"system","Desktop / System Audio",defaultId.empty()?"No default output endpoint":"",AudioSourceKind::SystemLoopback,!defaultId.empty()});
    for(const auto flow:{eRender,eCapture}) {
        ComPtr<IMMDeviceCollection> collection;if(FAILED(devices->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,&collection)))continue;
        UINT count=0;collection->GetCount(&count);count=std::min<UINT>(count,128);
        for(UINT i=0;i<count;++i) {
            ComPtr<IMMDevice> device;if(FAILED(collection->Item(i,&device)))continue;
            auto id=Id(device.Get()),name=Name(device.Get());if(id.empty())continue;
            if(flow==eRender) {
                catalog.outputs.push_back({id,name,id==defaultId});
                catalog.sources.push_back({"render:"+id,"Output loopback: "+name,{},AudioSourceKind::RenderLoopback,true});
            }else catalog.sources.push_back({"capture:"+id,"Input: "+name,{},AudioSourceKind::CaptureEndpoint,true});
        }
    }
    return catalog;
}
struct WasapiAudioSource::Impl {
    ComPtr<IMMDevice> device;ComPtr<IAudioClient> client;ComPtr<IAudioCaptureClient> capture;
    HANDLE event{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    AudioState state{AudioState::Disabled};AudioSourceDescriptor descriptor;AudioPcmFormat format;
    std::string endpoint;bool isDefault{},loopback{};std::uint64_t lastCheck{};
    ~Impl(){if(event)CloseHandle(event);}
};
WasapiAudioSource::WasapiAudioSource():impl_(std::make_unique<Impl>()){}
WasapiAudioSource::~WasapiAudioSource(){Stop();}
void WasapiAudioSource::Stop() {
    auto& p=*impl_;if(p.client)p.client->Stop();p.capture.Reset();p.client.Reset();p.device.Reset();p.endpoint.clear();
    p.state=AudioState::Disabled;p.isDefault=p.loopback=false;p.lastCheck=0;p.format={};p.descriptor={};
}
bool WasapiAudioSource::Start(const std::string& source,std::string& error) {
    Stop();error.clear();auto& p=*impl_;p.descriptor.id=source;
    if(source=="none"||source.empty()){p.descriptor.name="No Audio";return true;}
    p.state=AudioState::Error;EDataFlow flow=eRender;std::string id;
    if(source=="system"){p.isDefault=true;p.loopback=true;p.descriptor.kind=AudioSourceKind::SystemLoopback;}
    else if(source.starts_with("render:")){id=source.substr(7);p.loopback=true;p.descriptor.kind=AudioSourceKind::RenderLoopback;}
    else if(source.starts_with("capture:")){id=source.substr(8);flow=eCapture;p.descriptor.kind=AudioSourceKind::CaptureEndpoint;}
    else {error="Application/process audio capture is not supported by this qualified backend";return false;}
    if((!p.isDefault&&id.empty())||!p.event){error="Invalid audio source or event unavailable";return false;}
    if(!OpenEndpoint(id,flow,p.device,error))return false;p.endpoint=Id(p.device.Get());p.descriptor.name=Name(p.device.Get());
    if(!Client(p.device.Get(),p.loopback,p.client,p.event,p.format,error))return false;
    auto hr=p.client->GetService(IID_PPV_ARGS(&p.capture));if(SUCCEEDED(hr))hr=p.client->Start();
    if(FAILED(hr)){error=Hr("WASAPI capture start failed",hr);return false;}p.state=AudioState::Active;return true;
}
bool WasapiAudioSource::Poll(std::vector<AudioPcmChunk>& chunks,std::string& error) {
    chunks.clear();error.clear();auto& p=*impl_;if(p.state==AudioState::Disabled)return true;
    if(p.state!=AudioState::Active){error="Audio source is not active";return false;}
    if(GetTickCount64()-p.lastCheck>=1000) {
        p.lastCheck=GetTickCount64();DWORD state=0;
        if(FAILED(p.device->GetState(&state))||state!=DEVICE_STATE_ACTIVE){p.state=AudioState::SourceLost;error="Audio source endpoint was removed";return false;}
        if(p.isDefault) {std::string why;auto current=ResolveOutputEndpoint({},why);if(current!=p.endpoint){p.state=AudioState::SourceLost;error="Default audio output changed; restart audio source";return false;}}
    }
    if(WaitForSingleObject(p.event,0)!=WAIT_OBJECT_0)return true;
    UINT packet=0;auto hr=p.capture->GetNextPacketSize(&packet);
    for(unsigned n=0;SUCCEEDED(hr)&&packet&&n<6;++n) {
        BYTE* data=nullptr;UINT frames=0;DWORD flags=0;UINT64 position=0,qpc=0;
        hr=p.capture->GetBuffer(&data,&frames,&flags,&position,&qpc);if(FAILED(hr))break;
        if(frames>9600){p.capture->ReleaseBuffer(frames);error="WASAPI capture exceeded bounded packet capacity";p.state=AudioState::Error;return false;}
        AudioPcmChunk chunk;chunk.samples.assign(static_cast<std::size_t>(frames)*kAudioChannels,0);
        if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)&&data)std::memcpy(chunk.samples.data(),data,chunk.samples.size()*sizeof(float));
        for(auto& sample:chunk.samples)sample=std::isfinite(sample)?std::clamp(sample,-1.0f,1.0f):0.0f;
        chunk.pts_100ns=(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)||qpc>static_cast<UINT64>(INT64_MAX)?AudioNow100ns():static_cast<std::int64_t>(qpc);
        chunk.discontinuity=(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0;
        hr=p.capture->ReleaseBuffer(frames);if(FAILED(hr))break;
        chunks.push_back(std::move(chunk));hr=p.capture->GetNextPacketSize(&packet);
    }
    if(FAILED(hr)){p.state=AudioState::SourceLost;error=Hr("WASAPI capture device lost",hr);return false;}return true;
}
AudioState WasapiAudioSource::State()const{return impl_->state;}
AudioSourceDescriptor WasapiAudioSource::Descriptor()const{return impl_->descriptor;}
AudioPcmFormat WasapiAudioSource::Format()const{return impl_->format;}
std::string WasapiAudioSource::LoopbackEndpoint()const{return impl_->loopback?impl_->endpoint:std::string{};}
struct WasapiAudioRenderer::Impl {
    ComPtr<IAudioClient> client;ComPtr<IAudioRenderClient> render;HANDLE event{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    std::string endpoint;UINT capacity{};bool active{},muted{};int volume{100};
    std::deque<float> queued;AudioRenderMetrics metrics;
    ~Impl(){if(event)CloseHandle(event);}
};
WasapiAudioRenderer::WasapiAudioRenderer():impl_(std::make_unique<Impl>()){}
WasapiAudioRenderer::~WasapiAudioRenderer(){Stop();}
void WasapiAudioRenderer::Stop(){auto& p=*impl_;if(p.client)p.client->Stop();p.render.Reset();p.client.Reset();p.queued.clear();p.endpoint.clear();p.active=false;p.capacity=0;}
bool WasapiAudioRenderer::Start(const std::string& id,std::string& error) {
    Stop();error.clear();auto& p=*impl_;ComPtr<IMMDevice> endpoint;
    if(!p.event){error="WASAPI render event unavailable";return false;}
    if(!OpenEndpoint(id,eRender,endpoint,error))return false;p.endpoint=Id(endpoint.Get());AudioPcmFormat format;
    if(!Client(endpoint.Get(),false,p.client,p.event,format,error))return false;
    auto hr=p.client->GetBufferSize(&p.capacity);
    if(SUCCEEDED(hr))hr=p.client->GetService(IID_PPV_ARGS(&p.render));
    if(SUCCEEDED(hr))hr=p.client->Start();
    if(FAILED(hr)){error=Hr("WASAPI playback start failed",hr);Stop();return false;}
    p.active=true;return true;
}
void WasapiAudioRenderer::SetGain(bool muted,int volume){impl_->muted=muted;impl_->volume=std::clamp(volume,0,100);}
bool WasapiAudioRenderer::Queue(std::span<const float> samples,std::string& error) {
    auto& p=*impl_;error.clear();
    if(!p.active||samples.empty()||samples.size()>1920||samples.size()%2||
       !std::all_of(samples.begin(),samples.end(),[](float x){return std::isfinite(x);})) {error="Invalid PCM frame or inactive playback";return false;}
    while(p.queued.size()+samples.size()>kAudioFrames*kAudioChannels*3){p.queued.pop_front();p.queued.pop_front();++p.metrics.overruns;}
    p.queued.insert(p.queued.end(),samples.begin(),samples.end());p.metrics.max_queued_frames=std::max<std::uint64_t>(p.metrics.max_queued_frames,p.queued.size()/2);return true;
}
bool WasapiAudioRenderer::Pump(std::string& error) {
    auto& p=*impl_;error.clear();if(!p.active){error="Playback is inactive";return false;}
    if(WaitForSingleObject(p.event,0)!=WAIT_OBJECT_0)return true;
    UINT padding=0;auto hr=p.client->GetCurrentPadding(&padding);
    if(FAILED(hr)||padding>p.capacity){error=Hr("WASAPI playback endpoint lost",hr);Stop();return false;}
    const UINT available=p.capacity-padding;if(!available)return true;
    BYTE* raw=nullptr;hr=p.render->GetBuffer(available,&raw);
    if(FAILED(hr)){error=Hr("WASAPI playback buffer failed",hr);Stop();return false;}
    auto* samples=reinterpret_cast<float*>(raw);const auto wanted=static_cast<std::size_t>(available)*2;
    const auto amount=std::min(wanted,p.queued.size());std::size_t i=0;
    for(;i<amount;++i){samples[i]=p.queued.front();p.queued.pop_front();}
    std::fill(samples+amount,samples+wanted,0.0f);ApplyGain({samples,wanted},p.muted,p.volume);
    if(amount<wanted)++p.metrics.underflows;
    hr=p.render->ReleaseBuffer(available,amount?0:AUDCLNT_BUFFERFLAGS_SILENT);
    if(FAILED(hr)){error=Hr("WASAPI playback release failed",hr);Stop();return false;}
    p.metrics.frames+=amount/2;return true;
}
bool WasapiAudioRenderer::Active()const{return impl_->active;}
std::string WasapiAudioRenderer::Endpoint()const{return impl_->endpoint;}
AudioRenderMetrics WasapiAudioRenderer::Metrics()const{return impl_->metrics;}
}
