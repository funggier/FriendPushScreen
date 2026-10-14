#include "platform/windows/dxgi_adapter_probe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <string>
#include <vector>

namespace fps::platform::windows {
namespace {

std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') {
        return {};
    }

    const int length = static_cast<int>(wcslen(value));
    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        length,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        length,
        output.data(),
        required,
        nullptr,
        nullptr);
    if (written != required) {
        return {};
    }
    return output;
}

}  // namespace

std::vector<diagnostics::GraphicsAdapterInfo> ProbeDxgiAdapters() {
    std::vector<diagnostics::GraphicsAdapterInfo> adapters;

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return adapters;
    }

    for (UINT index = 0;; ++index) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        const HRESULT hr = factory->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) {
            continue;
        }

        diagnostics::GraphicsAdapterInfo info{};
        info.description = WideToUtf8(desc.Description);
        info.vendor_id = desc.VendorId;
        info.device_id = desc.DeviceId;
        info.dedicated_video_memory_bytes =
            static_cast<std::uint64_t>(desc.DedicatedVideoMemory);
        info.shared_system_memory_bytes =
            static_cast<std::uint64_t>(desc.SharedSystemMemory);
        info.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        adapters.push_back(std::move(info));
    }

    return adapters;
}

}  // namespace fps::platform::windows
