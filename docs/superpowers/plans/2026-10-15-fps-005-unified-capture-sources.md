# FPS-005 — Unified Capture Sources Implementation Plan

**Task:** FPS-005  
**Issue:** #11  
**Branch:** `fps-005-unified-capture-sources`  
**Base:** `b3e34d93f18af411f761359fed1e6c0fc18fea36`

## Goal

Deliver one native capture-source model for:

- monitors
- top-level windows
- generic Windows video capture devices

and prove local preview from the reference Windows 10 machine without introducing encode/decode/network transport.

## Product contract

The user chooses a source directly from one source selector. The UI must not require a separate source-type selection first.

Opening the Settings overlay must never reduce the viewer/render-surface dimensions. Settings remains an overlay above the preview surface.

The top status chrome is different: in normal windowed mode it owns a fixed strip above the viewer so it never obscures the top of the incoming image. The preview surface starts immediately below that strip. In F11 fullscreen, the status strip and its Settings button are hidden and the preview surface expands to the full client area.

## Architecture

```text
CaptureSourceCatalog
├─ Monitor descriptors
├─ Window descriptors
└─ Video-device descriptors
        │
        ▼
ICaptureSession
├─ DesktopDuplicationCaptureSession
├─ WindowsGraphicsCaptureSession
└─ MediaFoundationCaptureSession
        │
        ▼
CapturedFrame
├─ sequence
├─ timestamp
├─ width / height
├─ pixel format
├─ source kind/id
└─ D3D11 texture when available
        │
        ▼
PreviewRenderer
```

No encoder/network dependency is permitted in this layer.

## Backend policy for FPS-005

### Monitor

Use D3D11 DXGI Desktop Duplication as the qualified monitor-preview baseline because it is already proven available on the reference Windows 10 machine and gives a direct D3D11 texture path.

WGC monitor support may remain capability-visible, but DG-002 is not closed merely because WGC is present.

### Window

Use Windows Graphics Capture through Win32 interop when the local Windows SDK/toolchain exposes the required interop/runtime headers.

If toolchain support is absent, document the exact blocker rather than silently substituting a lower-quality capture API.

### Capture Card

Use Media Foundation device/source-reader APIs.

Device identity and modes must come from runtime enumeration. Never branch on Onten model name.

DirectShow remains evidence/fallback context only; no DirectShow production backend is added unless real Media Foundation qualification demonstrates a compatibility gap.

## Source catalog

Each source descriptor must carry enough stable data for UI selection and session creation:

- `kind`: Monitor / Window / VideoDevice
- stable `id`
- display label
- native monitor/window/device identifier
- availability state
- dimensions where known
- metadata needed by backend creation

The catalog must produce deterministic group order:

1. Screens
2. Windows
3. Devices

but expose one selectable list.

## Preview

FPS-005 preview is local only.

### Sender cursor visibility

The Share/Source UI exposes a persisted `Show Cursor` checkbox. It defaults to enabled.

- Monitor / DDA: cursor position and shape are captured as frame metadata and composited into the preview when enabled.
- Window / WGC: the same preference drives `IGraphicsCaptureSession2::IsCursorCaptureEnabled` when supported by the runtime.
- Video-device / Capture Card sources do not contain the local desktop cursor; the UI disables the checkbox and labels it as not applicable while a VideoDevice source is selected.
- Changing the option while preview is active takes effect immediately; it does not restart or change the selected source.

The cursor representation belongs to captured-frame policy rather than to the preview UI alone so FPS-006 can preserve the same sender choice before encoding.

The selected source frame replaces the placeholder render content while maintaining aspect ratio inside the active viewer viewport.

In windowed mode the viewport excludes the top status strip. In F11 fullscreen the viewport uses the complete client area. Settings stays overlayed and does not resize the preview.

## Source lifecycle

Required states:

- Available
- Starting
- Previewing
- Paused / SourceLost
- Error
- Stopped

For window close/minimize or device removal:

- do not auto-switch to another source
- preview stops/pauses visibly
- selection remains identifiable
- device reconnect with the same stable ID may become available again
- no network/share state is claimed

## Hot plug / refresh

Catalog refresh must detect:

- video device added
- video device removed
- source no longer available
- same stable device returning

FPS-005 may use explicit refresh plus device-change notification if practical; acceptance requires source-loss/reappearance evidence, not a specific UI mechanism.

## Decision gates

### DG-002

FPS-005 records real native monitor/window preview behavior and the reasons for the selected baseline policy.

Do not claim WGC vs DDA globally solved without evidence.

### DG-003

Media Foundation is qualified on the real generic USB video device if preview succeeds. DirectShow fallback remains unnecessary unless MF fails a real supported mode/device path.

## Tests

Unit-level:

- catalog grouping/order
- stable source IDs
- window filtering
- source-state transitions
- frame metadata validation
- device-mode normalization/selection
- no automatic source substitution on loss

Physical/local:

- enumerate real monitor(s)
- enumerate representative windows
- enumerate USB3 Video capture device
- monitor preview
- window preview if WGC backend is available
- capture-card preview through Media Foundation
- source removal/loss behavior where practical
- Settings overlay does not alter preview surface size
- windowed status chrome does not cover the preview
- F11 hides status chrome and uses the full client area
- `Show Cursor` defaults ON, can be toggled live, persists, and controls DDA/WGC cursor policy

Build gates:

- Debug build + CTest
- Release build + CTest
- exact-commit GitHub Windows Debug/Release CI

## Explicit non-goals

FPS-005 does not implement:

- video encoding/decoding
- local codec loopback
- audio capture/playback
- network media
- secure handshake/control protocol
- feedback mask/exclusion qualification
- automatic codec/quality policy

Those remain FPS-006 / FPS-006A / FPS-007+.
