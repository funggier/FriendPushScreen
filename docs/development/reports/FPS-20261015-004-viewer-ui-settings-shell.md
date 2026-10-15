# FPS-004 — Viewer UI & Settings Shell Qualification

**Date:** 2026-10-15  
**Task:** FPS-004  
**Branch:** `fps-004-viewer-ui-settings-shell`  
**Implementation candidate:** `46e14dd2f4520243079548a854d47c42048fd220`  
**GitHub Issue:** #9  
**Result:** GREEN / CLOSED

## Scope qualified

FPS-004 delivers the viewer-first native Win32/D3D11 shell without implementing real media capture, audio capture/playback, codec execution, network transport, control protocol, secure handshake or peer discovery.

Implemented UI/state behavior includes:

- full-client D3D11 viewer surface preserved
- full-width top status strip
- persistent `Hide Settings (Tab) / Settings (Tab)` control on the status row
- settings show/hide through the same state path as the `Tab` hotkey
- Share Port separated from Remote Port
- Connection Mode: LAN / Direct IP-DDNS / Tailscale
- Host + Remote Port shell
- sender Audio Source shell
- receiver mute/unmute + local volume state
- fullscreen / borderless / always-on-top
- diagnostics preserved under Advanced
- safe preference persistence without auto-share / auto-connect / fullscreen restoration
- FriendPushScreen-owned application control palette
- FriendPushScreen-owned drop-down popup, settings scrollbar and receiver-volume slider
- canonical transparent-background oak-gradient application icon embedded in the executable/window class
- explicit Connection Mode set limited to LAN / Direct IP-DDNS / Tailscale; no Auto mode

## Local build and test evidence

### Debug

- CMake/MSBuild x64 Debug build: PASS
- `FriendPushScreenTests`: PASS
- CTest: 1/1 PASS

### Release

- CMake/MSBuild x64 Release build: PASS
- `FriendPushScreenTests`: PASS
- CTest: 1/1 PASS

### Source hygiene

- `git diff --check`: PASS before candidate commit

## Physical Windows UX evidence

Reference machine: Windows 10 Pro build 19045.

### Viewer / renderer

- live window title: `FriendPushScreen - D3D11 Hardware`
- D3D11 hardware renderer preserved after FPS-004 UI integration

### Status chrome

- status strip spans the full client width
- status text includes a small left inset before `Peer:`
- live status example:
  `Peer: Offline | Connection: Disconnected | Sharing: Not sharing | Incoming Audio: Inactive`

### Settings visibility

- visible state button: `Hide Settings (Tab)`
- hidden state button: `Settings (Tab)`
- button remains at the same top-right position while the panel visibility changes
- panel hide/show repaints immediately; reopening no longer requires an additional click
- `Tab` and the visible button operate on the same state

### Program-owned controls

Live child classes after qualification:

- settings panel: `FriendPushScreenSettingsPanel`
- settings scrollbar: `FriendPushScreenSettingsScrollbar`
- receiver volume slider: `FriendPushScreenVolumeSlider`
- drop-down popup: `FriendPushScreenDropPopup`

The settings scrollbar and receiver volume slider no longer use Windows trackbar controls, eliminating native hover/theme repaint flashes.

### Settings scrolling

- one wheel notch moved the settings content by the configured 72 px target
- wheel scrolling uses timer-driven target interpolation
- direct scrollbar click/drag cancels pending smooth animation and moves to the requested position immediately

### Receiver volume

- Mute button and Volume slider physically align on the same row
- measured live geometry:
  - Mute: height 32 px
  - Volume slider: height 32 px
- wheel-up behavior validated from 70% → 75%
- wheel-down is the inverse path
- receiver volume remains persisted as a safe local preference

### Connection / sharing shell

- Share Port and Remote Port are independent persisted fields
- legacy `port=` remains accepted as a Remote Port alias
- Connection Mode contains exactly three explicit options: `LAN`, `Direct IP / DDNS`, `Tailscale`
- `Auto` is not exposed and is not serialized by the current model
- legacy `connection_mode=auto` is accepted only for migration and normalizes to `direct_ip_ddns`
- Direct IP / DDNS mode and Host/Remote Port UI are visible and persisted
- Share and Connect controls remain request-state shells only; no socket is opened and no real session is claimed

### Application icon

- canonical master PNG uses a transparent background
- master corner alpha validated as `0` at all four corners
- Windows ICO contains 16/24/32/48/64/128/256 pixel variants
- executable-associated 32x32 icon extracted from the Debug EXE with corner alpha `0`
- native main-window class reports non-null large/small icon handles
- the future FPS-015 installer is required to reuse this canonical ICO

### Audio capability

- Windows build 19045 reports Application Audio as unsupported
- unsupported selection is rejected/reverted rather than falsely claiming activation
- no WASAPI capture/playback occurs in FPS-004

### Window behavior

- F11 enters monitor-sized fullscreen
- Esc restores the prior placement
- fullscreen is not persisted
- borderless and always-on-top operate as real Win32 window states and persist as allowed preferences

### Diagnostics

- FPS-003 diagnostics service remains functional
- diagnostics completion callback restored after UI refactor
- Advanced view exposes diagnostics text plus Refresh/Save controls

## Exact-commit CI

Candidate commit:

`46e14dd2f4520243079548a854d47c42048fd220`

GitHub Actions run:

`38707483694` — Windows Build

Exact candidate result: **SUCCESS**

Qualified matrix:

- Windows x64 Debug — SUCCESS
- Windows x64 Release — SUCCESS

The exact candidate run completed successfully.

## Qualification decision

Local qualification is GREEN and exact-commit GitHub Actions is GREEN.

FPS-004 is qualified for closure and merge.
