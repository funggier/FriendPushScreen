# Current Development Task

## FPS-005 — Unified Capture Sources

**Status:** GREEN / CLOSED

**Branch:** `fps-005-unified-capture-sources`

**Started:** 2026-10-15

**GitHub Issue:** #11 — FPS-005 — Unified Capture Sources

### Goal

Deliver one native capture-source model for Monitor, Window, and generic Windows video capture devices, then prove local preview on the reference Windows 10 machine.

### Required outcomes

- one source catalog covering Screens / Windows / Devices
- direct source selection without a source-type-first workflow
- stable source IDs and availability state
- unified capture-session abstraction
- unified captured-frame metadata
- Monitor preview through native D3D11 Desktop Duplication baseline
- Window preview through Windows Graphics Capture when local SDK/runtime support is available
- generic Capture Card preview through Media Foundation
- runtime video-device mode enumeration
- source-loss/device-removal behavior without automatic source substitution
- source refresh/reappearance handling
- persisted `Show Cursor` sender option, default ON, with live DDA/WGC cursor policy; disabled as not-applicable for VideoDevice sources
- Settings remains an overlay; opening Settings must not resize the preview surface
- windowed status chrome reserves its own top strip so incoming content is never obscured
- F11 hides status chrome/Settings button and expands preview to the full client area
- evidence for DG-002 and DG-003

### Safety / scope boundary

FPS-005 must not implement video encoding/decoding, network media transport, control protocol, audio capture/playback, secure handshake, or feedback-mask qualification.

No device-specific Onten code path is allowed.

### Plan

`docs/superpowers/plans/2026-10-15-fps-005-unified-capture-sources.md`

### Qualification

FPS-005 becomes GREEN only after:

- source-catalog/state unit tests
- monitor/window/device enumeration evidence
- real local Monitor preview
- Window preview when WGC backend is available in the local toolchain/runtime
- real Media Foundation capture-device preview
- source-loss/hot-plug evidence where practical
- physical cursor-visible preview plus live `Show Cursor` ON/OFF/persistence evidence
- Settings overlay leaves preview surface dimensions unchanged
- windowed viewer begins below status chrome and F11 viewer fills the full client area
- Debug + Release local build/test
- exact-commit Windows CI
- qualification evidence report

### Next after FPS-005

`FPS-006 — Encode / Decode / Local Loopback`

No development task is ACTIVE after FPS-005 closure.
