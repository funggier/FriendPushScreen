# FPS-005 — Unified Capture Sources Qualification

**Date:** 2026-10-15  
**Task:** FPS-005  
**Branch:** `fps-005-unified-capture-sources`  
**Implementation candidate:** `6b77b268c6754ea7cda7a777656194eb87b0fb4b`  
**GitHub Issue:** #11  
**Result:** GREEN / CLOSED

## Scope qualified

FPS-005 delivers one native source-selection and local-preview layer for:

- Screen / Monitor
- Window
- generic Windows video capture device

The capture path stays below the future codec/network layers. FPS-005 does not add encoding, decoding, media transport, secure handshake, feedback masking, or audio capture.

## Architecture

Qualified backend policy:

- Monitor → DXGI Desktop Duplication (DDA)
- Window → Windows Graphics Capture (WGC)
- VideoDevice → Media Foundation (MF)

All three implement the same capture-session / captured-frame boundary and feed the same D3D11 preview renderer.

## Unified source catalog

A live source refresh on the Windows 10 reference machine exposed one selector containing all source kinds together.

Observed catalog included:

- `Screen — \\.\DISPLAY1 (2560x1440) [Primary]`
- `Screen — \\.\DISPLAY2 (1920x1080)`
- multiple visible top-level windows
- `Device — USB3 Video`

One observed refresh contained 19 selectable sources.

The UI does not require a source-type selection before choosing the actual source.

Stable source identity policy:

- monitor: Windows display device name
- window: HWND identity for the lifetime of that window
- video device: Media Foundation symbolic link

No Onten-specific branch exists.

## Monitor preview — DDA

Real local preview passed on the reference machine.

Primary display evidence:

- source: `\\.\DISPLAY1`
- source dimensions: 2560x1440
- status: `Monitor preview active (DDA)`
- Preview button changed to `Stop Preview`
- real desktop pixels rendered through the D3D11 preview path

Secondary display evidence:

- source: `\\.\DISPLAY2`
- source dimensions: 1920x1080
- DDA preview active
- used for cursor qualification without relying on the FriendPushScreen window being inside the captured monitor

## Window preview — WGC

A dedicated Notepad qualification window was created:

`FPS005-WGC-Qualification.txt - Notepad`

The first WGC start exposed a real integration defect:

`Window capture error (0x8001010D)`

The HRESULT is `RPC_E_CANTCALLOUT_ININPUTSYNCCALL`. Capture was being created synchronously inside the button's input callback.

The UI command path was corrected so capture-related commands are deferred through a posted MainWindow message before the application starts or modifies a capture session.

Deferred commands are:

- SelectCaptureSource
- ToggleCapturePreview
- RefreshCaptureSources
- ToggleShowCursor

After the fix, the same physical qualification produced:

`Window preview active (WGC ABI)`

This proves WGC window capture is functional on Windows 10 build 19045 with the native ABI implementation.

## Source-loss behavior

While the WGC Notepad preview was active, the target Notepad window was closed.

Observed state after loss:

- status: `Window source lost`
- selected source remained visible as `[Unavailable]`
- Preview returned to its stopped state
- FriendPushScreen did not automatically switch to another Screen, Window, or Device

This qualifies the no-auto-substitution rule.

Physical USB unplug/replug was not automated in this session. The real USB device was refreshed/re-enumerated repeatedly during qualification through its stable Media Foundation symbolic-link identity.

## Capture-card preview — Media Foundation

The generic capture device was selected from the same source selector:

`Device — USB3 Video`

Real Media Foundation preview passed with:

`Device preview active (MF) 1920x1080 @ 60 MJPG`

This is consistent with the earlier FPS-002 evidence that MJPEG is the stable 1080p60 path on the reference hardware.

For VideoDevice sources:

- Show Cursor displays as N/A
- cursor capture is not applied

DG-003 therefore resolves to Media Foundation as the V1 production capture-card backend. No DirectShow fallback is required by the reference device.

## Cursor policy

Show Cursor is a sender preference.

Qualified behavior:

- default path supports cursor for Screen and Window sources
- VideoDevice reports Show Cursor as not applicable
- DDA cursor capture was exercised on DISPLAY2
- toggling Show Cursor OFF persisted `show_cursor=false`
- after application restart the checkbox loaded unchecked
- toggling back ON persisted `show_cursor=true`

Pixel-level ON/OFF comparison of the Viewer region detected the expected small cursor-only change cluster around the rendered cursor position, confirming the preference affects captured/rendered cursor content rather than only changing checkbox state.

## Viewer layout — complete frame visible

The windowed status strip no longer overlays incoming content.

Measured physical geometry in a 1280x760 application window:

- status strip height: 30 px
- Viewer begins immediately below the status strip
- Viewer render surface: 1264x691 in the measured windowed state

Settings remains an overlay inside the Viewer.

A physical hide/show probe measured the render surface before hiding Settings, while Settings was hidden, and after Settings was shown again:

- X/Y unchanged
- width/height unchanged at 1264x691
- `SameWhenHidden=true`
- `SameWhenShown=true`

Therefore opening or closing Settings does not resize incoming video.

## Aspect-fit behavior

Preview rendering uses an aspect-fit viewport:

`scale = min(view/source width scale, view/source height scale)`

The frame is centered inside the Viewer.

Consequences:

- entire source frame remains visible
- aspect ratio is preserved
- no crop-to-fill behavior
- no stretch-to-window distortion
- unused area becomes letterbox/pillarbox space when aspect ratios differ

This calculation was extracted to `CalculateAspectFit` and unit-tested for:

- exact 16:9 fit
- 2560x1440 source into 1264x691 Viewer
- portrait source into landscape Viewer
- zero-size guard behavior

## Fullscreen

F11 physical qualification:

- client area: 2560x1440
- status strip: hidden
- Settings toggle button: hidden
- Viewer render surface: X=0, Y=0, W=2560, H=1440

Esc restores the prior windowed placement.

The same aspect-fit renderer remains in use in fullscreen.

## Local build and test evidence

### Debug

- x64 Debug build: PASS
- CTest: 1/1 PASS

### Release

- x64 Release build: PASS
- build process session completed with exit code 0
- CTest: 1/1 PASS

### Source hygiene

- `git diff --check`: PASS before candidate commit

## Decision gates

### DG-002 — RESOLVED

V1 capture backend policy:

- Monitor → DDA
- Window → WGC

Both were physically previewed on the Windows 10 reference machine through the unified session abstraction.

Feedback/exclusion behavior remains a separate FPS-007 responsibility.

### DG-003 — RESOLVED

V1 capture-card policy:

- Media Foundation primary
- no DirectShow production fallback required by the qualified reference device

A fallback may be revisited only if later hardware demonstrates a real Media Foundation compatibility gap.

## Exact-commit CI

Candidate:

`6b77b268c6754ea7cda7a777656194eb87b0fb4b`

GitHub Actions:

`38760344281` — Windows Build

Exact candidate result: **SUCCESS**

Qualified matrix:

- Windows x64 Debug — SUCCESS
- Windows x64 Release — SUCCESS

The exact candidate run completed successfully.

## Qualification decision

Local qualification is GREEN and exact-commit Windows CI is GREEN.

FPS-005 is qualified for closure and merge.
