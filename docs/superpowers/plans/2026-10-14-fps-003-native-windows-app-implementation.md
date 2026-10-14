# FPS-003 Native Windows App Skeleton & Hardware Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first launchable FriendPushScreen Windows x64 executable with a Win32 shell, D3D11 placeholder renderer, native hardware/system diagnostics, asynchronous refresh, UTF-8 diagnostics export, tests, CI, and physical-machine qualification evidence.

**Architecture:** The executable uses a Win32 UI shell and D3D11/DXGI rendering foundation. Hardware/system discovery is isolated behind focused Windows and media probe modules that produce one immutable `DiagnosticsSnapshot`; UI and export consume that snapshot without owning enumeration logic. FPS-003 contains no real capture stream, codec execution pipeline, network-video transport, audio streaming, or remote control.

**Tech Stack:** C++20, Win32, D3D11, DXGI, Media Foundation, IP Helper API, WRL `ComPtr`, CMake, MSVC, CTest, GitHub Actions Windows x64.

**Spec:** `docs/superpowers/specs/2026-10-14-fps-003-native-windows-app-design.md`

## Global Constraints

- Application baseline is native C++20 + Win32.
- Rendering foundation is D3D11 / DXGI.
- Media Foundation is used for native media device/capability enumeration.
- Architecture is Windows x64.
- No Qt dependency.
- No FFmpeg runtime dependency.
- No .NET application runtime.
- No Sunshine/Moonlight product code.
- No third-party JSON, logging, or test framework is introduced in FPS-003.
- UI must not own hardware enumeration logic.
- Probe modules must not own UI.
- Diagnostics are represented through one immutable snapshot contract.
- Missing optional capabilities are reported, not treated as fatal startup failures.
- No actual screen/window/capture-card frame acquisition is implemented.
- No video encode/decode execution pipeline is implemented.
- No network-video transport is implemented.
- No audio streaming or remote control is implemented.
- A complete Windows SDK must be installed/repaired before product compilation begins.
- Physical-machine qualification and CI qualification remain separate evidence classes.
- Diagnostics export must not contain captured pixels, credentials, tokens, or full visible-window title lists by default.

## Review Focus

- **Windows SDK incomplete or absent:** build bootstrap must fail clearly before source compilation, then pass after SDK repair/install; owned by Task 1.
- **No optional hardware/service present:** diagnostics collection must still complete with `Unavailable`/`Unsupported` rather than crash; owned by Tasks 3–6.
- **Duplicate refresh requests:** a second diagnostics refresh while collection is active must not start a parallel collection job; owned by Task 8.
- **D3D11 hardware-device failure/device-loss path:** shell must visibly report WARP fallback or renderer error without claiming hardware qualification; owned by Task 7.
- **Potentially sensitive window titles:** UI may show local development data, but default exported report must omit complete visible-window titles; owned by Tasks 4 and 8.

---

## File Structure Locked by This Plan

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
│  ├─ ui/
│  │  ├─ main_window.h
│  │  └─ main_window.cpp
│  ├─ graphics/
│  │  ├─ d3d11_device.h
│  │  ├─ d3d11_device.cpp
│  │  ├─ placeholder_renderer.h
│  │  └─ placeholder_renderer.cpp
│  ├─ diagnostics/
│  │  ├─ diagnostics_snapshot.h
│  │  ├─ diagnostics_service.h
│  │  ├─ diagnostics_service.cpp
│  │  ├─ diagnostics_formatter.h
│  │  ├─ diagnostics_formatter.cpp
│  │  ├─ logger.h
│  │  └─ logger.cpp
│  ├─ platform/windows/
│  │  ├─ os_probe.h
│  │  ├─ os_probe.cpp
│  │  ├─ cpu_memory_probe.h
│  │  ├─ cpu_memory_probe.cpp
│  │  ├─ dxgi_adapter_probe.h
│  │  ├─ dxgi_adapter_probe.cpp
│  │  ├─ monitor_probe.h
│  │  ├─ monitor_probe.cpp
│  │  ├─ window_probe.h
│  │  ├─ window_probe.cpp
│  │  ├─ network_probe.h
│  │  └─ network_probe.cpp
│  └─ media/
│     ├─ capture_device_probe.h
│     ├─ capture_device_probe.cpp
│     ├─ codec_capability_probe.h
│     └─ codec_capability_probe.cpp
├─ tests/
│  ├─ test_main.cpp
│  ├─ test_support.h
│  ├─ diagnostics_formatter_tests.cpp
│  ├─ platform_filter_tests.cpp
│  └─ diagnostics_service_tests.cpp
├─ .github/workflows/
│  └─ windows-build.yml
└─ docs/
   ├─ BUILDING.md
   └─ development/reports/
      └─ FPS-20261014-003-windows-app-skeleton-hardware-diagnostics.md
```

---

### Task 1: Toolchain Prerequisite, CMake Baseline, and Test Harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/warnings.cmake`
- Create: `tests/test_main.cpp`
- Create: `tests/test_support.h`
- Create: `docs/BUILDING.md`
- Modify: `docs/development/tasks/CURRENT.md`

**Interfaces:**
- Consumes: approved FPS-003 spec and local Visual Studio/MSVC/CMake installation.
- Produces: CMake targets `FriendPushScreen` and `FriendPushScreenTests`; helper macros `FPS_EXPECT_TRUE(expr)` and `FPS_EXPECT_EQ(actual, expected)`; documented configure/build/test commands used by all later tasks.

- [ ] **Step 1: Verify and repair/install the Windows SDK prerequisite**

Run PowerShell checks for:
- `C:\Program Files (x86)\Windows Kits\10\Include\<version>\um\Windows.h`
- corresponding `Lib\<version>\um\x64` libraries
- Visual Studio C++ x64 tools

If the SDK headers/libs are missing, install or repair a stable Windows SDK through Visual Studio Installer / Microsoft package tooling, then rerun the checks.

Expected: at least one complete SDK version with `um\Windows.h`, `um\d3d11.h`, `um\mfapi.h`, and x64 import libraries.

- [ ] **Step 2: Create the smallest failing CTest harness**

`tests/test_main.cpp` must register a named smoke assertion whose expected value is deliberately false until the test-support helper is wired.

Run:
```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: test target builds, CTest runs, and the deliberate smoke assertion FAILS.

- [ ] **Step 3: Implement CMake/test-support baseline**

Required CMake properties:
- `CMAKE_CXX_STANDARD 20`
- `CMAKE_CXX_STANDARD_REQUIRED ON`
- Unicode definitions `UNICODE` and `_UNICODE`
- MSVC options `/W4 /permissive- /Zc:__cplusplus /utf-8`
- x64 documented as the supported configuration
- `enable_testing()`
- `add_test(NAME FriendPushScreenTests COMMAND FriendPushScreenTests)`

`tests/test_support.h` provides the two minimal assertion helpers and a failure counter used by `test_main.cpp`.

- [ ] **Step 4: Run baseline verification**

Run:
```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: configure PASS, build PASS, one test PASS.

- [ ] **Step 5: Document exact local build commands**

`docs/BUILDING.md` records:
- prerequisite SDK
- Visual Studio/MSVC requirement
- CMake configure/build/test commands
- Debug and Release commands
- output executable/test locations

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt cmake tests docs/BUILDING.md docs/development/tasks/CURRENT.md
git commit -m "build: establish native Windows toolchain"
```

---

### Task 2: Diagnostics Model, Formatter, and Logger

**Files:**
- Create: `src/diagnostics/diagnostics_snapshot.h`
- Create: `src/diagnostics/diagnostics_formatter.h`
- Create: `src/diagnostics/diagnostics_formatter.cpp`
- Create: `src/diagnostics/logger.h`
- Create: `src/diagnostics/logger.cpp`
- Create: `tests/diagnostics_formatter_tests.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/test_main.cpp`

**Interfaces:**
- Consumes: Task 1 test harness.
- Produces:
  - `enum class ProbeState { Available, Unavailable, Unsupported, Error };`
  - `struct ProbeResult { std::string name; ProbeState state; long error_code; std::string message; };`
  - `struct DiagnosticsSnapshot` with sections defined by the spec.
  - `std::string FormatDiagnosticsText(const DiagnosticsSnapshot& snapshot, bool include_window_titles = false);`
  - `class Logger { public: static Logger& Instance(); void Log(LogLevel level, std::string_view subsystem, std::string_view message); };`

- [ ] **Step 1: Write failing formatter tests**

Tests must assert:
- all four `ProbeState` values produce stable text
- application/OS/CPU/GPU/capture/codecs/network section headings exist
- `include_window_titles=false` omits individual window titles but includes visible-window count/status
- an error result includes its numeric error code and message
- UTF-8 text survives Thai characters in a synthetic probe message

Run:
```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Expected: FAIL because diagnostics types/formatter do not exist.

- [ ] **Step 2: Implement snapshot types and formatter**

Use value types and `std::vector`/`std::optional` only; no JSON or UI dependencies.

The formatter must be deterministic for a fixed snapshot.

- [ ] **Step 3: Run formatter tests**

Expected: PASS.

- [ ] **Step 4: Add minimal logger**

Logger destination policy:
```text
%LOCALAPPDATA%\FriendPushScreen\logs\FriendPushScreen.log
```

Required behavior:
- UTF-8
- timestamp
- level
- subsystem
- file-open failure is non-fatal

Do not rotate logs in FPS-003.

- [ ] **Step 5: Add logger smoke test**

Test a temporary redirected file/sink path through a test-only constructor or path injection and assert the line includes severity/subsystem/message.

Expected: PASS without touching the user's real LocalAppData log.

- [ ] **Step 6: Commit**

```bash
git add src/diagnostics tests CMakeLists.txt
git commit -m "feat: add diagnostics model and formatter"
```

---

### Task 3: OS, CPU, and Memory Native Probes

**Files:**
- Create: `src/platform/windows/os_probe.h`
- Create: `src/platform/windows/os_probe.cpp`
- Create: `src/platform/windows/cpu_memory_probe.h`
- Create: `src/platform/windows/cpu_memory_probe.cpp`
- Modify: `tests/platform_filter_tests.cpp`
- Modify: `tests/test_main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ProbeResult` and snapshot section types from Task 2.
- Produces:
  - `OsInfo ProbeOperatingSystem();`
  - `CpuMemoryInfo ProbeCpuAndMemory();`

- [ ] **Step 1: Write failing normalization tests**

Tests assert:
- a synthetic valid OS version maps to `Available`
- a failed version API maps to `Error` with code preserved
- CPU model whitespace is trimmed but internal spaces preserved
- zero/failed physical-memory query maps to `Error`, not fabricated memory

Expected: FAIL before probe helpers exist.

- [ ] **Step 2: Implement OS probe**

Use real-version native behavior such as dynamically resolved `RtlGetVersion`; do not rely on compatibility-manifest-sensitive version helpers.

Return actual major/minor/build and architecture.

- [ ] **Step 3: Implement CPU/memory probe**

Collect:
- processor architecture
- logical processor count
- CPU model string from native Windows/registry state
- total physical memory

No WMI dependency.

- [ ] **Step 4: Add integration assertions for the reference machine**

Integration test requirements:
- build number > 0
- logical processor count > 0
- memory > 0
- CPU model non-empty

Do not hardcode `Core Ultra 5 245K` in product tests.

- [ ] **Step 5: Run CTest**

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add src/platform/windows tests CMakeLists.txt
git commit -m "feat: add system diagnostics probes"
```

---

### Task 4: DXGI Adapter, Monitor, and Visible-Window Enumeration

**Files:**
- Create: `src/platform/windows/dxgi_adapter_probe.h`
- Create: `src/platform/windows/dxgi_adapter_probe.cpp`
- Create: `src/platform/windows/monitor_probe.h`
- Create: `src/platform/windows/monitor_probe.cpp`
- Create: `src/platform/windows/window_probe.h`
- Create: `src/platform/windows/window_probe.cpp`
- Modify: `tests/platform_filter_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 diagnostics value types.
- Produces:
  - `std::vector<GraphicsAdapterInfo> ProbeDxgiAdapters();`
  - `std::vector<MonitorInfo> ProbeMonitors();`
  - `std::vector<WindowInfo> ProbeVisibleTopLevelWindows(HWND self_root);`
  - `bool ShouldIncludeWindow(const WindowCandidate& candidate, HWND self_root);`

- [ ] **Step 1: Write failing window-filter tests**

Synthetic cases:
- invisible window → excluded
- empty title → excluded
- own FriendPushScreen root/child → excluded
- visible titled top-level app → included
- tool/owned window that should not be a share candidate → excluded when the chosen policy marks it non-application

Also assert default diagnostics export does not contain the synthetic sensitive title `"Secret Project - Payroll"`.

Expected: FAIL.

- [ ] **Step 2: Implement DXGI adapter enumeration**

Record at minimum:
- description
- vendor ID
- device ID
- dedicated video memory
- shared system memory
- software-adapter flag

Do not infer codec support from vendor.

- [ ] **Step 3: Implement monitor enumeration**

Record:
- display/device name
- desktop rectangle
- primary flag
- current dimensions when available

- [ ] **Step 4: Implement window enumeration/filter**

Keep final FPS-005 shareability rules out of this task; this is diagnostic enumeration only.

- [ ] **Step 5: Add physical integration checks**

Reference-machine expectations:
- at least one DXGI adapter
- at least one monitor
- enumeration calls complete without crash even if no shareable windows pass the filter

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/platform/windows tests CMakeLists.txt
git commit -m "feat: enumerate graphics displays and windows"
```

---

### Task 5: Media Foundation Capture-Device and Codec Capability Probes

**Files:**
- Create: `src/media/capture_device_probe.h`
- Create: `src/media/capture_device_probe.cpp`
- Create: `src/media/codec_capability_probe.h`
- Create: `src/media/codec_capability_probe.cpp`
- Modify: `tests/platform_filter_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 diagnostics types; COM/Media Foundation startup owned by the caller/application lifecycle.
- Produces:
  - `std::vector<CaptureDeviceInfo> ProbeVideoCaptureDevices();`
  - `std::vector<CodecCapabilityInfo> ProbeCodecCapabilities();`
  - codec records include `CodecKind { H264, HEVC, AV1 }`, direction `Encode|Decode`, and hardware classification `Hardware|Software|Unknown`.

- [ ] **Step 1: Write failing capability-normalization tests**

Synthetic MFT records must prove:
- H.264 encode/decode are distinguished
- HEVC encode/decode are distinguished
- AV1 encode/decode are distinguished
- unknown hardware classification stays `Unknown`; never infer hardware from GPU vendor

Expected: FAIL.

- [ ] **Step 2: Implement Media Foundation video-device enumeration**

Use Media Foundation device-source enumeration.

Record friendly name and symbolic identity when exposed.

No hardcoded `USB3 Video`, Onten name, VID, or PID.

- [ ] **Step 3: Implement codec transform enumeration**

Enumerate H.264/HEVC/AV1 encoder and decoder capability records through Media Foundation transform APIs.

The probe reports what Windows exposes; it does not execute encode/decode.

- [ ] **Step 4: Test optional-hardware absence**

Introduce injectable/normalization-level tests where empty enumerations produce valid empty vectors and an `Unavailable`/informational probe result rather than throwing.

- [ ] **Step 5: Run reference-machine integration checks**

Expected when the reference capture device is connected:
- at least one device includes friendly name `USB3 Video`

If disconnected:
- enumeration still PASS; record environment-dependent absence

Codec enumeration must complete without crash.

- [ ] **Step 6: Commit**

```bash
git add src/media tests CMakeLists.txt
git commit -m "feat: probe media devices and codec capabilities"
```

---

### Task 6: Native Network and Tailscale Presence Probe

**Files:**
- Create: `src/platform/windows/network_probe.h`
- Create: `src/platform/windows/network_probe.cpp`
- Modify: `tests/platform_filter_tests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 diagnostics types.
- Produces:
  - `NetworkInfo ProbeNetworkState();`
  - records active adapters, operational status, IPv4 addresses, available link speed/characteristics, and `TailscalePresence { Present, Absent, Unknown }`.

- [ ] **Step 1: Write failing adapter-classification tests**

Synthetic adapter records:
- loopback → represented but not selected as primary physical network
- Ethernet up with IPv4 → active
- Tailscale-named/tunnel adapter with 100.x or fd7a address → Tailscale Present
- no Tailscale evidence → Absent, not Error
- native API failure → Unknown/Error with code

Expected: FAIL.

- [ ] **Step 2: Implement native network enumeration**

Use Windows IP Helper/network adapter APIs.

Do not invoke `tailscale.exe` in application startup.

- [ ] **Step 3: Add reference-machine integration check**

Expected:
- enumeration completes
- at least one active adapter exists
- when Tailscale is active on the machine, Tailscale presence is reported

Do not require a peer or Direct/Relay state.

- [ ] **Step 4: Run CTest**

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/platform/windows/network_probe.* tests CMakeLists.txt
git commit -m "feat: add native network diagnostics"
```

---

### Task 7: Win32 Application Shell and D3D11 Placeholder Renderer

**Files:**
- Create: `src/app/main.cpp`
- Create: `src/app/application.h`
- Create: `src/app/application.cpp`
- Create: `src/ui/main_window.h`
- Create: `src/ui/main_window.cpp`
- Create: `src/graphics/d3d11_device.h`
- Create: `src/graphics/d3d11_device.cpp`
- Create: `src/graphics/placeholder_renderer.h`
- Create: `src/graphics/placeholder_renderer.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform_filter_tests.cpp`

**Interfaces:**
- Consumes: Tasks 1–6 foundations; diagnostics collection may initially be synchronous only for a startup placeholder, with asynchronous orchestration added in Task 8.
- Produces:
  - `class Application { int Run(HINSTANCE instance, int show_command); };`
  - `class MainWindow { bool Create(HINSTANCE instance); HWND Handle() const; void SetDiagnosticsText(std::wstring_view text); void SetCollecting(bool collecting); };`
  - `class D3D11Device { bool Initialize(HWND hwnd); bool Resize(UINT width, UINT height); bool PresentPlaceholder(); bool IsWarpFallback() const; };`
  - renderer status that distinguishes hardware D3D11, WARP fallback, and error.

- [ ] **Step 1: Write failing pure renderer-state tests**

Test state mapping:
- hardware device success → `Hardware`
- hardware fail + WARP success → `WarpFallback`
- both fail → `Error`

Expected: FAIL before state helper exists.

- [ ] **Step 2: Implement minimal Win32 shell**

Requirements:
- Unicode `wWinMain`
- top-level HWND
- message loop
- clean close/quit
- no administrator requirement
- no automatic sharing behavior

- [ ] **Step 3: Implement D3D11 device/swap chain**

Attempt hardware D3D11 first.

If creation fails:
- attempt WARP for placeholder/UI validation
- record fallback explicitly
- never call WARP “hardware accelerated”

- [ ] **Step 4: Implement deterministic placeholder renderer**

The render region must visibly differ from ordinary Win32 background and survive resize.

No capture/video frame enters this path.

- [ ] **Step 5: Add manual/local smoke verification**

Launch Debug executable and verify:
- window visible
- placeholder visible
- resize repeatedly
- minimize/restore
- move between available monitor areas if practical
- close exits cleanly

Record actual D3D path (hardware/WARP).

- [ ] **Step 6: Exercise failure path without requiring GPU breakage**

Add a debug/test-only injectable device-creation policy used by tests to force:
- hardware failure + WARP success
- total failure

Assert status text is explicit.

- [ ] **Step 7: Commit**

```bash
git add src/app src/ui src/graphics tests CMakeLists.txt
git commit -m "feat: add Win32 D3D11 application shell"
```

---

### Task 8: Asynchronous Diagnostics Service, UI Presentation, and UTF-8 Export

**Files:**
- Create: `src/diagnostics/diagnostics_service.h`
- Create: `src/diagnostics/diagnostics_service.cpp`
- Create: `tests/diagnostics_service_tests.cpp`
- Modify: `src/app/application.cpp`
- Modify: `src/ui/main_window.h`
- Modify: `src/ui/main_window.cpp`
- Modify: `src/diagnostics/diagnostics_formatter.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/test_main.cpp`

**Interfaces:**
- Consumes: all probe functions from Tasks 3–6 and `DiagnosticsSnapshot` from Task 2.
- Produces:
  - `class DiagnosticsService { public: bool StartCollection(HWND notify_hwnd); bool IsCollecting() const; std::shared_ptr<const DiagnosticsSnapshot> Latest() const; };`
  - one application-defined window message `WM_FPS_DIAGNOSTICS_READY`
  - `bool SaveDiagnosticsUtf8(const DiagnosticsSnapshot& snapshot, const std::filesystem::path& path);`

- [ ] **Step 1: Write failing duplicate-refresh test**

Using injectable fake probe bundle:
- first `StartCollection` returns true
- second call while active returns false
- only one collection invocation occurs
- existing `Latest()` remains readable during collection

Expected: FAIL.

- [ ] **Step 2: Write failing partial-probe-failure test**

A fake capture-device probe returns an error while OS/GPU/network probes succeed.

Assert:
- snapshot still produced
- failed subsystem appears as `Error`
- successful subsystem data remains present

Expected: FAIL.

- [ ] **Step 3: Implement DiagnosticsService**

Requirements:
- one worker job at a time
- immutable completed snapshot
- post completion back to UI thread
- no HWND mutation on worker thread
- COM initialized correctly for the worker when Media Foundation/COM APIs require it
- no permanent thread pool

- [ ] **Step 4: Wire diagnostics panel**

UI must show at least:
- Windows build
- CPU/memory
- graphics adapters
- monitor count/details
- window-enumeration status
- capture devices
- H.264/HEVC/AV1 encode/decode capability status
- network adapters
- Tailscale presence
- probe warnings/errors
- D3D hardware/WARP renderer status

Buttons:
- `Refresh`
- `Save Diagnostics`

Refresh disabled or ignored while collection is active.

- [ ] **Step 5: Write failing export-privacy test**

Synthetic snapshot contains:
- full sensitive visible-window title
- network data
- fake token-looking string only inside a window title

Call:
```cpp
SaveDiagnosticsUtf8(snapshot, path);
```

Assert:
- file is non-empty UTF-8
- section headings present
- visible-window count/status present
- full title absent
- fake token-looking window-title content absent

Expected: FAIL before export behavior is implemented.

- [ ] **Step 6: Implement UTF-8 export**

Use the formatter with `include_window_titles=false`.

The Save dialog/path selection stays on the UI thread.

- [ ] **Step 7: Physical application verification**

On the reference machine verify UI reports:
- Windows build 19045 or actual current live build
- non-empty CPU/memory
- Intel Graphics adapter
- at least one monitor
- `USB3 Video` when capture device is connected
- codec capability records
- active network adapter(s)
- Tailscale presence when active

Save a diagnostics report and inspect it for privacy constraints.

- [ ] **Step 8: Run full tests**

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: PASS.

- [ ] **Step 9: Commit**

```bash
git add src tests CMakeLists.txt
git commit -m "feat: add asynchronous hardware diagnostics UI"
```

---

### Task 9: Windows CI, Build Reproducibility, and FPS-003 Qualification

**Files:**
- Create: `.github/workflows/windows-build.yml`
- Create: `docs/development/reports/FPS-20261014-003-windows-app-skeleton-hardware-diagnostics.md`
- Modify: `README.md`
- Modify: `docs/README.md`
- Modify: `docs/development/QUALIFICATION-MATRIX.md`
- Modify: `docs/development/tasks/CURRENT.md`
- Modify: `docs/development/tasks/ROADMAP.md`

**Interfaces:**
- Consumes: completed application/tests from Tasks 1–8.
- Produces: reproducible Windows x64 CI evidence, physical-machine runtime evidence, exact qualified commit, and FPS-003 closure state.

- [ ] **Step 1: Add Windows CI workflow**

Workflow requirements:
- Windows runner
- checkout
- CMake configure x64
- build Debug or Release
- CTest with `--output-on-failure`
- no expectation of physical capture hardware/Tailscale/GPU feature availability

- [ ] **Step 2: Verify CI locally equivalent**

Run documented clean configure/build/test commands after deleting only the generated build directory.

Do not reset/clean the Git worktree.

Expected: PASS from a fresh build directory.

- [ ] **Step 3: Push exact implementation commit and verify CI**

Record:
- exact branch head SHA
- GitHub CI run/conclusion
- no uncommitted files

CI must be GREEN before FPS-003 closure.

- [ ] **Step 4: Perform physical runtime qualification**

Record in the evidence report:
- OS/build
- executable launch result
- D3D path
- resize/minimize/restore behavior
- diagnostics values observed
- capture-device enumeration result
- codec enumeration result
- network/Tailscale result
- Save Diagnostics result
- application close result
- exact commit

- [ ] **Step 5: Verify FPS-003 scope**

Search source for prohibited implementation drift:
- no screen/window frame capture implementation
- no video transport sockets/protocol
- no audio streaming
- no remote input
- no Qt linkage
- no FFmpeg runtime linkage

Expected: PASS.

- [ ] **Step 6: Update project status**

Set:
- FPS-003 → `GREEN / CLOSED`
- qualification matrix → GREEN
- roadmap → GREEN
- next task → `FPS-004 — Viewer UI & Settings Shell`
- no active task after closure

- [ ] **Step 7: Final diff/working-tree verification**

Run:
```powershell
git diff --check
git status --short --branch
git rev-parse HEAD
git rev-parse origin/fps-003-native-windows-app-design
```

Expected:
- diff check clean
- working tree clean after closure commit
- local branch head equals remote branch head

- [ ] **Step 8: Commit qualification evidence**

```bash
git add README.md docs .github/workflows/windows-build.yml
git commit -m "docs: qualify FPS-003 Windows application baseline"
```

- [ ] **Step 9: Create PR with exact-head protection, merge only after all gates are GREEN, then sync local `main`**

Expected final state:
- PR merged
- Issue #5 closed completed
- local `main` == `origin/main`
- clean worktree
- FPS-003 GREEN

---

## Plan Self-Review Results

### Spec coverage

Covered explicitly:
- native C++20/Win32 baseline
- D3D11/DXGI renderer
- Windows SDK prerequisite
- diagnostics model and formatter
- OS/CPU/memory
- DXGI adapters
- monitors
- visible windows
- Media Foundation capture devices
- H.264/HEVC/AV1 capability enumeration
- native network/Tailscale presence
- asynchronous diagnostics collection
- D3D hardware/WARP distinction
- diagnostics UI
- UTF-8 export and privacy boundary
- minimal logging
- CTest
- Windows CI
- physical qualification
- FPS-003 scope exclusions

No spec requirement is intentionally left without an owning task.

### Type/interface consistency

Later tasks consume only interfaces declared in earlier tasks. UI receives snapshot/state and does not call platform/media enumeration directly.

### Scope check

The plan remains one coherent subsystem: first Windows executable + diagnostic foundation. FPS-004 UI polish, FPS-005 capture, FPS-006 codec execution, and FPS-009/FPS-010 networking remain deferred.

### Review Focus coverage

All five review-focus risks have an explicit test or physical qualification step in the task that owns the relevant behavior.
