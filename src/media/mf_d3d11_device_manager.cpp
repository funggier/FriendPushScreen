#include "media/mf_d3d11_device_manager.h"

namespace fps::media {

bool MfD3d11DeviceManager::Initialize(
    ID3D11Device* device) {
    Reset();

    if (device == nullptr) {
        last_error_ = E_INVALIDARG;
        return false;
    }

    HRESULT hr =
        MFCreateDXGIDeviceManager(
            &reset_token_,
            &manager_);

    if (FAILED(hr)) {
        last_error_ = hr;
        return false;
    }

    hr = manager_->ResetDevice(
        device,
        reset_token_);

    if (FAILED(hr)) {
        manager_.Reset();
        reset_token_ = 0;
        last_error_ = hr;
        return false;
    }

    last_error_ = S_OK;
    return true;
}

void MfD3d11DeviceManager::Reset() noexcept {
    manager_.Reset();
    reset_token_ = 0;
    last_error_ = S_OK;
}

}  // namespace fps::media
