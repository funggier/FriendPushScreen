# Current Development Task

## FPS-006 — Encode / Decode / Local Loopback

**Status:** GREEN (qualified on canonical feature branch)

**Branch:** `fps-006-encode-decode-local-loopback`

**Started:** 2026-10-16

**GitHub Issue:** #14 — FPS-006 — Encode / Decode / Local Loopback

### Base

`c9c0af087fe039ae1a8c8f695411560b147f3ef4` — FPS-005 GREEN merge.

### Goal

Insert the vendor-neutral V1 video codec layer between the qualified FPS-005 capture pipeline and the existing D3D11 viewer, then prove a sustained local capture → encode → bounded queue → decode → render loop.

### Required outcomes

- vendor-neutral encoder / decoder abstraction
- Media Foundation MFT runtime selection from capability, not CPU/GPU vendor assumptions
- H.264 required compatibility baseline
- real hardware H.264 encode/decode path
- explicitly forced software H.264 fallback path
- D3D11 BGRA → NV12 conversion for encoder input
- decoded NV12 → BGRA handoff to the existing PreviewRenderer
- FriendPushScreen-owned monotonic codec sequence/timestamps
- bounded encoded-access-unit queue
- stale/dependency recovery policy using keyframe/IDR recovery rather than unbounded growth
- codec-loop state and errors independent from future network state
- internal encode/decode/end-to-end latency + queue/drop metrics
- sustained 1920x1080 @ 60 fps hardware local-loop evidence
- HEVC / AV1 capability evidence where available
- preserve FPS-005 full-frame aspect-fit viewer
- preserve windowed status-strip geometry and Settings overlay behavior
- resolve DG-004 from runtime/physical evidence

### Safety / scope boundary

FPS-006 must not implement:

- network media transport
- secure peer/session handshake
- control protocol
- audio capture/codec/playback
- feedback protection
- network-driven bitrate adaptation
- two-peer codec negotiation

Those remain FPS-006A / FPS-007 / FPS-008 / FPS-009+.

### Plan

`docs/superpowers/plans/2026-10-16-fps-006-encode-decode-local-loopback.md`

### Qualification

FPS-006 becomes GREEN only after:

- codec-selection and state unit tests
- frame-clock monotonicity tests
- bounded queue / overflow recovery tests
- real H.264 hardware encode/decode activation
- real local capture → encode → decode → render
- forced software H.264 fallback evidence
- sustained 1080p60 hardware loop with bounded queue
- latency / FPS / drop / queue-depth metrics
- CPU/GPU/process-resource observations
- FPS-005 viewer layout/aspect-fit regression checks
- Debug + Release local build/test
- exact-commit Windows CI
- dated qualification evidence report
- DG-004 decision update

### Next after FPS-006

`FPS-006A — Audio Capture / Encode / Local Loopback`

### FPS-006 qualification closure — 2026-10-16

Runtime source: 1a1bbd194d08459e2479942a4b5df43e83628217. Real WGC 1080p60 hardware local loop: 180.020 s, 10762 frames through all drained stages, 59.7824 fps, queue max 1, zero queue drops. Real application after Stop/Start: 202.157 s at 59.5725 fps. Forced software fallback, source-loss/selection and viewer regressions passed. DG-004 resolved. Full Debug/Release local builds+CTest and exact-source Windows CI passed; final evidence HEAD is checked before announcing closure.

[Qualification report](../reports/FPS-20261016-006-wip-audit-and-execution.md). Canonical branch/worktree and both old WIP worktrees remain preserved. Integration remains separate from this feature-branch qualification; FPS-006A remains planned.


### Independent codec controls follow-up — 2026-10-16

**Status: GREEN on canonical feature branch.** Implementation source: d116ab13ec9422174aa9ed7d761672d14f5b42a8.

Settings → VIDEO / H.264 now selects CPU/GPU encoder and decoder independently by runtime registration, persists bitrate/keyframe/encoding-latency options, and reports actual paths and explicit codec errors. Apply preserves the capture source. Decoder retains qualified real-time low-latency behavior; CPU codec mode still uses D3D11 conversion/rendering.

Full Debug/Release builds + CTest, eight real-bitstream mode/latency runs, actual four-pair UI switching, invalid-input/draft/restart behavior, missing-codec recovery and Settings/F11/Esc/Stop/Start checks passed. [Exact-source Debug/Release CI passed](https://github.com/funggier/FriendPushScreen/actions/runs/38833566080). All 15 old WIP hashes are unchanged. Final documentation/evidence commit CI is checked before announcing completion.

[Report and evidence](../reports/FPS-20261016-006-codec-settings-controls.md). Original FPS-006 sustained results remain tied to their original source; short follow-up smokes do not extend that performance claim. FPS-006A remains planned; main integration remains separate.

### Two-folder localhost request and continued-system design — 2026-10-16

**Network test result: BLOCKED_NOT_IMPLEMENTED.** Source baseline 23038b26675a991b34da309c2daee8dbbcaf3c05; no product-code change in this follow-up.

Actual portable copies A/B loaded separate settings and ports 42801/42802 with cross-connected 127.0.0.1 endpoints. Real Start Share/Connect handlers only produced Requested state and inactive-transport logs; TCP/UDP port and PID checks returned no app-owned socket. A's real GPU H.264 local preview ran independently; B received no peer video. Both owned test apps closed normally. [Baseline report/evidence](../reports/FPS-20261016-dual-instance-localhost-baseline.md).

The user then explicitly requested continuing any missing system. Latest requested priority is a real No Audio network-video increment through two independent instances, followed by the remaining V1 roadmap. Optional audio remains a required later V1 outcome; FPS-006A is not marked complete or silently removed.

[Written secure dual-instance design](../../superpowers/specs/2026-10-16-secure-dual-instance-video-design.md) is PROPOSED for review. MsQuic/OpenSSL is the recommended first evaluation on Windows 10, with WebRTC as an alternative and no permanent DG-001 decision claimed. Required trust, bounded media, feedback protection, connection lifecycle and real-process qualification are specified. Written-spec review, implementation plan/execution selection, production implementation and network qualification remain pending. There is no new ACTIVE implementation task and no network GREEN claim. FPS-006's existing GREEN scope and main integration status are unchanged.
