#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace fps::capture {

enum class CapturePixelFormat {
    Bgra8
};

struct CapturedCursor {
    bool visible{false};
    std::int32_t x{0};
    std::int32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t row_pitch{0};
    std::vector<std::uint8_t> bgra;

    bool IsValid() const noexcept {
        return visible &&
            width > 0 &&
            height > 0 &&
            !bgra.empty();
    }
};

struct CapturedFrame {
    std::uint64_t sequence{0};
    std::int64_t timestamp_100ns{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t row_pitch{0};
    CapturePixelFormat pixel_format{
        CapturePixelFormat::Bgra8};

    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        texture;
    std::vector<std::uint8_t> bgra;
    CapturedCursor cursor;

    bool HasGpuTexture() const noexcept {
        return texture != nullptr;
    }

    bool HasCpuPixels() const noexcept {
        return !bgra.empty();
    }

    bool IsValid() const noexcept {
        return width > 0 &&
            height > 0 &&
            (HasGpuTexture() ||
             HasCpuPixels());
    }
};

}  // namespace fps::capture
