#pragma once

#include "capture/capture_frame.h"
#include "capture/capture_source.h"

#include <string>

namespace fps::capture {

enum class CaptureSessionState {
    Stopped,
    Starting,
    Previewing,
    SourceLost,
    Error
};

class ICaptureSession {
public:
    virtual ~ICaptureSession() = default;

    virtual bool Start() = 0;
    virtual void Stop() = 0;

    virtual bool TryAcquireFrame(
        CapturedFrame& frame) = 0;

    virtual bool SetCursorCaptureEnabled(
        bool enabled) {
        (void)enabled;
        return true;
    }

    virtual CaptureSessionState State()
        const noexcept = 0;

    virtual const CaptureSourceDescriptor&
    Source() const noexcept = 0;

    virtual long LastError()
        const noexcept = 0;

    virtual std::string StatusText()
        const = 0;
};

}  // namespace fps::capture
