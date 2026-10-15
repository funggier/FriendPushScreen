# FPS-20261014-003 — Windows App Skeleton & Hardware Diagnostics

## Status

**FPS-003: GREEN / CLOSED — NATIVE WINDOWS APPLICATION BASELINE**

Started: 2026-10-14

Qualified: 2026-10-15

Repository: `funggier/FriendPushScreen`

Branch: `fps-003-native-windows-app-design`

GitHub Issue: #5 — FPS-003 — Windows App Skeleton & Hardware Diagnostics

Implementation-qualified commit:

```text
741b206ed29fe8ac9b3b637015519d5b4f4b9096
```

The final documentation/closure commit is the commit containing this report. Git/GitHub exact-head evidence remains authoritative for that final SHA.

---

## 1. Outcome

FPS-003 establishes the first real FriendPushScreen Windows executable.

The implemented baseline is:

```text
C++20
Win32 application shell
D3D11 / DXGI rendering foundation
Media Foundation capability/device enumeration
native Windows system/network probes
CMake / MSVC x64
CTest
GitHub Actions Windows x64 CI
```

No Qt or FFmpeg runtime dependency is present.

FPS-003 intentionally does **not** implement screen/window frame capture, capture-card frame streaming, video encode/decode execution, network video transport, audio streaming, or remote input.

---

## 2. Toolchain qualification

The reference machine was missing a complete Windows SDK at FPS-003 start.

A supported Microsoft Windows 10 SDK was installed and verified:

```text
Windows SDK: 10.0.19041.0
```

Required files verified present:

- `Windows.h`
- `d3d11.h`
- `mfapi.h`
- `User32.lib`
- `d3d11.lib`
- `mfplat.lib`

Validated local tools:

- Visual Studio Community 2026 / Visual Studio 18 generator
- MSVC x64
- CMake 4.4.x
- Windows SDK 10.0.19041.0

Build instructions are recorded in `docs/BUILDING.md`.

---

## 3. Clean local build qualification

A generated `build\` directory was removed and recreated from scratch without resetting or cleaning the Git worktree.

Commands represented by the qualification run:

```powershell
cmake -S . -B build -A x64

cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure

cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Observed:

```text
Fresh configure: PASS
Debug build: PASS
Debug CTest: 1/1 PASS
Release build: PASS
Release CTest: 1/1 PASS
Qualification process exit: 0
```

The clean qualification used Windows SDK 10.0.19041.0 and MSVC.

---

## 4. Automated test coverage

FPS-003 tests cover:

- diagnostics state formatting
- UTF-8/Thai formatter behavior
- privacy-safe default diagnostics export
- logger smoke behavior using an injected temporary path
- OS version normalization and failure handling
- CPU-model normalization and memory-query failure handling
- live OS/CPU/memory probe sanity
- visible-window filtering
- DXGI/monitor enumeration on the physical reference machine
- codec capability normalization without GPU-vendor guessing
- Media Foundation capture/device enumeration completion
- native network/Tailscale classification
- primary physical adapter selection
- renderer state mapping: Hardware / WARP fallback / Error
- diagnostics single-flight refresh behavior
- partial-probe failure containment
- UTF-8 diagnostics export
- Win32 multiline LF→CRLF presentation normalization

CI-compatible tests separate physical-hardware expectations from hosted-runner expectations.

---

## 5. Physical application qualification

Reference machine:

```text
OS: Windows 10 Pro build 19045
CPU: Intel(R) Core(TM) Ultra 5 245K
RAM: 33,783,828,480 bytes
GPU: Intel(R) Graphics
Primary display: 2560×1440
Secondary display observed: 1920×1080
```

### Launch

The Debug application was launched in the interactive user session without administrator privilege.

Observed window title:

```text
FriendPushScreen — D3D11 Hardware
```

Result: **PASS**

### D3D11 placeholder

A D3D11 hardware device and DXGI swap chain initialized successfully.

The placeholder surface was visually verified and remained visible through resize/lifecycle testing.

Renderer status:

```text
D3D11 Hardware
```

Result: **PASS**

WARP remains an explicit fallback for shell validation only and is not counted as hardware qualification.

### Window lifecycle

Physically exercised:

- normal launch
- resize from 1100×700 to 1280×760 during Task 7 smoke
- minimize
- maximize/restore behavior
- move/resize
- `WM_CLOSE`
- clean process exit

Result: **PASS**

---

## 6. Physical diagnostics evidence

The final diagnostics UI was read through Win32 `WM_GETTEXT` / `WM_GETTEXTLENGTH`.

Important qualification note:

> Cross-process `GetWindowTextW` can return stale/cached child-control caption text. `WM_GETTEXT` is the authoritative smoke-probe method used for the multiline diagnostics control.

Observed live diagnostics included:

### Operating system

```text
State: Available
Build: 19045
Architecture: x64
```

### CPU / memory

```text
CPU: Intel(R) Core(TM) Ultra 5 245K
Logical processors: 14
Physical memory bytes: 33783828480
```

### Graphics

```text
Intel(R) Graphics
Microsoft Basic Render Driver
```

### Monitors

```text
DISPLAY1 / 2560×1440 / Primary
DISPLAY2 / 1920×1080
```

### Capture device

```text
USB3 Video
```

The device name is discovered through Media Foundation and is not hardcoded.

### Codec capability status

The UI reports explicit status for all six V1 capability directions:

- H.264 Encode
- H.264 Decode
- HEVC Encode
- HEVC Decode
- AV1 Encode
- AV1 Decode

On this machine Media Foundation exposed H.264/HEVC transforms, including Intel hardware encoders. AV1 was reported as `Unavailable` through the native Media Foundation capability view.

This does **not** contradict FPS-002's FFmpeg/QSV AV1 execution probe: the two probes measure different API exposure surfaces. FPS-006 remains responsible for final codec backend execution/selection.

### Network

Native IP Helper enumeration reported active adapters including:

- Tailscale
- physical Ethernet
- loopback
- virtual adapters

Observed:

```text
Tailscale: Present
```

The application does not invoke `tailscale.exe` during ordinary startup.

### Probe status

Final physical UI showed:

```text
Probe Warnings / Errors
None
```

Result: **PASS**

---

## 7. Asynchronous diagnostics behavior

`DiagnosticsService` uses one worker collection at a time.

Automated evidence proves:

- first collection starts
- a second refresh request while collection is active is rejected
- only one probe run occurs
- the existing snapshot remains readable
- one failing probe does not discard successful subsystem data
- completed snapshot is immutable to consumers
- UI mutation occurs on the UI thread after `WM_FPS_DIAGNOSTICS_READY`

Result: **PASS**

---

## 8. Diagnostics UI

FPS-003 includes a diagnostic-first UI:

- D3D11 viewer/placeholder area
- multiline diagnostic panel
- `Refresh`
- `Save Diagnostics`

Win32 multiline presentation normalizes LF to CRLF only at the UI boundary so the panel is readable without changing the stable UTF-8 export format.

Physical screenshot evidence was captured in the ignored execution workspace during qualification.

Result: **PASS**

---

## 9. Diagnostics export and privacy

The Save Diagnostics button was exercised through the real Win32 Save dialog.

Physical output:

```text
task8-saved-diagnostics.txt
size observed: 1529 bytes
```

Validated properties:

- non-empty UTF-8 text
- FriendPushScreen diagnostics header present
- OS/CPU/GPU/monitor information present
- capture device status present
- codec status present
- network/Tailscale status present
- visible-window count/status present
- complete visible-window titles omitted by default
- no captured image/frame data
- no authentication token or Tailscale credential export

A real visible title such as `New Tab - Google Chrome` was present in the local UI snapshot but absent from the saved report.

Result: **PASS**

---

## 10. Runtime logging

Application log location:

```text
%LOCALAPPDATA%\FriendPushScreen\logs\FriendPushScreen.log
```

Logging is:

- UTF-8
- timestamped
- severity/subsystem tagged
- non-fatal if the file cannot be opened

No third-party logger is used.

---

## 11. GitHub Actions exact-commit evidence

Workflow:

```text
Windows Build
```

Implementation-qualified SHA:

```text
741b206ed29fe8ac9b3b637015519d5b4f4b9096
```

GitHub Actions run:

```text
Run ID: 38631397900
Event: push
Status: completed
Conclusion: success
```

Jobs:

```text
Windows x64 Debug   — success
Windows x64 Release — success
```

Both jobs completed checkout, CMake configure, build, and CTest successfully.

---

## 12. License

FriendPushScreen now contains a root `LICENSE` file using the **MIT License**.

Copyright notice:

```text
Copyright (c) 2026 FriendPushScreen contributors
```

README links to the license.

Third-party components remain subject to their own licenses and DG-006 before integration/distribution.

---

## 13. Architecture decision consequence

DG-005 — UI/application framework is resolved by FPS-003:

```text
Native C++20 + Win32 + D3D11/DXGI + CMake
```

Reasons proven in implementation:

- direct integration with native Windows APIs
- successful D3D11 physical rendering
- no Qt runtime/deployment dependency
- successful Windows 10 physical runtime
- successful hosted Windows CI build/test
- clean separation between UI and diagnostics/platform/media modules

---

## 14. Scope audit

Source audit found no prohibited FPS-003 implementation drift.

Not present:

- Qt runtime/linkage
- FFmpeg runtime/linkage
- WGC frame capture
- DXGI Desktop Duplication frame capture
- Media Foundation source-reader frame acquisition
- video encode/decode execution pipeline
- video transport sockets/send/recv
- audio streaming
- remote keyboard/mouse input

Present media calls are capability/enumeration only:

- `MFEnumDeviceSources`
- `MFTEnumEx`

D3D11 execution is limited to the application placeholder renderer.

Result: **PASS**

---

## 15. Qualification conclusion

FPS-003 satisfies its build, application, diagnostics, export, CI, privacy, and scope gates.

```text
FPS-003 = GREEN / CLOSED
Native Windows application baseline = QUALIFIED
Next planned task = FPS-004 — Viewer UI & Settings Shell
```

No development task is active after FPS-003 closure.
