#include "app/application.h"

#include <objbase.h>

namespace fps::app {
namespace {

const wchar_t* RendererStatusText(
    graphics::RendererMode mode) {
    switch (mode) {
    case graphics::RendererMode::Hardware:
        return L"D3D11 Hardware";
    case graphics::RendererMode::WarpFallback:
        return L"D3D11 WARP Fallback";
    case graphics::RendererMode::Error:
        return L"D3D11 Renderer Error";
    }
    return L"D3D11 Renderer Error";
}

}  // namespace

int Application::Run(HINSTANCE instance, int show_command) {
    const HRESULT com_hr = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED);
    const bool should_uninitialize = SUCCEEDED(com_hr);

    main_window_.SetResizeCallback(
        [this](UINT width, UINT height) {
            if (!renderer_initialized_) {
                return;
            }
            renderer_.Resize(width, height);
            renderer_.Render();
        });

    main_window_.SetFrameCallback(
        [this]() {
            if (renderer_initialized_) {
                renderer_.Render();
            }
        });

    if (!main_window_.Create(instance, show_command)) {
        if (should_uninitialize) {
            CoUninitialize();
        }
        return 1;
    }

    renderer_initialized_ =
        renderer_.Initialize(main_window_.RenderHandle());
    main_window_.SetStatusText(
        RendererStatusText(renderer_.Mode()));

    if (renderer_initialized_) {
        RECT client{};
        if (GetClientRect(
                main_window_.RenderHandle(),
                &client) != FALSE) {
            renderer_.Resize(
                static_cast<UINT>(client.right - client.left),
                static_cast<UINT>(client.bottom - client.top));
        }
        renderer_.Render();
    }

    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(
            &message,
            nullptr,
            0,
            0);
        if (result == 0) {
            break;
        }
        if (result == -1) {
            if (should_uninitialize) {
                CoUninitialize();
            }
            return 2;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (should_uninitialize) {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}

}  // namespace fps::app
