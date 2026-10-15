# FPS-006 — Encode / Decode / Local Loopback Implementation Plan

**Task:** FPS-006  
**Issue:** #14  
**Branch:** `fps-006-encode-decode-local-loopback`  
**Base:** `c9c0af087fe039ae1a8c8f695411560b147f3ef4`

## Goal

Insert the production-shaped V1 video codec layer between the already-qualified FPS-005 capture sessions and viewer renderer, then qualify a sustained local capture → encode → bounded queue → decode → render loop at 1920x1080 @ 60 fps on the reference Windows 10 machine.

FPS-006 remains completely local. No network/control/audio dependency is allowed.

## Inherited FPS-005 contract

The codec layer consumes `capture::CapturedFrame` from the existing unified source sessions:

- Monitor → DDA
- Window → WGC
- VideoDevice → Media Foundation

The frame contract already owns:

- source sequence
- 100 ns timestamp
- width / height
- BGRA pixel format
- optional D3D11 texture
- optional CPU BGRA pixels
- sender cursor policy

The viewer contract already owns:

- complete-frame aspect-fit rendering
- no crop-to-fill
- windowed status strip outside the viewer
- Settings overlay that does not resize the viewer
- F11 full-client viewer

FPS-006 must preserve all of those behaviors.

## Architecture

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
D3D11 BGRA → NV12 conversion
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
Decoded NV12 surface
      │
      ▼
D3D11 NV12 → BGRA conversion
      │
      ▼
CapturedFrame-compatible decoded frame
      │
      ▼
existing PreviewRenderer
```

No transport packetization belongs in FPS-006.

## Core codec model

### VideoCodecKind

- H264
- HEVC
- AV1

### VideoCodecBackend

Backend identity must describe the actual runtime transform, not infer from CPU vendor.

At minimum record:

- friendly transform name
- hardware / software classification
- encoder / decoder direction
- accepted input/output media subtypes
- D3D11-aware capability where exposed

### VideoCodecConfig

At minimum:

- codec
- width
- height
- fps numerator / denominator
- target bitrate
- keyframe interval
- low-latency preference
- hardware preference/requirement policy

Default FPS-006 qualification profile:

- 1920x1080
- 60 fps
- H.264
- SDR
- 20 Mbps initial local-loop target
- low latency enabled
- bounded GOP suitable for rapid recovery

The bitrate is a qualification baseline, not the final FPS-013 adaptation policy.

## Runtime backend selection

Use Media Foundation MFT enumeration.

Selection rules:

1. Filter by requested codec and direction.
2. Prefer transforms with explicit hardware evidence.
3. Verify compatible input/output types before selection.
4. Verify D3D11 manager/device compatibility where required.
5. Never select from CPU vendor name.
6. H.264 must have a software fallback path that can be explicitly forced for qualification.
7. HEVC/AV1 remain capability-dependent and never block FPS-006 GREEN.

The selected encoder and decoder may come from different vendors/backends.

## D3D11 device / surface policy

FPS-006 uses the same D3D11 device family as capture/preview whenever compatible.

Preferred hardware path:

```text
Captured BGRA texture
  → D3D11 VideoProcessor / equivalent native GPU conversion
  → NV12 texture
  → MFCreateDXGISurfaceBuffer
  → IMFMediaBuffer / IMFSample
  → hardware MFT
```

Decoder output should remain a D3D11 surface when the transform supports it.

Decoded NV12 is converted back to BGRA for the existing PreviewRenderer during FPS-006. A future renderer may consume NV12 directly, but that optimization is not required to qualify this task.

CPU-backed capture frames may be uploaded into a reusable D3D11 BGRA texture before conversion. That upload must be measured and must not become the normal monitor/window path.

## Media Foundation transform wrapper

Create focused wrappers rather than exposing raw `IMFTransform` throughout the application.

Required responsibilities:

### Encoder

- activate selected encoder
- configure H.264 output
- configure NV12 input
- attach `IMFDXGIDeviceManager` where supported
- set frame size/rate/bitrate
- request low-latency mode where supported
- set GOP/keyframe policy where supported
- accept timestamped NV12 samples
- drain complete encoded access units
- expose transform name/classification/errors

### Decoder

- activate selected decoder
- configure H.264 input
- configure NV12 output
- attach D3D11 manager where supported
- accept complete encoded access units
- handle stream changes
- emit decoded surfaces/frames
- expose transform name/classification/errors

Async MFTs must be handled correctly; do not assume every hardware MFT is synchronous.

## Frame clock / timestamp normalization

FriendPushScreen owns monotonic codec timing even when a capture backend delivers repeated, zero, or non-monotonic timestamps.

Create a small frame-clock normalizer that produces:

- monotonically increasing codec sequence
- monotonically increasing 100 ns timestamp
- frame duration
- original capture sequence/timestamp retained for metrics

For a 60 fps target the ideal duration is approximately 166,667 × 100 ns.

Tests must include:

- monotonic input
- repeated timestamp
- regressed timestamp
- missing/zero timestamp
- sequence gaps

Do not rewrite the FPS-005 capture contract merely to satisfy the codec.

## Encoded access unit

Each queue item must represent one complete decodable access unit and carry at minimum:

- codec
- codec sequence
- presentation timestamp
- duration
- keyframe flag
- encoded bytes or native buffer ownership
- source capture sequence
- enqueue timestamp for latency metrics

The network framing format is intentionally deferred to FPS-009.

## Bounded queue policy

The encode→decode queue must be explicitly bounded.

Initial qualification capacity:

- target depth: 1–2 frames
- hard maximum: small fixed frame/access-unit count, never unbounded

Rules:

- producer must never grow memory indefinitely
- stale frames are less valuable than recent frames
- inter-frame dependency must be respected
- on overflow/recovery, do not randomly remove arbitrary predictive frames and continue as though the stream remained decodable
- if continuity is broken, clear stale dependent data and request/await a fresh keyframe/IDR
- record queue drops, recovery resets and maximum observed depth

Unit tests must prove bounded behavior.

## Local loop state

Add a codec-loop state model independent from future network state:

- Stopped
- Starting
- Running
- Recovering
- Error

The existing capture source remains selected across codec-loop errors unless the source itself is lost.

No automatic source substitution.

## Application integration

FPS-005 raw preview remains a useful diagnostic capability, but FPS-006 must add a path that actually renders decoded output.

The application loop must be able to run:

```text
capture frame
  → codec local loop
  → decoded frame
  → PreviewRenderer
```

Qualification must make it unambiguous whether the viewer is showing raw capture or decoded local-loop output.

Do not silently fall back to raw preview after a codec failure while claiming codec-loop success.

## H.264 required qualification

FPS-006 cannot become GREEN without both:

### Hardware path

Real local:

```text
1080p60 capture
→ hardware H.264 encode
→ bounded queue
→ hardware-capable H.264 decode when available
→ D3D11 render
```

### Software fallback

Force hardware selection off and prove a functional Microsoft/software H.264 encode/decode path.

Software fallback does not need to equal hardware 1080p60 performance, but it must be functionally correct and measured.

## HEVC / AV1 evaluation

Probe and record:

- hardware encoder availability
- hardware decoder availability
- transform names
- whether required input/output surfaces can be configured

If practical on the reference machine, perform a short local encode/decode smoke test.

Failure or absence does not block FPS-006 GREEN because H.264 is the V1 compatibility baseline.

## Metrics

Internal local-loop metrics must include at least:

- capture frames received
- frames submitted to encoder
- encoded access units
- decoded frames
- rendered decoded frames
- input/queue drops
- recovery/keyframe resets
- current queue depth
- maximum queue depth
- encode latency
- decode latency
- capture→decoded/render latency
- observed FPS

Qualification evidence should report average plus useful tail behavior (for example p95/max where the sample count is meaningful).

Resource evidence must include reference-machine observations for:

- CPU utilization
- GPU/video encode/decode engine activity where available
- process memory / queue stability

External Windows counters may supplement internal metrics.

## 1080p60 sustained qualification

Use a real FPS-005 capture source capable of stable 1080p60 input.

Run long enough to distinguish a short startup success from sustained behavior.

Required evidence:

- exact selected encoder/decoder
- hardware/software classification
- input/output resolution/fps
- encoded bitrate
- elapsed duration
- frames in/encoded/decoded/rendered
- queue max depth and drop count
- latency metrics
- CPU/GPU/resource observations
- no unbounded memory/queue growth
- no viewer aspect/layout regression

## DG-004 decision

FPS-006 owns the V1 codec abstraction decision.

DG-004 can resolve only when evidence proves:

- selection is based on runtime transform capability
- Intel reference hardware works without Intel-specific application branching
- software H.264 fallback is functional
- interfaces do not encode Intel/AMD/NVIDIA assumptions

AMD/NVIDIA physical qualification may remain future compatibility evidence if those GPUs are not available locally, but the architecture must remain vendor-neutral.

## Tests

Unit-level:

- codec candidate ranking
- hardware/software forcing policy
- H.264 fallback selection
- frame-clock monotonicity
- encoded queue capacity
- overflow / recovery / keyframe policy
- metrics accounting
- state transitions
- media-type/config validation
- decoded-frame metadata handoff

Local/physical:

- real H.264 hardware encoder activation
- real H.264 decode activation
- local capture→encode→decode→render
- forced software H.264 fallback
- sustained 1080p60 hardware loop
- source stop/restart around codec loop
- Settings overlay does not resize the viewer
- full-frame aspect-fit still preserves the entire decoded image
- F11 still uses full client viewer area

Build gates:

- Debug build + CTest
- Release build + CTest
- exact-commit Windows Debug/Release CI
- dated qualification report before GREEN closure

## Explicit non-goals

FPS-006 does not implement:

- network transport
- packet encryption
- peer handshake
- control protocol
- audio capture/codec/playback
- feedback protection
- bitrate adaptation from network conditions
- two-peer codec negotiation

Those remain FPS-006A / FPS-007 / FPS-008 / FPS-009+.
