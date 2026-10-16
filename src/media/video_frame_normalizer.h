#pragma once
#include "capture/capture_frame.h"
#include <string>
namespace fps::media {
class VideoFrameNormalizer {
public:
    void Configure(ID3D11Device*,std::uint32_t width,std::uint32_t height);
    bool Normalize(const capture::CapturedFrame&,capture::CapturedFrame&,std::string&);
    bool RestoreVisible(capture::CapturedFrame&,std::uint32_t width,std::uint32_t height,std::string&);
    void Stop();
private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pad_;
    std::uint32_t width_{},height_{};
};
}
