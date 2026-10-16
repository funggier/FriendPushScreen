#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>
#include "audio/wasapi_audio.h"
#include "audio/opus_codec.h"
#include <cmath>
#include <iostream>
#include <algorithm>
using namespace fps::audio;
namespace {
std::string Json(const std::string& s){std::string out;for(unsigned char c:s){if(c=='"'||c=='\\')out+='\\';if(c>=32)out+=static_cast<char>(c);}return out;}
std::vector<float> Tone(std::uint64_t frame,double hz=440) {
    std::vector<float> pcm(1920);for(unsigned i=0;i<960;++i)pcm[2*i]=pcm[2*i+1]=0.03f*static_cast<float>(std::sin(6.283185307179586*hz*static_cast<double>(frame*960+i)/48000.0));return pcm;
}
}
int main(int argc,char** argv) {
    const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(com))return 2;
    std::string mode="--inventory",sourceId="system",outputId;unsigned seconds=6;bool pcmMode=false;
    for(int i=1;i<argc;++i){const std::string a=argv[i];if(a=="--source"&&i+1<argc)sourceId=argv[++i];else if(a=="--output"&&i+1<argc)outputId=argv[++i];else if(a=="--seconds"&&i+1<argc)seconds=std::clamp(static_cast<unsigned>(std::atoi(argv[++i])),1U,600U);else if(a=="--pcm")pcmMode=true;else mode=a;}
    int result=0;
    {
        std::string error;auto catalog=EnumerateAudioDevices(19045);
        if(mode=="--inventory") {
            std::cout<<"{\"sources\":[";bool first=true;for(const auto& s:catalog.sources){if(!first)std::cout<<',';first=false;std::cout<<"{\"id\":\""<<Json(s.id)<<"\",\"name\":\""<<Json(s.name)<<"\",\"supported\":"<<(s.supported?"true":"false")<<"}";}
            std::cout<<"],\"outputs\":[";first=true;for(const auto& o:catalog.outputs){if(!first)std::cout<<',';first=false;std::cout<<"{\"id\":\""<<Json(o.id)<<"\",\"name\":\""<<Json(o.name)<<"\",\"default\":"<<(o.is_default?"true":"false")<<"}";}
            std::cout<<"],\"process\":\""<<Json(catalog.process_reason)<<"\"}\n";
        }else if(mode=="--tone") {
            WasapiAudioRenderer tone;
            if(!tone.Start(outputId,error)){std::cerr<<error;result=3;}
            else {const auto start=GetTickCount64();std::uint64_t n=0;while(GetTickCount64()-start<seconds*1000ULL){const auto now=GetTickCount64()-start;if(now>=n*20){auto f=Tone(n++);if(!tone.Queue(f,error)){result=4;break;}}if(!tone.Pump(error)){result=4;break;}Sleep(1);}std::cout<<"tone_frames="<<tone.Metrics().frames<<" "<<error<<"\n";}
        }else if(mode=="--self-test") {
            WasapiAudioSource source;WasapiAudioRenderer tone,receiver;OpusCodec codec;
            if(!source.Start(sourceId,error)||source.LoopbackEndpoint().empty()){std::cerr<<"Loopback source required: "<<error;result=3;}
            else if(!tone.Start(source.LoopbackEndpoint(),error)||!codec.Start(error)){std::cerr<<error;result=3;}
            else {
                auto output=ResolveOutputEndpoint(outputId,error);const bool blocked=FeedbackRisk(true,source.LoopbackEndpoint(),output,{});
                if(output.empty()||(!blocked&&!receiver.Start(outputId,error))){std::cerr<<error;result=3;}
                else {
                    const auto started=GetTickCount64();std::uint64_t toneSeq=0,encoded=0,sampleCount=0,bytes=0;std::int64_t pts=0;
                    std::vector<float> pending;std::deque<std::vector<float>> replay;
                    double inputEnergy=0,outputEnergy=0,latencySum=0,latencyMax=0;std::uint64_t discontinuities=0;
                    while(GetTickCount64()-started<seconds*1000ULL&&result==0) {
                        const auto now=GetTickCount64()-started;
                        if(now>=toneSeq*20){auto f=Tone(toneSeq++);if(!tone.Queue(f,error)){result=4;break;}}
                        if(!tone.Pump(error)){result=4;break;}
                        std::vector<AudioPcmChunk> chunks;if(!source.Poll(chunks,error)){result=4;break;}
                        for(auto& chunk:chunks) {
                            if(chunk.discontinuity)++discontinuities;
                            if(pending.empty())pts=chunk.pts_100ns;
                            pending.insert(pending.end(),chunk.samples.begin(),chunk.samples.end());
                            while(pending.size()>=1920) {
                                std::vector<float> input(pending.begin(),pending.begin()+1920);pending.erase(pending.begin(),pending.begin()+1920);
                                auto packet=codec.Encode(input,error);auto outputPcm=pcmMode?input:codec.Decode(packet,error);
                                if(packet.empty()||outputPcm.size()!=1920){result=4;break;}
                                if(encoded>10)for(std::size_t i=0;i<1920;++i){inputEnergy+=input[i]*input[i];outputEnergy+=outputPcm[i]*outputPcm[i];++sampleCount;}
                                auto half=outputPcm;ApplyGain(half,false,50);auto muted=outputPcm;ApplyGain(muted,true,100);
                                for(std::size_t i=0;i<outputPcm.size();++i)if(half[i]!=outputPcm[i]*0.5f||muted[i]!=0)result=5;
                                const double latency=std::max(0.0,static_cast<double>(AudioNow100ns()-pts)/10000.0);latencySum+=latency;latencyMax=std::max(latencyMax,latency);
                                pts+=200000;++encoded;bytes+=packet.size();
                                if(blocked){replay.push_back(std::move(outputPcm));if(replay.size()>100)replay.pop_front();}
                                else if(!receiver.Queue(outputPcm,error)){result=4;break;}
                            }
                        }
                        if(!blocked&&!receiver.Pump(error)){result=4;break;}Sleep(1);
                    }
                    const auto native=source.Format();source.Stop();tone.Stop();
                    // Same-endpoint physical playback is safe after capture has stopped.
                    if(blocked&&result==0) {
                        if(!receiver.Start(outputId,error))result=4;
                        const auto start=GetTickCount64();std::uint64_t n=0;
                        while(result==0&&(!replay.empty()||GetTickCount64()-start<n*20+60)) {
                            if(!replay.empty()&&GetTickCount64()-start>=n*20){if(!receiver.Queue(replay.front(),error))result=4;replay.pop_front();++n;}
                            if(!receiver.Pump(error))result=4;Sleep(1);
                        }
                    }
                    const auto render=receiver.Metrics();
                    if(encoded<25||!sampleCount||inputEnergy/sampleCount<0.000001||outputEnergy/sampleCount<0.000001||render.frames<960)result=6;
                    std::cout<<"{\"mode\":\""<<(pcmMode?"PCM":"Opus1.6.1")<<"\",\"result\":"<<result<<",\"encoded_frames\":"<<encoded<<",\"bytes\":"<<bytes<<",\"native_rate\":"<<native.native_rate<<",\"native_channels\":"<<native.native_channels<<",\"input_rms\":"<<std::sqrt(inputEnergy/std::max<std::uint64_t>(1,sampleCount))<<",\"decoded_rms\":"<<std::sqrt(outputEnergy/std::max<std::uint64_t>(1,sampleCount))<<",\"capture_encode_latency_mean_ms\":"<<latencySum/std::max<std::uint64_t>(1,encoded)<<",\"capture_encode_latency_max_ms\":"<<latencyMax<<",\"render_frames\":"<<render.frames<<",\"render_underflows\":"<<render.underflows<<",\"render_overruns\":"<<render.overruns<<",\"render_max_queue_frames\":"<<render.max_queued_frames<<",\"discontinuities\":"<<discontinuities<<",\"same_endpoint_sequential_playback\":"<<(blocked?"true":"false")<<",\"error\":\""<<Json(error)<<"\"}\n";
                }
            }
        }else {std::cerr<<"Unknown audio probe mode";result=2;}
    }
    CoUninitialize();return result;
}
