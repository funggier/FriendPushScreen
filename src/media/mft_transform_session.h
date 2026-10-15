#pragma once
#include "media/mf_d3d11_device_manager.h"
#include "media/mft_video_transform_activation.h"
#include <mfapi.h>
#include <mferror.h>
#include <vector>
#include <string>
namespace fps::media {
// One worker owns this object; only its D3D11 device is shared.
class MftTransformSession {
public:
    ~MftTransformSession();
    bool Open(const MftVideoCodecDescriptor&, ID3D11Device*, bool use_surfaces);
    bool SetTypes(IMFMediaType* input, IMFMediaType* output, bool encoder);
    bool Start();
    bool Submit(IMFSample*, std::vector<Microsoft::WRL::ComPtr<IMFSample>>&);
    bool Poll(std::vector<Microsoft::WRL::ComPtr<IMFSample>>&);
    bool Drain(std::vector<Microsoft::WRL::ComPtr<IMFSample>>&);
    bool Flush();
    bool Check(HRESULT, const char* stage);
    void Stop();
    IMFTransform* Get() const { return transform_.Get(); }
    HRESULT Hr() const { return hr_; }
    std::string Error() const;
    bool Async() const { return asynchronous_; }
    bool SurfaceIo() const { return surface_io_; }
private:
    bool Events();
    bool Output(std::vector<Microsoft::WRL::ComPtr<IMFSample>>&);
    bool SelectOutput();
    Microsoft::WRL::ComPtr<IMFActivate> activation_;
    Microsoft::WRL::ComPtr<IMFTransform> transform_;
    Microsoft::WRL::ComPtr<IMFMediaEventGenerator> events_;
    MfD3d11DeviceManager manager_;
    GUID output_subtype_{};
    bool mf_started_{false}, com_started_{false};
    bool asynchronous_{false}, surface_io_{false}, draining_{false}, drained_{false};
    unsigned input_credits_{0}, output_credits_{0};
    HRESULT hr_{S_OK};
    std::string stage_;
};
}
