# FPS-004 — Viewer UI & Settings Shell Implementation Plan

**Status:** APPROVED FOR EXECUTION

**Date:** 2026-10-15

**Issue:** #9 — FPS-004 — Viewer UI & Settings Shell

**Branch:** `fps-004-viewer-ui-settings-shell`

**Baseline:** `cf804561a4e935e2047b97ee9d7711e6ad83ab03`

## Goal

Turn the FPS-003 diagnostics window into the first viewer-first FriendPushScreen UI while keeping all media/network work out of scope.

FPS-004 must deliver:

- full native D3D11 viewer surface
- settings/control overlay toggled by `Tab`
- responsive/scrollable settings panel
- visible peer/share/connection/audio state
- fullscreen, borderless and always-on-top viewer behavior
- keyboard shortcuts
- persistent non-sensitive UI preferences
- Connection Mode + Host/Port shell
- sender Audio Source dropdown shell
- receiver mute/unmute + local volume shell
- diagnostics accessible from Advanced
- zero real capture/audio/network-media execution

## Architecture

### 1. Pure UI state model

Create `src/ui/ui_state.{h,cpp}`.

State owned independently from HWND:

- settings visibility
- fullscreen
- borderless
- always on top
- local share request state
- connection mode
- connection request state
- host
- port
- selected audio source
- application-audio capability
- receiver mute
- receiver volume
- diagnostics visibility

Important rule:

> runtime state and persistent preference state are not the same thing.

Never persist:

- active sharing
- connected/connecting state
- fullscreen transient state

This preserves the existing no-auto-share safety contract.

### 2. Preferences persistence

Create `src/ui/settings_store.{h,cpp}`.

Persist only safe local preferences:

- settings overlay visibility
- borderless
- always on top
- connection mode
- host
- port
- selected audio source kind
- receiver mute
- receiver volume

Default location:

`%LOCALAPPDATA%\FriendPushScreen\settings.ini`

Use versioned UTF-8 key/value serialization with tolerant parsing.

No pairing keys, passwords, transport secrets or private identity material belong in this file.

### 3. Viewer-first Win32 layout

The D3D11 render child remains the full client viewer surface.

Add overlay children over the render surface rather than shrinking the viewer.

Top status strip:

- Peer: Offline
- Connection: Disconnected
- Sharing: Not sharing
- Incoming Audio: Inactive

Settings overlay on the right:

- Share
- Connection
- Audio
- Viewer
- Advanced

Use native child controls.

A dedicated scrollable overlay container owns the settings controls.

### 4. Connection shell

Controls:

- Connection Mode combo: Auto / LAN / Direct IP-DDNS / Tailscale
- Host edit
- Port edit
- Connect request button
- connection status/path label

FPS-004 does not open a socket.

Pressing Connect records a UI request state and displays an explicit transport-not-active status rather than claiming a peer is connected.

### 5. Sender audio shell

Audio Source combo:

- No Audio
- Desktop / System Audio
- Device Source (capability populated in FPS-006A)
- Application Audio

Application Audio capability:

- Windows build >= 20348: capability target available
- lower build: explicit Unsupported explanation

FPS-004 does not call WASAPI.

### 6. Receiver audio shell

Controls:

- mute/unmute
- local volume 0–100
- incoming-audio state label

Mute and volume are local UI state only until FPS-006A/FPS-008 integrate audio playback/protocol.

### 7. Viewer modes

Implement actual native window behavior:

- `Tab`: show/hide settings overlay
- `F11`: enter/exit fullscreen
- `Esc`: exit fullscreen
- `Ctrl+Shift+T`: toggle Always on Top
- `Ctrl+Shift+S`: toggle local Share request shell
- Borderless toggle from settings

Fullscreen is transient and does not persist.

Always-on-top and borderless persist.

### 8. Diagnostics

Keep existing FPS-003 diagnostics service.

Move diagnostics text/Refresh/Save under Advanced and hide it unless requested.

No diagnostics regression is allowed.

## State semantics

### Share

FPS-004 states:

- Not sharing
- Share requested — media pipeline not implemented

The app must never claim real sharing in FPS-004.

### Connection

FPS-004 states:

- Disconnected
- Connect requested — transport not implemented

The app must never claim a real peer connection in FPS-004.

### Audio

FPS-004 states:

- No Audio
- Selected source — audio pipeline not active
- Unsupported capability

The app must never claim real audio capture in FPS-004.

## Test strategy

### Unit/state tests

RED before GREEN for:

- Tab visibility toggle
- fullscreen transition rules
- borderless/topmost state
- share request does not become real Sharing
- connection request does not become Connected
- volume clamping
- mute toggle
- connection mode serialization
- settings round-trip
- corrupt settings fallback
- persistent state excludes share/connect/fullscreen
- application-audio support rule by Windows build

### Physical UI qualification

On the reference Windows machine:

- launch app
- viewer surface renders through D3D11 Hardware
- Tab hides/shows overlay
- overlay is scrollable/responsive
- Connection Mode includes all four modes
- Host/Port visible
- Audio Source visible
- current Windows 10 build shows Application Audio unsupported
- mute/volume controls visible and interactive
- diagnostics Advanced view works
- F11 / Esc works
- borderless works
- always-on-top works
- resize/minimize/maximize works
- Ctrl+Shift+S changes only UI request state
- restart restores allowed preferences
- restart does not auto-share/connect/fullscreen
- clean close

### Build gates

- Debug local build + CTest
- Release local build + CTest
- `git diff --check`
- exact-head Windows CI Debug + Release

## Scope exclusions

FPS-004 must not add:

- monitor/window/capture-card frame capture
- WASAPI capture/playback
- video/audio encoder execution
- control protocol packets
- sockets or network media
- secure handshake implementation
- peer discovery
- real share/connection success state

Those remain owned by later FPS tasks.

## Task sequence

1. Activate FPS-004 docs/state.
2. RED/GREEN pure UI state model.
3. RED/GREEN settings persistence.
4. Build viewer/status/scrollable settings overlay.
5. Wire connection/audio/viewer controls.
6. Implement hotkeys/fullscreen/borderless/topmost.
7. Preserve diagnostics under Advanced.
8. Physical UX qualification.
9. Debug/Release + exact-head CI.
10. Evidence report, GREEN closure, PR/merge.

## Physical-review refinements accepted during FPS-004

The following refinements were added during physical review on the reference Windows machine and are part of the qualified FPS-004 contract.

### Separate local share port from remote connection port

The UI and persistence model use two independent fields:

- Share Port - the local port the future sharing/listen path will use.
- Remote Port - the port used with the remote Host when connecting.

The previous persisted port= key is accepted as a legacy alias for remote_port so existing FPS-004 test preferences are not discarded.

Neither field opens a socket in FPS-004.

### FriendPushScreen-owned dark theme

FriendPushScreen owns its application-control color palette through src/ui/theme.h and src/ui/theme.cpp.

The program-owned palette covers:

- settings panel background and border
- top status chrome
- buttons
- text/edit fields
- drop-down button and option list
- receiver-volume slider
- settings scrollbar
- primary/secondary text

The UI does not depend on Windows light/dark control colors for these surfaces.

The OS title bar can still use the supported Windows non-client dark-mode request; application content colors remain owned by FriendPushScreen.

### Visible Settings toggle outside the scroll container

Hide Settings / Settings is a persistent top-chrome button outside the scrollable settings panel.

It and the Tab shortcut operate on the same settings-visible state.

Show/hide forces immediate relayout and redraw of the parent/child hierarchy so reopening the panel does not require an additional click to repaint correctly.

### Program-owned drop-down option popup

Connection Mode and Audio Source use a FriendPushScreen owner-drawn button plus a dedicated owned popup window containing an owner-drawn list.

The popup is owned by the root FriendPushScreen window rather than the scrolling child panel. This guarantees the option list is above Host / Remote Port / Connect and the rest of the settings controls.

### Program-owned settings scrollbar

The native panel `WS_VSCROLL` chrome and Windows trackbar are not used for settings scrolling.

`FriendPushScreenSettingsScrollbar` is a dedicated FriendPushScreen child window that owns track/thumb painting, click/drag behavior and wheel forwarding. Its colors come only from the central FriendPushScreen palette.

Mouse-wheel settings scrolling uses a target offset plus a short timer-driven animation so wheel movement is smoother than direct jump scrolling. Direct scrollbar drag remains immediate and cancels any pending smooth animation.

### Program-owned receiver volume slider

The receiver volume control no longer uses the Windows trackbar.

`FriendPushScreenVolumeSlider` owns its own track/thumb painting, click/drag input and mouse-wheel behavior. Wheel up increases volume and wheel down decreases volume in 5-point increments.

The volume slider is vertically aligned to the same row and height as the Mute / Unmute button.

### Full-width status chrome and visible Settings shortcut

The status strip spans the full client width. The Settings button sits on the same top-chrome row at the right edge and remains above the strip in z-order.

The button text explicitly advertises the keyboard shortcut:

- visible panel: `Hide Settings (Tab)`
- hidden panel: `Settings (Tab)`

The status summary uses a small left text inset so `Peer:` is not visually flush against the left border.
