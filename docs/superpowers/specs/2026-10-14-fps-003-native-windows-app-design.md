# FPS-003 Native Windows App Skeleton & Hardware Diagnostics Design

**Status:** Written design for user review

**Date:** 2026-10-14

**Task:** FPS-003 — Windows App Skeleton & Hardware Diagnostics

**Repository:** `funggier/FriendPushScreen`

**Branch:** `fps-003-native-windows-app-design`

**Issue:** #5

**Master specification:** `docs/development-plan/ScreenLink_V1_Architecture_and_Development_Plan_TH.md`

**Prior evidence:** `docs/development/reports/FPS-20261014-002-reference-baseline-technical-spike.md`

---

## 1. Purpose

FPS-003 creates the first real FriendPushScreen Windows executable and the native foundation that later capture, codec, rendering, and network tasks will build on.

The executable must:

- launch on the project's Windows 10 x64 baseline
- create a native top-level application window
- initialize a D3D11 render surface
- gather real machine diagnostics through native Windows APIs
- display diagnostics in the application
- export the same diagnostics as UTF-8 text
- fail safely when optional capabilities are missing
- remain independent from streaming/network-video implementation

FPS-003 is not a streaming milestone. It establishes the application shell and proves that FriendPushScreen can understand the machine it is running on.

---

## 2. Architecture decision

### Selected stack

FriendPushScreen V1 will use the following application baseline:

```text
Language:          C++20
Application shell: Win32
Graphics:          D3D11 / DXGI
Build system:      CMake
Compiler family:   MSVC
Platform APIs:     native Windows APIs
UI baseline:       Win32 controls + D3D11 child/render surface
Architecture:      x64
Third-party UI:    none
Qt dependency:     none
```

This supersedes the master specification's provisional recommendation of C++ + Qt 6 for the V1 application shell.

It does not invalidate the master specification's architectural intent:

- UI remains separated from application/core services
- D3D11 remains the viewer/rendering path
- Media Foundation remains the preferred Windows media-device API
- capture/video/network systems remain replaceable behind explicit interfaces

### Why native Win32 is preferred for V1

FriendPushScreen V1 is Windows-only, and its critical paths are already native Windows technologies:

- Windows Graphics Capture
- DXGI Desktop Duplication
- D3D11
- Media Foundation
- Windows display/window APIs
- Windows networking and service APIs

Using a second application framework would add a deployment/runtime layer without removing the need for Win32 interop.

The V1 UI is intentionally small enough that Win32 controls are acceptable:

- viewer area
- status indicators
- diagnostics panel
- settings overlay in FPS-004
- fullscreen / borderless / always-on-top behavior

The video core must never depend on UI framework types. If a later version replaces the UI shell, graphics/media/core modules should not need redesign.

---

## 3. Toolchain baseline

### Validated locally

The reference machine currently has:

```text
Visual Studio Community 2026 18.10.2
MSVC tools:
  14.52.36725
  14.51.36231
CMake:
  4.4.2
Architecture:
  x64
```

### Source compatibility target

Project source should remain compatible with a conventional modern MSVC C++20 toolchain rather than require VS 2026-specific language extensions.

CMake must not hardcode a Visual Studio generator in project files.

Expected local generation may use:

```text
Visual Studio 18 2026
```

CI or other developers may use an older compatible Visual Studio generator.

### Windows SDK prerequisite

The reference machine currently exposes the Visual Studio Windows SDK component marker, but the standard Windows SDK Include/Lib directories are not presently available under the expected Windows Kits path.

Therefore the FPS-003 implementation has an explicit prerequisite:

> A complete Windows SDK with headers and libraries must be installed or repaired before compiling product code.

The implementation plan must verify this prerequisite first.

Required SDK policy:

- SDK must support the APIs required by Windows 10 Version 2004+
- compiling with a newer SDK is allowed
- runtime minimum remains Windows 10 Version 2004+
- runtime feature detection is required for optional/newer behavior
- source must not assume that compiling with a newer SDK raises the runtime minimum

---

## 4. Dependency policy

FPS-003 begins with **zero third-party runtime dependencies**.

Allowed baseline dependencies are Windows/Visual Studio platform components:

- User32
- Gdi32 only where required for standard Win32 window/control behavior
- D3D11
- DXGI
- DXGUID where required
- Media Foundation
- Media Foundation Platform
- Media Foundation ReadWrite only if enumeration requires it
- Windows networking/IP helper libraries
- Ole32 / COM
- Shell32 / Shlwapi only when justified
- standard C++ runtime

COM smart pointers should use SDK-provided facilities such as `Microsoft::WRL::ComPtr`.

FPS-003 must not add:

- Qt
- Electron
- Chromium
- .NET application runtime
- FFmpeg as a product runtime dependency
- Sunshine/Moonlight code
- JSON libraries
- logging libraries
- package managers solely for convenience

FFmpeg remains a qualification/probe tool, not product architecture.

---

## 5. Module architecture

The first source tree is organized by responsibility rather than one large Windows application file.

```text
FriendPushScreen/
├─ CMakeLists.txt
├─ cmake/
│  └─ warnings.cmake
├─ src/
│  ├─ app/
│  │  ├─ main.cpp
│  │  ├─ application.h
│  │  └─ application.cpp
│  │
│  ├─ ui/
│  │  ├─ main_window.h
│  │  └─ main_window.cpp
│  │
│  ├─ graphics/
│  │  ├─ d3d11_device.h
│  │  ├─ d3d11_device.cpp
│  │  ├─ placeholder_renderer.h
│  │  └─ placeholder_renderer.cpp
│  │
│  ├─ diagnostics/
│  │  ├─ diagnostics_snapshot.h
│  │  ├─ diagnostics_service.h
│  │  ├─ diagnostics_service.cpp
│  │  ├─ diagnostics_formatter.h
│  │  └─ diagnostics_formatter.cpp
│  │
│  ├─ platform/
│  │  └─ windows/
│  │     ├─ os_probe.*
│  │     ├─ cpu_memory_probe.*
│  │     ├─ dxgi_adapter_probe.*
│  │     ├─ monitor_probe.*
│  │     ├─ window_probe.*
│  │     └─ network_probe.*
│  │
│  └─ media/
│     ├─ capture_device_probe.*
│     └─ codec_capability_probe.*
│
├─ tests/
│  ├─ test_main.cpp
│  ├─ diagnostics_formatter_tests.cpp
│  └─ platform_filter_tests.cpp
│
└─ docs/
```

The `.*` notation means a focused header/source pair where the implementation requires both.

No module may include another module's private implementation file.

---

## 6. Component responsibilities

### 6.1 `app`

Owns:

- WinMain entry point
- COM initialization
- Media Foundation startup/shutdown when required
- top-level application lifetime
- coordination of UI, graphics, and diagnostics

Does not own:

- machine enumeration logic
- DXGI enumeration logic
- Media Foundation device enumeration logic
- formatting diagnostics text

### 6.2 `ui`

Owns:

- top-level HWND
- child controls
- layout
- menu/commands
- diagnostics presentation
- Save Diagnostics command
- resize/fullscreen hooks needed by the shell

The UI receives already-structured state.

The UI must not query WMI, DXGI, Media Foundation, network adapters, registry, or codec capabilities directly.

### 6.3 `graphics`

Owns:

- D3D11 device/context lifetime for the application rendering path
- swap chain for the viewer/render child HWND
- resize handling
- placeholder frame presentation
- device-lost detection/reporting

FPS-003 renders only a deterministic placeholder/test surface.

No capture frame enters this module in FPS-003.

### 6.4 `diagnostics`

Owns:

- immutable diagnostic snapshot model
- invoking individual probe interfaces
- aggregation
- status/error reporting
- conversion to stable human-readable UTF-8 text

The snapshot is the contract between platform/media probes and the UI/export layer.

### 6.5 `platform/windows`

Owns Windows-specific machine discovery:

- OS build
- CPU model and architecture
- physical memory
- DXGI adapters
- monitors/displays
- visible top-level windows
- network adapters
- Tailscale adapter/service presence where detectable through native Windows state

The platform layer returns data. It does not render UI.

### 6.6 `media`

Owns media-capability discovery only:

- video capture-device enumeration through Media Foundation
- encoder capability enumeration
- decoder capability enumeration
- hardware/software classification when the Windows API exposes reliable evidence

FPS-003 does not open a capture stream and does not encode/decode frames.

---

## 7. Diagnostics data contract

The central model is an immutable `DiagnosticsSnapshot`.

Conceptually:

```text
DiagnosticsSnapshot
├─ generated_at
├─ app_version
├─ os
├─ cpu
├─ memory
├─ graphics_adapters[]
├─ monitors[]
├─ visible_windows[]
├─ capture_devices[]
├─ codec_encoders[]
├─ codec_decoders[]
├─ network_adapters[]
├─ tailscale
└─ probe_results[]
```

Every probe result has an explicit state:

```text
Available
Unavailable
Unsupported
Error
```

A missing optional capability is not represented as a thrown exception or application failure.

For errors, the snapshot records:

- probe name
- stable error category
- HRESULT / Win32 error when applicable
- short human-readable message

Diagnostics output must never contain:

- captured frames
- screenshots
- passwords
- access tokens
- Tailscale authentication material
- arbitrary file contents

---

## 8. Native probe strategy

### 8.1 OS

Use a version query that reports the real OS build and is not distorted by application compatibility manifest behavior.

The implementation may dynamically call `RtlGetVersion` for this purpose.

Target evidence:

- Windows edition where available
- major/minor/build
- x64 architecture

### 8.2 CPU and memory

Use native Windows/registry/system calls for:

- CPU architecture
- logical processor count
- processor model string
- total physical memory

No WMI dependency is required for the initial implementation.

### 8.3 GPU / DXGI adapters

Use DXGI enumeration.

For each adapter record at minimum:

- description
- vendor ID
- device ID
- dedicated video memory
- shared system memory
- software-adapter flag

Do not infer codec capability solely from GPU vendor.

### 8.4 Monitors

Use native display/monitor enumeration.

Record:

- device/display name
- desktop rectangle
- primary flag
- effective current resolution where obtainable

### 8.5 Windows

Enumerate visible top-level application windows.

Filter at minimum:

- invisible windows
- windows without a usable title
- FriendPushScreen's own diagnostics child controls

The enumeration is diagnostic only. FPS-005 will define the final shareable-window policy.

### 8.6 Capture devices

Use Media Foundation device-source enumeration.

Record:

- friendly name
- symbolic link/device identity when available
- enumeration status

The FPS-003 snapshot must be able to show the current `USB3 Video` reference device without hardcoding that name or VID/PID.

### 8.7 Codec capabilities

Use Media Foundation Transform enumeration or equivalent native Windows capability APIs.

Probe at minimum:

- H.264 encoder availability
- H.264 decoder availability
- HEVC encoder availability
- HEVC decoder availability
- AV1 encoder availability
- AV1 decoder availability

Where hardware classification cannot be established reliably, report `Unknown` rather than guessing from Intel/AMD/NVIDIA identity.

### 8.8 Network / Tailscale

Use native network-adapter enumeration for baseline network state.

Record:

- active adapters
- operational state
- IPv4 addresses
- link characteristics available from the API
- presence of a Tailscale adapter/address

FPS-003 does not depend on invoking `tailscale.exe` for basic application startup.

Detailed Direct/Relay/peer-state integration belongs to FPS-009/FPS-010.

---

## 9. D3D11 placeholder renderer

The FPS-003 viewer region proves the rendering foundation before real video frames exist.

Behavior:

1. create the render child HWND
2. create a D3D11 hardware device
3. create a DXGI swap chain for that HWND
4. render a deterministic placeholder/test frame
5. resize swap-chain buffers when the client area changes
6. present continuously or on invalidation at a bounded rate
7. report device creation/present failures through application state/logging

The placeholder should visually prove:

- the correct child area is being rendered
- resize works
- DPI/window layout remains usable

It must not attempt video decode or screen capture.

### Hardware failure behavior

If normal D3D11 hardware-device creation fails:

- record the failure in diagnostics
- allow an explicit WARP fallback for the **application placeholder/UI validation only**
- visibly report that WARP is in use

WARP fallback must never be interpreted as satisfying later hardware-video qualification.

---

## 10. UI design for FPS-003

The first window is diagnostic-first, not the final FPS-004 viewer UX.

Recommended layout:

```text
┌──────────────────────────────────────────────────────────┐
│ FriendPushScreen                         [Diagnostics]   │
├──────────────────────────────────────┬───────────────────┤
│                                      │ System            │
│       D3D11 Placeholder Surface      │ GPU               │
│                                      │ Capture Devices   │
│                                      │ Codecs            │
│                                      │ Network           │
│                                      │                   │
│                                      │ [Refresh]         │
│                                      │ [Save Report]     │
└──────────────────────────────────────┴───────────────────┘
```

The diagnostic panel may use ordinary Win32 controls.

FPS-003 prioritizes:

- correctness
- resize behavior
- legible state
- deterministic diagnostics

FPS-004 owns final viewer/settings interaction design.

---

## 11. Diagnostics refresh flow

Diagnostics must not permanently block the UI thread.

Preferred flow:

```text
UI command / startup
      ↓
DiagnosticsService starts one collection job
      ↓
platform/media probes execute
      ↓
immutable DiagnosticsSnapshot produced
      ↓
result posted back to UI thread
      ↓
UI replaces old snapshot atomically
      ↓
same snapshot may be exported to text
```

Only one diagnostics collection job may be active at a time.

If Refresh is pressed while collection is running:

- do not launch duplicate probes
- keep the current result visible
- show a collecting state

---

## 12. Diagnostics export

The UI provides **Save Diagnostics**.

Output:

- UTF-8 text
- human-readable sections
- stable enough to attach to GitHub issues or qualification reports
- generated from the exact `DiagnosticsSnapshot` currently shown in the UI

Suggested sections:

```text
FriendPushScreen Diagnostics
Application
Operating System
CPU / Memory
Graphics Adapters
Monitors
Capture Devices
Codec Capabilities
Network
Tailscale
Probe Warnings / Errors
```

No binary dump or frame data is exported.

---

## 13. Error-handling model

### Non-fatal probe failures

A failure to enumerate any optional subsystem must not terminate the application.

Examples:

- no capture device
- no Tailscale adapter
- no AV1 encoder
- Media Foundation device enumeration failure
- network adapter query failure

These appear in diagnostics as `Unavailable`, `Unsupported`, or `Error`.

### Fatal application failures

Only failures that make the shell fundamentally unusable should stop startup, such as:

- top-level window creation failure
- unrecoverable message-loop setup failure

D3D11 hardware creation failure is not fatal if WARP placeholder fallback can start.

### No silent substitution

When a fallback is used, diagnostics/UI must state which fallback is active.

---

## 14. Threading and ownership

### UI thread

Owns:

- HWND hierarchy
- Win32 message loop
- UI control mutation
- swap-chain/window resize coordination

### Diagnostics worker

Owns:

- one snapshot collection job at a time
- blocking native enumeration that should not delay window interaction

### Graphics ownership

D3D11 device/context ownership remains explicit inside the graphics module.

Cross-thread access to D3D resources should not be introduced in FPS-003 unless required.

No permanent worker pool is required.

---

## 15. Logging baseline

FPS-003 should include a minimal internal logger because later video/network qualification requires reproducible evidence.

Initial log levels:

- Info
- Warning
- Error

Log destination:

```text
%LOCALAPPDATA%\FriendPushScreen\logs\
```

Requirements:

- UTF-8 text
- timestamp
- severity
- subsystem
- no frame contents
- no authentication secrets
- failure to create the log file must not prevent the app from launching

No third-party logging dependency is required.

---

## 16. Configuration policy

FPS-003 does not need a full settings subsystem.

The implementation may define application/version constants but should not create a speculative configuration schema merely to satisfy future needs.

FPS-004 will introduce settings persistence when there are real settings to persist.

Invariant preserved from the V1 specification:

> FriendPushScreen must never auto-start sharing merely because it restarted.

---

## 17. Build structure and compiler policy

Top-level CMake requirements:

- C++20
- x64 build
- Unicode
- `UNICODE` / `_UNICODE`
- warning level equivalent to MSVC `/W4`
- `/permissive-`
- `/Zc:__cplusplus`
- `/utf-8`
- no warning-as-error policy until the baseline compiles cleanly across supported MSVC versions; CI may enable it later deliberately

The project should use focused CMake targets rather than one monolithic target when this improves testability.

Expected targets:

```text
FriendPushScreen
FriendPushScreenTests
```

Platform-only implementation remains guarded for Windows.

---

## 18. Testing strategy

FPS-003 uses two levels of tests.

### 18.1 Unit tests

Pure or mostly-pure logic:

- diagnostics text formatting
- state-to-text mapping
- window-list filtering rules
- adapter/device classification helpers
- error formatting

The initial baseline may use a small in-repository test executable registered with CTest rather than adding a third-party testing framework.

### 18.2 Windows integration/smoke tests

Runtime tests on Windows:

- OS probe returns a valid build
- DXGI returns at least one adapter on the reference machine
- monitor enumeration returns at least one monitor
- capture-device enumeration completes whether zero or many devices exist
- codec enumeration completes without crash
- network enumeration completes
- application executable starts and creates its window
- D3D11 placeholder initializes on the reference machine
- diagnostics export produces non-empty UTF-8 text

Integration tests must distinguish environment-dependent absence from implementation failure.

Example:

```text
"No capture devices found" = valid test result
"Capture-device enumeration API failed unexpectedly" = failure
```

---

## 19. CI baseline

FPS-003 should introduce a Windows x64 GitHub Actions build after local build is proven.

CI responsibilities:

- configure CMake
- build Release and/or Debug x64
- run CTest
- retain build failure logs

CI is not expected to prove:

- hardware QSV
- physical capture-card enumeration
- D3D performance
- real monitor/window topology
- Tailscale presence

Those remain physical qualification evidence.

Source/build files must therefore keep CI-compatible logic separate from physical-machine qualification.

---

## 20. Security and privacy constraints

Diagnostics are local by default.

FPS-003 must not:

- upload diagnostics automatically
- send telemetry
- contact a cloud service
- enumerate arbitrary file contents
- capture pixels
- store screenshots
- store Tailscale credentials
- expose raw access tokens in logs/export

Visible window titles are potentially sensitive. They may be shown locally in the diagnostics UI for development, but exported diagnostics should make window-title export an explicit design choice rather than silently include all titles.

For FPS-003 default export:

> export the **count and basic enumeration status**, not the complete visible-window title list.

---

## 21. Acceptance criteria

FPS-003 is eligible for GREEN when all applicable conditions are proven on the reference machine.

### Build

- clean checkout configures through documented CMake commands
- x64 executable builds with MSVC C++20
- CTest passes
- Windows CI build passes on the exact commit

### Application

- executable launches without administrator privilege
- main window opens
- D3D11 placeholder surface is visible
- resize does not crash
- closing the window exits cleanly

### Diagnostics

The UI displays or reports:

- Windows build
- CPU model
- memory
- DXGI adapter(s)
- monitor(s)
- visible-window enumeration status
- Media Foundation capture device(s)
- H.264/HEVC/AV1 encoder capability status
- H.264/HEVC/AV1 decoder capability status
- network adapter state
- Tailscale presence/state available from native local evidence

### Reference hardware expectations

On the current reference machine, qualification should observe:

- Intel Graphics adapter
- `USB3 Video` reference capture device when connected
- Intel media capability entries where the native API exposes them
- Tailscale adapter when active

These values are qualification expectations, not hardcoded product rules.

### Export

- Save Diagnostics writes a non-empty UTF-8 report
- export contains no captured image/frame data
- export contains no authentication secrets

### Scope

FPS-003 must not contain:

- actual screen capture
- actual window capture
- capture-card frame streaming
- video encode/decode pipeline
- network video transport
- share/stop-share behavior
- audio streaming
- remote control

---

## 22. Deferred work

### FPS-004

- final viewer UI shell
- settings overlay
- fullscreen/borderless/always-on-top behavior
- hotkeys
- settings persistence
- final state indicators

### FPS-005

- WGC / Desktop Duplication implementation and comparison
- final shareable-window policy
- capture-card frame acquisition
- Media Foundation capture modes
- source loss / hot-plug behavior

### FPS-006

- encoder/decoder abstraction
- hardware codec execution
- local loopback
- latency/bounded-queue measurements

### FPS-009 / FPS-010

- network transport
- two-peer behavior
- Tailscale Direct/DERP
- real end-to-end connection qualification

---

## 23. Decision consequences

Choosing Native C++/Win32 for V1 means:

### Benefits

- direct fit with D3D11, DXGI, Media Foundation, WGC, and Win32 window APIs
- fewer runtime/deployment dependencies
- smaller licensing surface
- no UI framework/native-video interop boundary
- video core remains easy to isolate from UI

### Costs

- more UI/layout code than Qt
- accessibility/high-DPI behavior must be handled deliberately
- FPS-004 will require disciplined Win32 UI engineering
- custom modern visual polish is not a V1 priority

This trade is accepted because FriendPushScreen V1 prioritizes video correctness, latency, stability, and inspectable native behavior over UI-framework convenience.

---

## 24. Binding design rules

For FPS-003 implementation:

1. native C++20/Win32 is the application baseline
2. D3D11/DXGI is the rendering foundation
3. Media Foundation is used for native media capability/device enumeration
4. no Qt dependency is introduced
5. no FFmpeg runtime dependency is introduced
6. UI does not own hardware enumeration logic
7. platform/media probes do not own UI
8. diagnostics use one immutable snapshot contract
9. optional capability absence never crashes startup
10. no streaming path is implemented in FPS-003
11. the missing Windows SDK must be installed/repaired before compilation
12. physical-machine evidence and CI evidence remain distinct
