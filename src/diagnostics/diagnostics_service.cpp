#include "diagnostics/diagnostics_service.h"

#include "media/capture_device_probe.h"
#include "media/codec_capability_probe.h"
#include "platform/windows/cpu_memory_probe.h"
#include "platform/windows/dxgi_adapter_probe.h"
#include "platform/windows/monitor_probe.h"
#include "platform/windows/network_probe.h"
#include "platform/windows/os_probe.h"
#include "platform/windows/window_probe.h"

#include <mfapi.h>
#include <objbase.h>

#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace fps::diagnostics {
namespace {

std::string TimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value =
        std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
    localtime_s(&local_time, &value);

    std::ostringstream out;
    out << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

ProbeResult ExceptionResult(
    std::string name,
    const std::exception& error) {
    return ProbeResult{
        std::move(name),
        ProbeState::Error,
        0,
        error.what()
    };
}

ProbeResult UnknownExceptionResult(std::string name) {
    return ProbeResult{
        std::move(name),
        ProbeState::Error,
        0,
        "Unknown probe exception"
    };
}

}  // namespace

DiagnosticsProbeBundle MakeNativeProbeBundle() {
    DiagnosticsProbeBundle bundle{};
    bundle.operating_system =
        []() { return platform::windows::ProbeOperatingSystem(); };
    bundle.cpu_memory =
        []() { return platform::windows::ProbeCpuAndMemory(); };
    bundle.graphics_adapters =
        []() { return platform::windows::ProbeDxgiAdapters(); };
    bundle.monitors =
        []() { return platform::windows::ProbeMonitors(); };
    bundle.visible_windows =
        [](HWND self_root) {
            return platform::windows::ProbeVisibleTopLevelWindows(self_root);
        };
    bundle.capture_devices =
        []() { return media::ProbeVideoCaptureDevices(); };
    bundle.codec_capabilities =
        []() { return media::ProbeCodecCapabilities(); };
    bundle.network =
        []() { return platform::windows::ProbeNetworkState(); };
    return bundle;
}

DiagnosticsService::DiagnosticsService()
    : DiagnosticsService(MakeNativeProbeBundle()) {}

DiagnosticsService::DiagnosticsService(DiagnosticsProbeBundle bundle)
    : bundle_(std::move(bundle)) {}

DiagnosticsService::~DiagnosticsService() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void DiagnosticsService::JoinCompletedWorker() {
    std::thread completed;
    {
        std::lock_guard lock{mutex_};
        if (!collecting_ && worker_.joinable()) {
            completed = std::move(worker_);
        }
    }

    if (completed.joinable()) {
        completed.join();
    }
}

bool DiagnosticsService::StartCollection(HWND notify_hwnd) {
    JoinCompletedWorker();

    {
        std::lock_guard lock{mutex_};
        if (collecting_) {
            return false;
        }
        collecting_ = true;
    }

    try {
        worker_ = std::thread(
            [this, notify_hwnd]() {
                Collect(notify_hwnd);
            });
    } catch (...) {
        {
            std::lock_guard lock{mutex_};
            collecting_ = false;
        }
        idle_cv_.notify_all();
        throw;
    }

    return true;
}

bool DiagnosticsService::IsCollecting() const noexcept {
    std::lock_guard lock{mutex_};
    return collecting_;
}

std::shared_ptr<const DiagnosticsSnapshot>
DiagnosticsService::Latest() const {
    std::lock_guard lock{mutex_};
    return latest_;
}

bool DiagnosticsService::WaitForIdle(
    std::chrono::milliseconds timeout) {
    {
        std::unique_lock lock{mutex_};
        if (!idle_cv_.wait_for(
                lock,
                timeout,
                [this]() { return !collecting_; })) {
            return false;
        }
    }

    JoinCompletedWorker();
    return true;
}

void DiagnosticsService::FinishCollection(
    HWND notify_hwnd,
    std::shared_ptr<const DiagnosticsSnapshot> snapshot) {
    {
        std::lock_guard lock{mutex_};
        latest_ = std::move(snapshot);
        collecting_ = false;
    }

    idle_cv_.notify_all();

    if (notify_hwnd != nullptr) {
        PostMessageW(
            notify_hwnd,
            WM_FPS_DIAGNOSTICS_READY,
            0,
            0);
    }
}

void DiagnosticsService::Collect(HWND notify_hwnd) {
    auto snapshot = std::make_shared<DiagnosticsSnapshot>();
    snapshot->generated_at = TimestampNow();
    snapshot->app_version = "0.1.0";

    const HRESULT co_hr =
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool should_uninitialize = SUCCEEDED(co_hr);

    if (FAILED(co_hr) && co_hr != RPC_E_CHANGED_MODE) {
        snapshot->probe_results.push_back(ProbeResult{
            "com",
            ProbeState::Error,
            static_cast<long>(co_hr),
            "CoInitializeEx failed"
        });
    }

    HRESULT mf_hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    const bool media_foundation_started = SUCCEEDED(mf_hr);
    if (!media_foundation_started) {
        snapshot->probe_results.push_back(ProbeResult{
            "media_foundation",
            ProbeState::Error,
            static_cast<long>(mf_hr),
            "MFStartup failed"
        });
    }

    if (bundle_.operating_system) {
        try {
            snapshot->os = bundle_.operating_system();
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("operating_system", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("operating_system"));
        }
    }

    if (bundle_.cpu_memory) {
        try {
            snapshot->cpu_memory = bundle_.cpu_memory();
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("cpu_memory", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("cpu_memory"));
        }
    }

    if (bundle_.graphics_adapters) {
        try {
            snapshot->graphics_adapters =
                bundle_.graphics_adapters();
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("graphics_adapters", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("graphics_adapters"));
        }
    }

    if (bundle_.monitors) {
        try {
            snapshot->monitors = bundle_.monitors();
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("monitors", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("monitors"));
        }
    }

    if (bundle_.visible_windows) {
        try {
            snapshot->visible_windows =
                bundle_.visible_windows(notify_hwnd);
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("visible_windows", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("visible_windows"));
        }
    }

    if (bundle_.network) {
        try {
            snapshot->network = bundle_.network();
            if (snapshot->network.result.state != ProbeState::Available) {
                snapshot->probe_results.push_back(
                    snapshot->network.result);
            }
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("network", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("network"));
        }
    }

    if (media_foundation_started && bundle_.capture_devices) {
        try {
            snapshot->capture_devices =
                bundle_.capture_devices();
            const auto result = media::MakeMediaEnumerationResult(
                "capture_devices",
                0,
                snapshot->capture_devices.size());
            if (result.state != ProbeState::Available) {
                snapshot->probe_results.push_back(result);
            }
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("capture_devices", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("capture_devices"));
        }
    }

    if (media_foundation_started && bundle_.codec_capabilities) {
        try {
            snapshot->codec_capabilities =
                bundle_.codec_capabilities();
            const auto result = media::MakeMediaEnumerationResult(
                "codec_capabilities",
                0,
                snapshot->codec_capabilities.size());
            if (result.state != ProbeState::Available) {
                snapshot->probe_results.push_back(result);
            }
        } catch (const std::exception& error) {
            snapshot->probe_results.push_back(
                ExceptionResult("codec_capabilities", error));
        } catch (...) {
            snapshot->probe_results.push_back(
                UnknownExceptionResult("codec_capabilities"));
        }
    }

    if (media_foundation_started) {
        MFShutdown();
    }

    if (should_uninitialize) {
        CoUninitialize();
    }

    FinishCollection(
        notify_hwnd,
        std::move(snapshot));
}

}  // namespace fps::diagnostics
