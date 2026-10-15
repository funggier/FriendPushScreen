#include "platform/windows/window_probe.h"

#include <string>
#include <vector>

namespace fps::platform::windows {
namespace {

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            output.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }
    return output;
}

std::string ReadWindowTitle(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) {
        return {};
    }

    std::wstring buffer(static_cast<std::size_t>(length + 1), L'\0');
    const int written = GetWindowTextW(hwnd, buffer.data(), length + 1);
    if (written <= 0) {
        return {};
    }
    buffer.resize(static_cast<std::size_t>(written));
    return WideToUtf8(buffer);
}

struct EnumContext {
    HWND self_root{nullptr};
    std::vector<diagnostics::WindowInfo>* output{nullptr};
};

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM user_data) {
    auto* context = reinterpret_cast<EnumContext*>(user_data);

    WindowCandidate candidate{};
    candidate.handle = hwnd;
    candidate.owner = GetWindow(hwnd, GW_OWNER);
    candidate.visible = IsWindowVisible(hwnd) != FALSE;
    candidate.title = ReadWindowTitle(hwnd);
    const LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    candidate.tool_window = (ex_style & WS_EX_TOOLWINDOW) != 0;

    if (!ShouldIncludeWindow(candidate, context->self_root)) {
        return TRUE;
    }

    diagnostics::WindowInfo info{};
    info.title = std::move(candidate.title);
    info.native_handle = reinterpret_cast<std::uintptr_t>(hwnd);
    context->output->push_back(std::move(info));
    return TRUE;
}

}  // namespace

bool ShouldIncludeWindow(const WindowCandidate& candidate, HWND self_root) {
    if (!candidate.visible || candidate.title.empty()) {
        return false;
    }

    if (candidate.handle == nullptr) {
        return false;
    }

    if (self_root != nullptr) {
        if (candidate.handle == self_root || IsChild(self_root, candidate.handle) != FALSE) {
            return false;
        }
        if (GetAncestor(candidate.handle, GA_ROOTOWNER) == self_root) {
            return false;
        }
    }

    if (candidate.tool_window) {
        return false;
    }

    if (candidate.owner != nullptr) {
        return false;
    }

    return true;
}

std::vector<diagnostics::WindowInfo> ProbeVisibleTopLevelWindows(HWND self_root) {
    std::vector<diagnostics::WindowInfo> windows;
    EnumContext context{self_root, &windows};
    EnumWindows(
        EnumWindowsCallback,
        reinterpret_cast<LPARAM>(&context));
    return windows;
}

}  // namespace fps::platform::windows
