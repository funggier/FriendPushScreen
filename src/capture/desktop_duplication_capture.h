#pragma once

#include "capture/capture_session.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>

namespace fps::capture {

class DesktopDuplicationCaptureSession final
    : public ICaptureSession {
public:
    DesktopDuplicationCaptureSession(
        CaptureSourceDescriptor source,
        ID3D11Device* device);

    bool Start() override;
    void Stop() override;

    bool TryAcquireFrame(
        CapturedFrame& frame) override;

    bool SetCursorCaptureEnabled(
        bool enabled) override {
        show_cursor_ = enabled;
        return true;
    }

    CaptureSessionState State()
        const noexcept override {
        return state_;
    }

    const CaptureSourceDescriptor&
    Source() const noexcept override {
        return source_;
    }

    long LastError()
        const noexcept override {
        return last_error_;
    }

    std::string StatusText()
        const override;

private:
    bool FindOutput();
    bool EnsureFrameTexture(
        const D3D11_TEXTURE2D_DESC& source_desc);
    void CaptureCursor(
        CapturedCursor& cursor) const;

    CaptureSourceDescriptor source_;

    Microsoft::WRL::ComPtr<ID3D11Device>
        device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>
        context_;
    Microsoft::WRL::ComPtr<IDXGIOutput1>
        output_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication>
        duplication_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        frame_texture_;
    Microsoft::WRL::ComPtr<ID3D11Query>
        copy_complete_query_;

    RECT output_rect_{};
    std::uint64_t sequence_{0};
    bool show_cursor_{true};
    CaptureSessionState state_{
        CaptureSessionState::Stopped};
    HRESULT last_error_{S_OK};
};

}  // namespace fps::capture
