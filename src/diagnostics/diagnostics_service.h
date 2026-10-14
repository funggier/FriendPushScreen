#pragma once

#include "diagnostics/diagnostics_snapshot.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace fps::diagnostics {

inline constexpr UINT WM_FPS_DIAGNOSTICS_READY = WM_APP + 0x31;

struct DiagnosticsProbeBundle {
    std::function<OsInfo()> operating_system;
    std::function<CpuMemoryInfo()> cpu_memory;
    std::function<std::vector<GraphicsAdapterInfo>()> graphics_adapters;
    std::function<std::vector<MonitorInfo>()> monitors;
    std::function<std::vector<WindowInfo>(HWND)> visible_windows;
    std::function<std::vector<CaptureDeviceInfo>()> capture_devices;
    std::function<std::vector<CodecCapabilityInfo>()> codec_capabilities;
    std::function<NetworkInfo()> network;
};

DiagnosticsProbeBundle MakeNativeProbeBundle();

class DiagnosticsService {
public:
    DiagnosticsService();
    explicit DiagnosticsService(DiagnosticsProbeBundle bundle);
    ~DiagnosticsService();

    DiagnosticsService(const DiagnosticsService&) = delete;
    DiagnosticsService& operator=(const DiagnosticsService&) = delete;

    bool StartCollection(HWND notify_hwnd);
    bool IsCollecting() const noexcept;
    std::shared_ptr<const DiagnosticsSnapshot> Latest() const;
    bool WaitForIdle(std::chrono::milliseconds timeout);

private:
    void Collect(HWND notify_hwnd);
    void FinishCollection(
        HWND notify_hwnd,
        std::shared_ptr<const DiagnosticsSnapshot> snapshot);
    void JoinCompletedWorker();

    DiagnosticsProbeBundle bundle_;
    mutable std::mutex mutex_;
    std::condition_variable idle_cv_;
    std::shared_ptr<const DiagnosticsSnapshot> latest_;
    std::thread worker_;
    bool collecting_{false};
};

}  // namespace fps::diagnostics
