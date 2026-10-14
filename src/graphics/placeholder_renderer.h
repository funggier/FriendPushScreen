#pragma once

#include "graphics/d3d11_device.h"

namespace fps::graphics {

class PlaceholderRenderer {
public:
    bool Initialize(
        HWND hwnd,
        const RendererCreationPolicy& policy = {});
    bool Resize(UINT width, UINT height);
    bool Render();

    RendererMode Mode() const noexcept { return device_.Mode(); }
    HRESULT LastError() const noexcept { return device_.LastError(); }

private:
    D3D11Device device_;
};

}  // namespace fps::graphics
