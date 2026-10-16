# FPS-006 — WIP audit and native execution qualification
Date: 2026-10-16 (Asia/Bangkok)
Status: GREEN — qualified on the canonical feature branch; exact final evidence-commit CI is checked before announcing closure.
Authority: canonical branch `fps-006-encode-decode-local-loopback`; resumed at `ed6a07f2a008b3fafa717981c45d7510aa7d1b46`.
Machine: CDQ-P, Windows 10 build 19045, Intel Core Ultra 5 245K.
Task: #14. No network, control, or audio changes.

## WIP audit before new conversion/encoder work
Both older worktrees were read without reset, clean, build, or writes. Their existing binaries predate their latest source edits, so old passing logs do not qualify the WIP.

| Source | Decision | Reason |
|---|---|---|
| fps-006-video-codec-local-loopback, graphics/d3d11_video_converter.* | Selectively port | Existing bidirectional VideoProcessor implementation fits CapturedFrame; caller owns destinations and decoder subresource is explicit |
| fps-006-codec-local-loopback, media/d3d11_video_converter.* | Reference | Useful pitch-aware readback and CPU uploads; public decoder conversion ignores array slice; reusable NV12 can be overwritten while async MFT consumes it |
| fps-006-codec-local-loopback, media/video_frame_converter.* | Do not wholesale port | CPU conversion/reference useful, but BT.601 conflicts with chosen SDR BT.709; GPU readback cannot be normal monitor/window path; malformed odd NV12 validation unsafe |
| fps-006-codec-local-loopback, media/mf_h264_codec.* | Adapt sample/event concepts | Old model differs; timestamp mapping is FIFO, pending decoder sample lease lost, async backpressure can stall, drain not resumable |
| fps-006-video-codec-local-loopback, media/mf_video_encoder.* | Adapt negotiation/CodecAPI patterns | Single reused NV12 surface unsafe for async input; boolean input event credit loses multiplicity; waiting for every output can deadlock buffering |
| Canonical clock/catalog/activation/media types/queue/metrics | Retain | Current task contracts and prior model tests already committed |

Implementation direction: shared sync/async MFT session, timestamp keyed metadata, bounded eight-surface encoder pool retained through encoded output, tracked decoder sample retained until GPU copy/conversion fence completes, explicit software encode/decode forcing, actual runtime-selected transforms.

## Conversion checks
Physical test target: `FriendPushScreenConversionTests`, manually invoked; never silently skipped/passed on generic CI.
RED on ported WIP: 2 failures — short CPU pitch accepted, padded decoder texture rejected.
GREEN after corrections: 0 failures, native BGRA→NV12→BGRA executes, BT.709 red luma 63, decoder slice index and padded extent validation.
SDR contract: BT.709 primaries/matrix/transfer, full-range RGB, limited-range NV12. Codec media types carry matching attributes.
Decoder input is copied from the requested validated subresource and visible rectangle. Output CapturedFrame owns BGRA texture.
No conversion success is inferred solely from resource creation.

## Evidence rules
- MFT catalog classification is preserved; Microsoft decoder remains a software MFT even when D3D11 acceleration executes.
- Device inventory, codec discovery/activation, accelerator creation, real compressed-bitstream execution, and sustained capture/render are separate evidence levels.
- HEVC/AV1 absence through MF does not imply silicon lacks them.
- No AMD/NVIDIA physical qualification is claimed.
- Final source/execution evidence is recorded below; runtime source commit is 1a1bbd194d08459e2479942a4b5df43e83628217.

## Implemented local path

The application renders decoded H.264 by default. FPS-005 Monitor/DDA, Window/WGC and VideoDevice/MF sessions feed the same VideoLocalLoop, two-access-unit bounded queue and existing PreviewRenderer. A codec failure presents an error/placeholder, retains the selected source, and never silently claims success through raw preview. FPS_RAW_PREVIEW=1 is an explicit diagnostic override; FPS_FORCE_SOFTWARE_CODEC=1 forces both software wrappers.

The shared MFT session supports sync and async transforms, counts input/output event credits, services output while awaiting input, renegotiates stream changes, and restarts explicitly after flush/drain. Intel's async stream-change event consumes its output credit; retrying ProcessOutput immediately was a real runtime failure and is fixed.

Encoder NV12 input uses eight bounded owned surfaces; pending samples retain their surface until timestamp-matched output. Decoder output keeps the tracked native sample alive until GPU copy/conversion completes. Decoder array slices, 1920x1088 coded surfaces with a 1920x1080 display aperture, and software row pitch are honored. Source timestamps/sequence are retained behind the FriendPushScreen monotonic codec clock; metadata is keyed by codec sequence/timestamp and bounded.

CPU-backed capture input is counted and uploaded explicitly. Normal WGC/DDA input stays on GPU. Odd capture dimensions are padded for NV12 and cropped back to the original visible dimensions after decode. Sender cursor metadata reaches the existing renderer unchanged.

The inherited application WM_TIMER ran every 100 ms, limiting actual preview to 10 fps. The application now services UI messages before capture/codec steps, lets Present pace rendered frames, waits for messages when stopped, and waits briefly only when active but idle. A 1 ms timer-resolution request is paired with capture start/stop. Optional FPS_CODEC_METRICS=1 writes backend/counter/latency snapshots to the existing diagnostic logger.

## Physical hardware and codec inventory

Reference: CDQ-P, Windows 10 build 19045; Core Ultra 5 245K, 32 GB RAM; one Intel(R) Graphics adapter, PCI 8086:7d67, driver 32.0.101.8826. No AMD/NVIDIA adapter was present.

| Codec | Native MF encoder | Native MF decoder | Independent installed QSV bitstream execution |
|---|---|---|---|
| H.264 | Intel Quick Sync Video H.264 Encoder MFT: hardware, D3D11, async; H264 Encoder MFT: software, sync | Microsoft H264 Video Decoder MFT: software catalog class, D3D11-aware; actual GPU surfaces and media-engine activity verified | QSV encode and decode: 120 frames, 1920x1080 at 60, both exit 0 |
| HEVC | Intel Hardware H265 Encoder MFT: hardware, D3D11, async; activates | None enumerated | QSV encode and decode: 120 frames, 1920x1080 at 60, both exit 0 |
| AV1 | None enumerated | None enumerated | QSV encode and decode: 120 frames, 1920x1080 at 60, both exit 0 |

The raw native enumeration originally repeated identical Intel registrations. Catalog deduplication now uses CLSID, name, classification and the exact opaque hardware URL. Final counts: H.264 encode 2 / decode 1, HEVC encode 1 / decode 0, AV1 encode 0 / decode 0. Different hardware URLs are not collapsed, even if CLSID/name match; activation resolves the exact descriptor.

D3D11 enumerates 68 decoder profiles. Creating a 1080p accelerator/configuration succeeded for H.264 VLD, HEVC Main/Main10, VP9 Profile0/Profile2 10-bit and AV1 Profile0, using NV12/P010 where exposed. This is accelerator-creation evidence; only actual bitstream runs establish encode/decode execution.

External FFmpeg 8.1.2/QSV was already installed and was used solely as independent capability evidence. No FFmpeg dependency or Intel-specific application branch was added. Missing HEVC/AV1 MF pairs do not block the H.264 V1 baseline.

### Native encoder/decoder isolation

All modes use production MFT wrappers and request IDR at frame 37, away from the configured GOP boundary:

| Mode | Input / encoded / decoded | Quality failures | GPU encoder / decoder frames | Observed fps |
|---|---:|---:|---:|---:|
| Hardware encode + software decode, 12 s | 720 / 720 / 720 | 0 | 720 / 0 | 59.87 |
| Software encode + accelerated decode, 12 s | 720 / 720 / 720 | 0 | 0 / 720 | 59.94 |
| Hardware encode + accelerated decode, 30 s | 1800 / 1800 / 1800 | 0 | 1800 / 1800 | 59.93 |

Each had queue maximum 1, zero queue drops, exact timestamp/sequence/dimension handoff and 13/13/31 keyframes respectively. Alternating red/blue frames are checked after real encode/decode. Forced software encode+decode also passed 120/120; a 1366x768 software regression run passed after stride correction.

Windows names Intel media nodes engtype_VideoDecode even in the isolated hardware-encoder/CPU-decoder run. That run showed a media-node average 9.83%; the isolated CPU-encoder/accelerated-decoder run showed 2.9%. Node assignment can vary; evidence records literal counter names, selected transforms, explicit software forcing and GPU/CPU sample counts instead of renaming a counter from assumption.

## Review and regression evidence

One independent whole-branch review covered c9c0af0..0d3aa3a. No Critical finding. All three Important findings were reproduced/fixed in the parent verification pass:

1. Software decoder omitted stride: a 1376x768 coded surface with 1366x768 display aperture must use coded stride 1376, not visible width 1366. RED failed expected stride; GREEN validates computed/explicit pitch, cropped luma/UV rows, negative/short pitches and truncated buffers. Native forced-software 1366x768 passed.
2. Distinct hardware activation instances shared CLSID/name/classification. RED matched the wrong opaque URL; GREEN matches the complete descriptor, deduplicates only identical registrations and preserves distinct instance fallback.
3. Original force-keyframe probe requested frame 61 on a 60-frame GOP and could pass on an unrelated periodic IDR. Negative run with exact frame-37 assertion failed; hardware and forced-software runs now request frame 37 and require that exact access unit to be a keyframe.

Minor hard-coded software decoder label in the probe was also fixed. Deferred Minor: converter Initialize does not clear scratch/processor caches when reused with a different D3D11 device. Current shipping codec wrappers reconstruct the converter on every restart, so this is outside their reachable restart path; do not reuse a converter across devices without addressing it.

Async stream-driver edge cases have physical runtime evidence rather than a complete mocked native event-driver matrix. Generic CI includes selection/forcing, clock normalization, queue capacity/overflow/IDR, state/error behavior, metrics, media types and decoded metadata/cursor handoff. Runtime tests are explicit physical executables, not silently skipped generic CI tests.

Rulings: use real WGC input and 180 s duration for the sustained gate; activation and external QSV are insufficient substitutes. Network/audio/control and transport policy remain deferred scope. AMD/NVIDIA physical tests remain future compatibility evidence; interfaces remain vendor-neutral.

## Qualification methodology and retained failures

Moving 1920x1080 window -> original FPS-005 WGC -> D3D11 BGRA/NV12 -> native H.264 encode -> bounded queue -> decode -> existing PreviewRenderer. The source owns a separate thread and phase-preserving 60 Hz clock. Four source borders and color stripes mark full-frame geometry.

Sustained acceptance: at least 59 rendered fps averaged over 180 s including codec initialization/drain, 1920x1080, matching submitted/encoded/decoded/rendered counts, zero queue drops, final queue empty. Tail p95 uses the bounded most recent 2048 samples; max/average cover the whole run. Latency starts on capture acquisition, not source exposure. Encoded bitrate is actual bytes/duration.

| Run | Seconds | Rendered | fps | Queue max / drops | Outcome |
|---|---:|---:|---:|---:|---|
| Original main-thread fixture | 180.021 | 10417 | 57.87 | 1 / 0 | FAIL |
| Phase-preserving clock, source thread shared with pipeline | 180.020 | 10449 | 58.04 | 1 / 0 | FAIL |
| Independent source, per-second external process/GPU sampling | 180.020 | 10284 | 57.13 | 1 / 0 | FAIL |
| Counter A/B, GPU counters disabled | 25.026 | 1394 | 55.70 | 1 / 0 | FAIL |
| Direct invocation, sparse >=100 ms phase tracing | 30.029 | 1778 | 59.21 | 1 / 0 | Short PASS only |

Counter A/B did not prove GPU sampling caused stalls. Do not claim that root cause. Separate direct invocation measures CPU/memory internally and uses isolated jobs for GPU-engine evidence; its sustained result and application regression checks follow after completion.

The original Windows PowerShell Start-Process wrapper could report a null exit code after child exit. It now retains the process handle immediately, treats null as failure and requires the exact final PASS marker. The second failing 180 s run correctly returned exit 1. Original raw CAPTURE_LOOP_FAIL remains authoritative despite its wrapper's null exit field.

Evidence: ../evidence/fps-006-20261016/ contains text logs, identities, resource summaries, regression negatives and preserved WIP hashes. Unrelated enumerated window titles were removed from committed capture logs; raw local logs remain under ignored .superpowers. No captured desktop video is committed.


## Sustained native hardware result

Source commit: 1a1bbd194d08459e2479942a4b5df43e83628217, clean source tree at launch; binary SHA-256 and exact command in sustained-180s-identity.json. Direct invocation returned exit 0 and CAPTURE_LOOP_PASS.

| Measurement | Result |
|---|---:|
| Real WGC input / decoded output | 1920x1080, 60 fps target |
| Elapsed, including codec initialization and drain | 180.020 s |
| Captured / submitted / encoded / decoded / rendered | 10762 each |
| Observed rendered fps | 59.7824 |
| Encoded bitrate | 19.8161 Mbps, 445910870 bytes |
| Queue max / final / drops / recovery | 1 / 0 / 0 / 0 |
| GPU encoder / decoder samples | 10762 / 10762 |
| CPU capture / padded frames | 0 / 0 |
| Encode latency avg / p95 / max | 17.319 / 18.862 / 41.009 ms |
| Decode latency avg / p95 / max | 1.626 / 1.723 / 29.481 ms |
| Capture-acquisition to rendered avg / p95 / max | 32.931 / 34.499 / 64.136 ms |
| CPU average, all logical cores | 2.84381% |
| Private bytes min / max | 250929152 / 267698176 |
| Working set max | 235212800 |

Private memory cycled within about 239.3–255.3 MiB after initialization, with no increasing trend over the run. The bounded queue and pending metadata/surface limits prevent unbounded accumulation. GPU evidence is from the separate isolated encoder/decoder jobs; no concurrent GPU-counter sampler ran in this sustained measurement. No sparse stage call exceeded 100 ms in this run.

## Build / CI source identity

Local full Debug and Release builds plus CTest each passed (1/1 CTest target; full native suite). Physical conversion tests passed in Debug and Release. Runtime source commit 1a1bbd194d08459e2479942a4b5df43e83628217 has successful Windows Debug/Release GitHub Actions [run 38827576279](https://github.com/funggier/FriendPushScreen/actions/runs/38827576279). Previous checkpoint runs 38824677044 (0d3aa3a) and 38826728264 (54c16b1) also passed. Final documentation/evidence HEAD receives its own exact-commit CI before closure.

## Actual application and viewer regression checks

The real FriendPushScreen.exe from source 1a1bbd1 ran with FPS_CODEC_METRICS=1 and its own isolated portable settings, consuming the standalone 60 Hz moving-source fixture through the normal Source -> Preview UI commands. A first interactive observation included window movement, Tab/F11/Esc and Stop; it measured 58.46 fps over 112.11 s and is retained, not called the sustained result. After Stop -> Start, the unchanged selected source ran 202.157 s, 12044 captured, 12043 encoded/decoded/rendered, 59.5725 rendered fps; queue max 1, zero drops. GPU input/output counts 12044/12043. At source closure an in-flight sample may remain because stop is immediate; matching fully drained stage counts are separately proven by the 180 s probe.

Verified on the physical application: Settings open/hidden viewer remains 1264x691; status strip is a separate 1264x30 rectangle above it. F11 viewer is full client 2560x1440; Esc restores 1264x691. Decoded source border, stripes and moving rectangle remain complete under aspect-fit; Settings overlays the viewer without resizing it. Closing the owned source showed Window source lost and preserved Window - FPS-006 1080p moving source [Unavailable], with Preview stopped and no source substitution. Own test application and fixture were closed afterward. No user app/settings were replaced.

Application metrics, source-loss readback, geometry JSON and screenshots are in the evidence directory. These checks exercise the application cadence fix and production decoded-preview integration, not only the standalone probe.

## Forced software and other capture sources

All three final source smokes returned exit 0 and CAPTURE_LOOP_PASS. They prove functional local pipelines; the latter two are not claimed as sustained 60 fps qualifications.

| Source / codec policy | Duration | Frames at every drained stage | fps | CPU, all cores | CPU capture frames | GPU encoder / decoder |
|---|---:|---:|---:|---:|---:|---:|
| WGC / forced software encode and decode | 12.009 s | 706 | 58.79 | 7.63% | 0 | 0 / 0 |
| DISPLAY2 DDA / hardware | 10.010 s | 563 | 56.24 | 2.44% | 0 | 563 / 563 |
| USB3 Video MF / hardware | 10.035 s | 520 | 51.82 | 9.38% | 520 | 520 / 520 |

Every run had queue max 1, final 0, zero codec-queue drops/resets. Device capture reported 14 source-sequence gaps; that is acquisition loss before the codec queue, not a queue drop. CPU-backed device upload is explicit and measured. Forced-software private memory remained 350863360–350920704 bytes; encode/decode average 6.273/4.365 ms and capture-acquisition-to-render average 16.411 ms. Codec GPU-frame counters were zero, while conversion/render still used D3D11 as designed.

## Gate disposition and limits

- PASS: WIP audit before implementation; 15 old WIP source hashes unchanged; no reset/clean or old-worktree writes.
- PASS: physical BGRA/NV12 conversion, hardware and forced software H.264 sample execution, synthetic color/metadata checks and off-GOP exact IDR assertion.
- PASS: original FPS-005 capture -> bounded codec queue -> decoded PreviewRenderer; 180 s real WGC hardware qualification plus 202.157 s real application observation at 59.5725 fps.
- PASS: source selection/Stop/Start/source-loss behavior and viewer Settings/F11/Esc/full-frame geometry.
- PASS: Debug/Release local build+CTest and exact runtime-source Windows CI; final documentation/evidence HEAD is also checked through GitHub Actions before final closure.
- RESOLVED: DG-004 V1 vendor-neutral native-MF abstraction. No application vendor branching.

Limits: only Intel reference hardware is physically available. HEVC/AV1 QSV capability does not establish native-MF local-loop support. Monitor/device short-smoke throughput is below 60 and is not covered by the WGC sustained performance claim. External process-launch/sampling runs showed occasional stalls that were not fully attributed; their failures remain visible alongside direct and actual-application successes. Deferred converter cross-device reinitialization Minor remains as described above. Network, control, audio, feedback protection and transport adaptation remain their existing later tasks; FPS-006A is not started automatically.

Reproduction: full build with cmake --build build --config Debug|Release --parallel 8; ctest --test-dir build -C Debug|Release --output-on-failure. Invoke FriendPushScreenConversionTests explicitly on a D3D11 machine. For sustained native measurement invoke build/Release/FriendPushScreenCaptureLoopProbe.exe --source window --seconds 180 --require-1080p60; use --software without the hardware performance requirement for fallback. The PowerShell qualification helper additionally collects process/GPU counters and records exact source/binary identity. Use --fixture-only --seconds 600 for the real application Source/Preview checks. FPS_CODEC_METRICS=1 enables application snapshots in its regular diagnostic log.
