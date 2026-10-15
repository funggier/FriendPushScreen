#pragma once

#include <d3d11.h>
#include <mfapi.h>
#include <wrl/client.h>

namespace fps::media {

class MfD3d11DeviceManager {
public:
    bool Initialize(ID3D11Device* device);
    void Reset() noexcept;

    IMFDXGIDeviceManager* Get() const noexcept {
        return manager_.Get();
    }

    UINT ResetToken() const noexcept {
        return reset_token_;
    }

    HRESULT LastError() const noexcept {
        return last_error_;
    }

private:
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> manager_;
    UINT reset_token_{0};
    HRESULT last_error_{S_OK};
};

}  // namespace fps::media
