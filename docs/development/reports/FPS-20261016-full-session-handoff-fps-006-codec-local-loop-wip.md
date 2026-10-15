# FriendPushScreen — Full Session Handoff — FPS-006 Codec Local Loop WIP

**Date:** 2026-10-16  
**Repository:** `funggier/FriendPushScreen`  
**Canonical local repo:** `T:\Space\Projects\ProjectsProgram\FriendPushScreen`  
**Canonical continuation worktree:** `T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-encode-decode-local-loopback`  
**Active task:** FPS-006 — Encode / Decode / Local Loopback  
**GitHub issue:** #14  
**Canonical branch:** `fps-006-encode-decode-local-loopback`  
**Base main:** `c9c0af087fe039ae1a8c8f695411560b147f3ef4` — Merge FPS-005 Unified Capture Sources  
**Canonical branch HEAD before this handoff commit:** `eff4a2d5ca13d8ca33672b1d1fe7f2e544f619bb`  
**Status at handoff:** FPS-006 ACTIVE / implementation in progress / no PR yet

---

## 1. Authoritative-source rule

For the next session, treat these as authoritative in this order:

1. live Git state in the canonical FPS-006 worktree
2. live GitHub issue / branch / CI state
3. runtime evidence from the reference Windows machine
4. current FPS-006 plan and task documents
5. this handoff

Do not infer completion from an old report or old worktree.

Do not reset, clean, delete, or overwrite any worktree described below.

---

## 2. Main branch state

At handoff creation:

- `main` is clean
- `main == origin/main`
- main HEAD:
  `c9c0af087fe039ae1a8c8f695411560b147f3ef4`
- commit:
  `Merge FPS-005 Unified Capture Sources`

FPS-005 is GREEN / CLOSED and merged.

The root `main` task document still correctly describes FPS-005 as the most recently closed task because FPS-006 activation exists only on the FPS-006 feature branch.

Do not restart FPS-005.

---

## 3. Product/UI baseline inherited from FPS-004/FPS-005

These behaviors are already accepted product behavior and FPS-006 must preserve them.

### Viewer geometry

Windowed mode:

- the top status chrome owns/reserves its own strip
- the viewer starts below the status strip
- the entire incoming/shared frame remains visible
- status chrome must not cover the top of the video
- the video uses aspect-fit / no crop-to-fill

Settings:

- Settings is an overlay above the Viewer
- opening Settings must not resize or squeeze the Viewer surface
- hiding Settings reveals the same full-size Viewer behind it

F11:

- status chrome and Settings button are hidden
- Viewer expands to the full client area
- Esc restores prior windowed placement

### Current UI/branding decisions

- FriendPushScreen owns the dark palette
- custom FriendPushScreen dropdowns
- custom Settings scrollbar
- custom receiver volume slider
- `Hide Settings (Tab) / Settings (Tab)` shown on the top status row
- status text has a small left inset before `Peer:`
- canonical app icon is an oak/bronze/amber monitor on a transparent background
- icon is embedded into the EXE/window class and is intended to be reused by FPS-015 installer/shortcuts

### Connection mode

There is no user-selectable Auto mode.

Current modes are exactly:

- LAN
- Direct IP / DDNS
- Tailscale

Legacy `connection_mode=auto` is read only for migration and normalizes to `direct_ip_ddns`.

### Ports

- Share Port and Remote Port are separate fields
- Share Port belongs to the sender/listener side
- Remote Port belongs to the connection target side

### Audio scope

FPS-004 only added audio UI/state shell.

Real audio capture/codec/playback is **not** FPS-006 video scope and remains FPS-006A.

---

## 4. FPS-005 baseline inherited by FPS-006

FPS-005 qualified and merged these native capture paths:

- Monitor → DXGI Desktop Duplication (DDA)
- Window → Windows Graphics Capture (WGC)
- VideoDevice → Media Foundation

The unified source model already exists.

The capture frame contract includes:

- source sequence
- 100 ns timestamp
- width / height
- BGRA pixel format
- D3D11 texture when available
- CPU BGRA fallback where applicable
- source identity/state
- sender cursor policy

Additional accepted FPS-005 behavior:

- persisted `Show Cursor` sender option, default ON
- DDA/WGC cursor policy responds live
- Show Cursor is disabled/not-applicable for VideoDevice sources
- no automatic source substitution after source loss

FPS-006 must consume the FPS-005 capture contract rather than redesigning it.

---

## 5. FPS-006 goal

Insert a production-shaped video codec layer between FPS-005 capture and the existing PreviewRenderer:

```text
ICaptureSession
      │
      ▼
CapturedFrame (BGRA8 GPU/CPU)
      │
      ▼
VideoFrameNormalizer / FrameClock
      │
      ▼
D3D11 BGRA → NV12
      │
      ▼
IVideoEncoder
      │
      ▼
EncodedAccessUnit
      │
      ▼
BoundedEncodedQueue
      │
      ▼
IVideoDecoder
      │
      ▼
Decoded NV12
      │
      ▼
D3D11 NV12 → BGRA
      │
      ▼
existing PreviewRenderer
```

FPS-006 remains local-only.

Do not add:

- network media transport
- peer handshake
- control protocol
- audio capture/codec/playback
- feedback protection
- network bitrate adaptation
- two-peer codec negotiation

---

## 6. Canonical FPS-006 continuation branch

Use this branch/worktree for new implementation:

```text
Branch:
fps-006-encode-decode-local-loopback

Worktree:
T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-encode-decode-local-loopback
```

Current task file:

`docs/development/tasks/CURRENT.md`

Current plan:

`docs/superpowers/plans/2026-10-16-fps-006-encode-decode-local-loopback.md`

GitHub issue:

`#14 — FPS-006 — Encode / Decode / Local Loopback`

Before this handoff commit:

- HEAD: `eff4a2d5ca13d8ca33672b1d1fe7f2e544f619bb`
- remote branch HEAD: `47f17881d689ef2d9c76bd145f02e5fd159485e6`
- branch was ahead remote by 1 commit
- worktree was clean

The handoff commit will advance this branch again. Re-read live HEAD after checkout.

---

## 7. Implemented on the canonical FPS-006 branch

The following FPS-006 foundation is already committed on the canonical branch.

### Codec/pipeline models

Files include:

- `src/media/video_codec_model.*`
- `src/media/video_frame_clock.*`
- `src/media/encoded_access_unit_queue.*`
- `src/media/video_loop_metrics.*`

Implemented model-level concerns include:

- codec kinds
- backend/acceleration classification
- codec configuration/state
- monotonic codec frame clock
- bounded encoded-access-unit queue
- local-loop metric accounting

### Media Foundation codec discovery

Files include:

- `src/media/mft_video_codec_catalog.*`

Runtime enumeration is based on Media Foundation transform capability, not CPU/GPU brand strings.

### Media Foundation D3D11 support layer

Files include:

- `src/media/mf_d3d11_device_manager.*`
- `src/media/mft_video_media_type.*`

The canonical branch has D3D11 device-manager/media-type groundwork for codec MFT configuration.

### Runtime transform activation probe

Latest canonical commit before handoff:

`eff4a2d — feat: add FPS-006 runtime MFT activation probe`

Files added/changed include:

- `src/media/mft_video_transform_activation.*`
- `tools/codec_probe/main.cpp`
- CMake wiring

This is currently the newest committed FPS-006 execution-layer step.

---

## 8. Tests already present

Canonical branch includes FPS-006-related tests such as:

- `tests/video_pipeline_model_tests.cpp`
- `tests/video_loop_metrics_tests.cpp`
- `tests/mft_video_media_type_tests.cpp`

At handoff creation, Debug CTest was run against canonical HEAD:

```text
FriendPushScreenTests: PASS
1/1 tests passed
CTEST_EXIT=0
```

Release tests were **not built at the current canonical HEAD** at handoff time.

Do not claim Release qualification yet.

---

## 9. Runtime codec probe evidence at canonical HEAD

Command executed:

```text
build\Debug\FriendPushScreenCodecProbe.exe
```

Result:

```text
H.264 encode: 3 candidate(s)
  [0] Intel® Quick Sync Video H.264 Encoder MFT
      hardware | d3d11=yes | async=yes | activate=PASS
  [1] H264 Encoder MFT
      software | d3d11=no | async=no | activate=PASS
  [2] Intel® Quick Sync Video H.264 Encoder MFT
      hardware | d3d11=yes | async=yes | activate=PASS

H.264 decode: 1 candidate(s)
  [0] Microsoft H264 Video Decoder MFT
      software | d3d11=yes | async=no | activate=PASS

HEVC encode: 2 candidate(s)
  [0] Intel® Hardware H265 Encoder MFT
      hardware | d3d11=yes | async=yes | activate=PASS
  [1] Intel® Hardware H265 Encoder MFT
      hardware | d3d11=yes | async=yes | activate=PASS

HEVC decode: 0 candidate(s)

AV1 encode: 0 candidate(s)
AV1 decode: 0 candidate(s)

PROBE_EXIT=0
```

Important interpretation:

- H.264 hardware encoder activation is proven through native MF.
- H.264 software encoder activation is proven.
- Native MF enumeration currently exposes only the Microsoft H.264 decoder MFT, classified software, though it is D3D11-aware.
- HEVC hardware encoder activation is visible.
- HEVC decoder is not visible through the current MF enumeration path.
- AV1 encoder/decoder is not visible through the current MF enumeration path.
- duplicate Intel encoder entries exist and may need catalog deduplication/identity analysis.

Do not mislabel the Microsoft H.264 decoder as hardware merely because it is D3D11-aware.

Earlier FPS-002 FFmpeg/QSV evidence showed broader Intel hardware codec capability, but FPS-006 must qualify the actual native API/backend it ships, not infer success from FFmpeg.

---

## 10. Critical unresolved FPS-006 questions

### A. Actual frame execution is not done yet

MFT enumeration/activation is not the same as successful codec execution.

Still required:

- real BGRA → NV12 frame preparation
- submit real/synthetic NV12 sample to selected encoder
- drain encoded H.264 access units
- submit encoded H.264 to decoder
- obtain decoded frame/surface
- convert/hand off decoded frame to PreviewRenderer

### B. Async hardware MFT handling

The Intel H.264 encoder reports `async=yes`.

Do not implement it as though every transform were synchronous.

Correctly handle Media Foundation async events/messages or create a wrapper that supports both sync and async transforms.

### C. Hardware decoder requirement may need evidence-driven interpretation

Issue #14/plan expects hardware H.264 encode/decode where available.

Current MF catalog exposes:

- Intel hardware H.264 encoder
- Microsoft H.264 decoder, D3D11-aware, classified software

Need to determine whether:

1. the Microsoft decoder internally uses DXVA/D3D11 acceleration while still being the Microsoft MFT,
2. a hardware decoder MFT requires different enumeration/config flags,
3. a different native Windows decode path is needed,
4. the qualification wording should be revised to “hardware-capable decode when available” if Windows MF does not expose a separately classified hardware decoder on this machine.

Do not fake a hardware decoder classification to satisfy the plan.

### D. Pixel conversion is still the next major implementation layer

Canonical branch does not yet contain the finished D3D11 BGRA↔NV12 execution path.

That work must precede real encoder execution.

---

## 11. Other FPS-006 worktrees — preserve, audit, do not clean/reset

There are two older experimental FPS-006 worktrees with useful but unmerged WIP.

They are **not** the canonical continuation branch, but may contain reusable implementation.

### 11.1 `fps-006-codec-local-loopback`

Path:

`T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-codec-local-loopback`

HEAD at handoff:

`808183fa052cfc076a1dc834b1d7efff26acb9bc`

Important uncommitted WIP:

- modified `CMakeLists.txt`
- modified `src/media/mf_h264_codec.cpp`
- modified `src/media/mf_h264_codec.h`
- modified `tests/test_main.cpp`
- modified `tools/codec_probe/main.cpp`
- untracked `src/media/d3d11_video_converter.cpp`
- untracked `src/media/d3d11_video_converter.h`
- untracked `src/media/video_frame_converter.cpp`
- untracked `src/media/video_frame_converter.h`
- untracked `tests/video_frame_converter_tests.cpp`

This WIP was last actively edited around 00:14 on 2026-10-16.

It may contain useful conversion and H.264 execution work.

**Do not reset/clean/delete this worktree.**

Audit and selectively port useful code into the canonical branch after comparing contracts.

### 11.2 `fps-006-video-codec-local-loopback`

Path:

`T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-video-codec-local-loopback`

HEAD at handoff:

`1bdce13457d0405a4a226be320c014ab05e5c5d3`

Important uncommitted WIP:

- modified `CMakeLists.txt`
- untracked `src/media/mf_video_encoder.cpp`
- untracked `src/media/mf_video_encoder.h`
- untracked `src/media/mf_video_transform_activation.cpp`
- untracked `src/media/mf_video_transform_activation.h`

The committed history on this experimental line also includes work such as:

- runtime Media Foundation codec candidates
- queue/metrics foundation
- D3D11 BGRA → NV12 conversion

**Do not reset/clean/delete this worktree.**

Audit before writing equivalent encoder/conversion code from scratch.

### 11.3 Why multiple worktrees exist

FPS-006 went through several implementation attempts while probing the native Windows codec path.

The canonical continuation line is now:

`fps-006-encode-decode-local-loopback`

because it has the newest plan, cleanest worktree, model/tests foundation, MF catalog, D3D11 media-type layer, and runtime activation probe.

The older worktrees are retained as implementation evidence/WIP sources, not as authoritative task state.

---

## 12. Recommended next implementation order

Use this order in the next session.

### Step 1 — Re-verify live state

Read first:

1. this handoff
2. `docs/development/tasks/CURRENT.md`
3. `docs/superpowers/plans/2026-10-16-fps-006-encode-decode-local-loopback.md`
4. Git/GitHub/runtime state

Verify:

- branch is `fps-006-encode-decode-local-loopback`
- worktree is clean before new edits
- Issue #14 remains open
- no newer PR/branch has superseded this handoff

### Step 2 — Audit the two experimental worktrees

Before implementing conversion/encoder wrappers from scratch:

- diff their D3D11 converter code against canonical contracts
- diff `mf_h264_codec.*` and `mf_video_encoder.*`
- identify reusable tested pieces
- port only compatible pieces
- do not cherry-pick blindly because the branches evolved with different abstractions

Document what was reused/rejected.

### Step 3 — Finish D3D11 frame conversion

Required path:

- BGRA D3D11 texture → NV12 D3D11 texture
- reusable textures/resources
- CPU BGRA upload path only as explicit fallback
- later NV12 → BGRA for PreviewRenderer compatibility

Add focused tests where unit-testable and a runtime conversion probe for real D3D11 surfaces.

### Step 4 — Implement real H.264 encoder execution wrapper

Use the runtime-selected Intel hardware H.264 MFT first.

Must support the async transform correctly.

Prove:

- media-type negotiation
- D3D11 manager attachment
- sample submission
- encoded access-unit output
- timestamps/duration
- keyframe behavior
- error reporting

### Step 5 — Implement H.264 decoder execution

Start with the Microsoft H.264 decoder MFT exposed by native MF.

Determine actual D3D11/DXVA behavior with evidence.

Do not assume “software classification” means CPU-only execution and do not assume D3D11 awareness means hardware acceleration.

### Step 6 — Synthetic local encode/decode smoke

Before wiring full capture:

- deterministic synthetic BGRA frame
- BGRA → NV12
- H.264 encode
- bounded queue
- H.264 decode
- NV12 → BGRA
- frame metadata/dimensions/timestamps verified

### Step 7 — Real FPS-005 capture → decoded PreviewRenderer

Wire:

`ICaptureSession → codec local loop → decoded frame → existing PreviewRenderer`

UI/status must make it unambiguous when the Viewer is showing:

- raw FPS-005 capture, versus
- FPS-006 decoded local-loop output

Never silently fall back to raw preview while claiming codec loop success.

### Step 8 — Forced H.264 software fallback

Explicitly disable hardware encoder preference and execute the software `H264 Encoder MFT`.

Measure functionality/performance separately.

### Step 9 — Sustained 1080p60 qualification

Use a real qualified FPS-005 1080p60 source.

Record:

- exact encoder/decoder names
- hardware/software classification
- resolution/fps
- bitrate
- duration
- frames in/encoded/decoded/rendered
- queue max depth
- drops/recovery resets
- average/p95/max latency where meaningful
- CPU usage
- GPU video-engine activity
- memory stability

Do not close FPS-006 on a short startup smoke.

### Step 10 — HEVC / AV1 evidence

Record native MF availability honestly.

Current probe does not expose a complete HEVC pair or AV1 pair.

These do not block H.264 V1 qualification unless task scope is explicitly changed.

### Step 11 — DG-004

Resolve only from actual runtime execution evidence.

Architecture must remain vendor-neutral.

Do not claim AMD/NVIDIA physical qualification without those machines.

### Step 12 — Closure gates

FPS-006 may become GREEN only after:

- codec model tests GREEN
- frame clock tests GREEN
- bounded queue tests GREEN
- real H.264 local encode/decode/render GREEN
- forced software H.264 fallback GREEN
- sustained 1080p60 hardware-path evidence
- latency/resource evidence
- FPS-005 viewer/layout regression checks
- Debug local build + CTest
- Release local build + CTest
- exact-commit Windows CI
- dated qualification report
- DG-004 update
- PR/merge only after qualification

---

## 13. Git/GitHub state at handoff

### Main

```text
main = origin/main
c9c0af087fe039ae1a8c8f695411560b147f3ef4
Merge FPS-005 Unified Capture Sources
```

### FPS-006 issue

`#14 — FPS-006 — Encode / Decode / Local Loopback`

State: OPEN

No FPS-006 PR was open when this handoff was prepared.

### Canonical branch

Before this handoff commit:

```text
fps-006-encode-decode-local-loopback
HEAD   = eff4a2d5ca13d8ca33672b1d1fe7f2e544f619bb
remote = 47f17881d689ef2d9c76bd145f02e5fd159485e6
```

The branch was ahead remote by one commit.

After the handoff is committed/pushed, live Git state supersedes these exact branch SHAs.

---

## 14. Commands/checks useful at session start

```powershell
$wt = 'T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-encode-decode-local-loopback'

git -C $wt status --short --branch
git -C $wt fetch origin
git -C $wt rev-parse HEAD
git -C $wt rev-parse origin/fps-006-encode-decode-local-loopback

Get-Content "$wt\docs\development\tasks\CURRENT.md"
Get-Content "$wt\docs\superpowers\plans\2026-10-16-fps-006-encode-decode-local-loopback.md"

& "$wt\build\Debug\FriendPushScreenCodecProbe.exe"

ctest --test-dir "$wt\build" -C Debug --output-on-failure
```

Also inspect before editing:

```text
T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-codec-local-loopback
T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-video-codec-local-loopback
```

Do not clean them.

---

## 15. Ready-to-use prompt for the next ChatGPT session

```text
@use-local-project

ทำ funggier/FriendPushScreen ต่อจาก Full Handoff ล่าสุด โดยอ่านไฟล์นี้ก่อนเป็นอันดับแรก:

docs/development/reports/FPS-20261016-full-session-handoff-fps-006-codec-local-loop-wip.md

ให้ยึด Git / GitHub / runtime สดเป็น authoritative source ก่อนเอกสารเก่า

สถานะที่คาดไว้:
- main ปิด FPS-005 GREEN แล้ว
- FPS-006 Issue #14 ACTIVE
- canonical worktree คือ:
  T:\Space\Projects\ProjectsProgram\FriendPushScreen\.worktrees\fps-006-encode-decode-local-loopback
- canonical branch:
  fps-006-encode-decode-local-loopback

ข้อห้าม:
- ห้าม reset/clean worktree
- ห้ามลบ WIP ใน:
  fps-006-codec-local-loopback
  fps-006-video-codec-local-loopback
- ห้ามย้อนกลับไปทำ FPS-005
- ห้าม claim codec hardware path จากชื่อ CPU/GPU
- ห้าม claim encode/decode success แค่เพราะ MFT enumerate/activate ผ่าน
- ห้ามเพิ่ม network/audio/control protocol ใน FPS-006

ก่อนเขียน conversion/encoder code ซ้ำ ให้ audit WIP ในสอง experimental worktree ก่อน เพราะมี d3d11_video_converter / video_frame_converter / mf_h264_codec / mf_video_encoder ที่อาจนำกลับมาใช้ได้

จากนั้นทำ FPS-006 ต่อจาก runtime MFT activation probe โดยเป้าหมายถัดไปคือ:
1. BGRA↔NV12 D3D11 conversion
2. real H.264 encoder sample execution
3. real H.264 decoder execution
4. synthetic encode→decode smoke
5. FPS-005 capture→encode→bounded queue→decode→PreviewRenderer
6. forced software H.264 fallback
7. sustained 1080p60 qualification + metrics/resource evidence
8. DG-004
9. Debug/Release + exact-commit CI + report ก่อน GREEN

รักษา UI baseline เดิม:
- windowed status strip ต้องไม่บังภาพ
- Settings เป็น overlay และไม่ย่อ Viewer
- F11 Viewer เต็ม client
- aspect-fit เห็นภาพครบ ไม่ crop
```

---

## 16. Stop-point summary

The project is not blocked.

FPS-005 is complete.

FPS-006 has a solid model/catalog/media-type foundation and real native MFT activation evidence.

The next real engineering boundary is no longer “can Windows enumerate a codec?” but:

**Can FriendPushScreen execute real D3D11 frame conversion + H.264 encode/decode correctly, asynchronously where required, and sustain the local loop with bounded latency/queues?**

That is the correct continuation point for the next session.
