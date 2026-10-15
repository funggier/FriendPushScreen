# Current Development Task

## FPS-004 — Viewer UI & Settings Shell

**Status:** ACTIVE

**Branch:** `fps-004-viewer-ui-settings-shell`

**Started:** 2026-10-15

**GitHub Issue:** #9 — FPS-004 — Viewer UI & Settings Shell

### Goal

Deliver the first viewer-first native Win32 UI shell on top of the FPS-003 D3D11/diagnostics baseline.

### Required outcomes

- viewer-first D3D11 surface
- Tab settings overlay plus an always-visible `Hide Settings (Tab) / Settings (Tab)` button on the full-width top status row
- smooth wheel-scrollable responsive settings with a fully program-owned dark scrollbar
- visible peer/share/connection/audio state
- fullscreen / borderless / always-on-top
- keyboard shortcuts
- safe settings persistence
- Connection Mode: Auto / LAN / Direct IP-DDNS / Tailscale
- Share Port for the local sharing/listen endpoint
- Connection Host / Remote Port
- Connect / Disconnect request-state shell without real networking
- Sender Audio Source dropdown
- receiver mute/unmute + fully program-owned local volume slider
- capability explanation for unsupported Application Audio
- FriendPushScreen-owned dark palette for panel/status/buttons/text fields/dropdowns/slider/scrollbar
- diagnostics preserved under Advanced

### Safety / scope boundary

FPS-004 must not implement real capture, audio capture/playback, codec execution, network media, control protocol or secure handshake.

Share and Connect controls must not claim a real active media/network session.

### Plan

`docs/superpowers/plans/2026-10-15-fps-004-viewer-ui-settings-shell.md`

### Qualification

FPS-004 becomes GREEN only after:

- UI/state unit tests
- settings-persistence tests
- physical Windows UX evidence
- D3D11 Hardware render preserved
- Debug + Release local build/test
- exact-commit Windows CI

### Next after FPS-004

`FPS-005 — Unified Capture Sources`
