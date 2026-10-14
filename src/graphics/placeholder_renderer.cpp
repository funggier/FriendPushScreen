#include "graphics/placeholder_renderer.h"

namespace fps::graphics {

bool PlaceholderRenderer::Initialize(
    HWND hwnd,
    const RendererCreationPolicy& policy) {
    return device_.Initialize(hwnd, policy);
}

bool PlaceholderRenderer::Resize(UINT width, UINT height) {
    return device_.Resize(width, height);
}

bool PlaceholderRenderer::Render() {
    return device_.PresentPlaceholder();
}

}  // namespace fps::graphics
