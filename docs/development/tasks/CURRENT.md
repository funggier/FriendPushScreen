# Current Development Task

## FPS-003 — Windows App Skeleton & Hardware Diagnostics

**Status:** IMPLEMENTATION PLAN REVIEW PENDING

**Branch:** `fps-003-native-windows-app-design`

**Started:** 2026-10-14

**GitHub Issue:** #5 — FPS-003 — Windows App Skeleton & Hardware Diagnostics

### Goal

Create the first real FriendPushScreen Windows executable and native diagnostics foundation.

### Approved architectural direction

- C++20
- Win32 application shell
- D3D11 / DXGI rendering foundation
- CMake
- MSVC
- Media Foundation for native media capability/device enumeration
- no Qt dependency in the V1 baseline
- no FFmpeg runtime dependency

### Written design

`docs/superpowers/specs/2026-10-14-fps-003-native-windows-app-design.md`

### Approved design

`docs/superpowers/specs/2026-10-14-fps-003-native-windows-app-design.md`

User approved the written FPS-003 design on 2026-10-14.

### Implementation plan

`docs/superpowers/plans/2026-10-14-fps-003-native-windows-app-implementation.md`

### Current gate

The implementation plan exists and has passed self-review. Product code, dependency installation/repair, and build changes must wait until the user confirms the plan and selects the execution method.

### Important implementation prerequisite

The reference machine has Visual Studio 2026, MSVC and CMake, but the standard Windows SDK Include/Lib directories are currently missing. A complete Windows SDK must be installed or repaired before product code compilation begins.

### Scope boundary

FPS-003 does not implement real capture, video encode/decode, network video transport, audio streaming, or remote control.
