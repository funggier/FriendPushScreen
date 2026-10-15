#pragma once

#include "capture/capture_frame.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace fps::graphics {

struct VideoConversionSize {
    std::uint32_t width{0};
    std::uint32_t height{0};

    bool IsValid() const noexcept {
        // NV12 requires even luma dimensions.
        return width >= 2 &&
            height >= 2 &&
            (width % 2) == 0 &&
            (height % 2) == 0;
    }
};

class D3D11VideoConverter {
public:
    bool Initialize(
        ID3D11Device* device);

    bool CreateNv12Texture(
        std::uint32_t width,
        std::uint32_t height,
        Microsoft::WRL::ComPtr<
            ID3D11Texture2D>& texture);

    bool CreateBgraTexture(
        std::uint32_t width,
        std::uint32_t height,
        Microsoft::WRL::ComPtr<
            ID3D11Texture2D>& texture);

    bool ConvertBgraFrameToNv12(
        const capture::CapturedFrame& frame,
        ID3D11Texture2D* output_nv12);

    bool ConvertNv12TextureToBgra(
        ID3D11Texture2D* input_nv12,
        UINT input_subresource,
        std::uint32_t width,
        std::uint32_t height,
        ID3D11Texture2D* output_bgra);

    HRESULT LastError() const noexcept {
        return last_error_;
    }

private:
    bool EnsureProcessor(
        std::uint32_t width,
        std::uint32_t height,
        DXGI_FORMAT input_format,
        DXGI_FORMAT output_format);

    bool EnsureBgraInputScratch(
        std::uint32_t width,
        std::uint32_t height);

    bool EnsureNv12InputScratch(
        std::uint32_t width,
        std::uint32_t height);

    bool CopyCapturedFrameToBgraScratch(
        const capture::CapturedFrame& frame);

    bool Blit(
        ID3D11Texture2D* input,
        ID3D11Texture2D* output);

    Microsoft::WRL::ComPtr<ID3D11Device>
        device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>
        context_;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice>
        video_device_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext>
        video_context_;

    Microsoft::WRL::ComPtr<
        ID3D11VideoProcessorEnumerator>
        processor_enumerator_;
    Microsoft::WRL::ComPtr<
        ID3D11VideoProcessor>
        processor_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        bgra_input_scratch_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        nv12_input_scratch_;

    std::uint32_t processor_width_{0};
    std::uint32_t processor_height_{0};
    DXGI_FORMAT processor_input_format_{
        DXGI_FORMAT_UNKNOWN};
    DXGI_FORMAT processor_output_format_{
        DXGI_FORMAT_UNKNOWN};

    std::uint32_t bgra_scratch_width_{0};
    std::uint32_t bgra_scratch_height_{0};
    std::uint32_t nv12_scratch_width_{0};
    std::uint32_t nv12_scratch_height_{0};

    HRESULT last_error_{S_OK};
};

}  // namespace fps::graphics
