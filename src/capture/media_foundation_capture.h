#pragma once

#include "capture/capture_session.h"
#include "media/capture_device_probe.h"

#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace fps::capture {

class MediaFoundationCaptureSession final
    : public ICaptureSession {
public:
    explicit MediaFoundationCaptureSession(
        CaptureSourceDescriptor source);

    ~MediaFoundationCaptureSession()
        override;

    bool Start() override;
    void Stop() override;

    bool TryAcquireFrame(
        CapturedFrame& frame) override;

    CaptureSessionState State()
        const noexcept override {
        return state_.load();
    }

    const CaptureSourceDescriptor&
    Source() const noexcept override {
        return source_;
    }

    long LastError()
        const noexcept override {
        return last_error_.load();
    }

    std::string StatusText()
        const override;

    const media::VideoCaptureMode&
    SelectedMode() const noexcept {
        return selected_mode_;
    }

private:
    bool CreateReader();
    bool ConfigureBestMode();
    bool ConfigureRgb32Output();

    void CaptureLoop();
    bool CopySampleToFrame(
        IMFSample* sample,
        LONGLONG timestamp_100ns,
        CapturedFrame& frame);

    CaptureSourceDescriptor source_;

    Microsoft::WRL::ComPtr<IMFMediaSource>
        media_source_;
    Microsoft::WRL::ComPtr<IMFSourceReader>
        reader_;

    media::VideoCaptureMode
        selected_mode_{};

    std::uint32_t output_width_{0};
    std::uint32_t output_height_{0};
    LONG output_stride_{0};

    std::atomic<bool>
        stop_requested_{false};

    std::thread worker_;

    mutable std::mutex frame_mutex_;
    CapturedFrame latest_frame_{};

    std::uint64_t sequence_{0};

    std::atomic<CaptureSessionState>
        state_{
            CaptureSessionState::Stopped};

    std::atomic<long>
        last_error_{S_OK};

    bool mf_started_{false};
};

}  // namespace fps::capture
