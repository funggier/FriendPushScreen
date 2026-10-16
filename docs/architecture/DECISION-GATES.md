# Architecture Decision Gates

These choices are intentionally unresolved. The owning task must gather evidence before selecting an option.

## DG-001 â€” Network media transport

**Owner:** FPS-002 evidence â†’ FPS-009 decision

Candidates include:

- reuse a compatible existing Sunshine/Moonlight path
- fork/adapt selected components
- implement a native FriendPushScreen transport

Decision inputs:

- end-to-end video/audio latency and bounded queue behavior
- packet loss/recovery behavior
- control/video/audio multiplexing
- one-public-UDP-port feasibility for Direct IP/DDNS
- LAN / Direct IP-DDNS / Tailscale route independence
- Direct P2P practicality
- security/session identity requirements
- A/V timestamp/jitter behavior
- maintenance burden
- GPL/license/distribution consequences

**Rule:** no permanent transport choice before benchmark, secure-Direct-Internet review, audio/video multiplexing evidence, and licensing review.

**FPS-002 evidence (2026-10-14):** Tailscale is operational with UDP available and stable mapping indication, but no second physical Windows peer is available. Sunshine/Moonlight were not installed locally. FPS-002 closes on the local reference baseline; two-peer Direct/DERP and end-to-end stream measurements are explicitly deferred to FPS-009/FPS-010. DG-001 stays OPEN.

## DG-002 â€” Monitor capture backend policy

**Status:** RESOLVED by FPS-005

**Owner:** FPS-002 evidence â†’ FPS-005 implementation

Candidates include Windows Graphics Capture and DXGI Desktop Duplication, potentially selected per source/runtime.

Decision inputs:

- latency
- capture exclusion behavior
- resize/display-change handling
- GPU-copy behavior
- Windows 10/11 compatibility

**FPS-002 evidence (2026-10-14):** Windows Graphics Capture reports supported; Desktop Duplication is available and functional; `WDA_EXCLUDEFROMCAPTURE` can be set/read as `0x11` on an owned top-level window. Native WGC-vs-DDA latency and real viewer-exclusion behavior were not yet compared, so DG-002 remained OPEN pending FPS-005.

**FPS-005 decision/evidence (2026-10-15):** V1 uses DXGI Desktop Duplication for Monitor sources and Windows Graphics Capture for Window sources behind the same capture-session abstraction. Real DDA preview was qualified on both attached displays, including cursor policy. Real WGC preview was qualified against a Notepad window on Windows 10 build 19045; closing that window produced Window source lost while preserving the selected source as unavailable rather than switching sources. A WGC startup failure 0x8001010D was traced to starting WinRT capture inside the input-synchronous button callback and fixed by deferring capture commands onto the next UI-thread message. DG-002 is RESOLVED for V1. Feedback/exclusion qualification remains owned by FPS-007 and does not reopen this backend-policy decision.

## DG-003 â€” Capture-card backend fallback

**Status:** RESOLVED by FPS-005

**Owner:** FPS-002 evidence â†’ FPS-005 implementation

Preferred direction is Media Foundation with a compatibility fallback only if real hardware demonstrates a need.

Decision inputs include actual runtime modes exposed by Onten OTN-332 and at least one generic-device path when available.

**FPS-002 evidence (2026-10-14):** the reference USB3 capture device enumerates generically as `USB3 Video` and advertises/produces 1080p60. MJPEG DirectShow capture sustained ~60 fps in a short probe; uncompressed YUYV showed buffer pressure and the live DirectShowâ†’QSV path exposed timestamp anomalies. This supports evaluating Media Foundation as planned, but does not yet qualify it.

**FPS-005 decision/evidence (2026-10-15):** the production capture-card path is Media Foundation. The real generic USB3 Video device was enumerated and previewed through Media Foundation at 1920x1080 @ 60 MJPG. The source remains device-generic and contains no Onten-specific branch. Because the reference hardware succeeds on the preferred Media Foundation path, V1 does not add a DirectShow fallback. A compatibility fallback may be reconsidered only if later real hardware demonstrates a Media Foundation gap. DG-003 is RESOLVED for V1.

## DG-004 â€” Hardware codec abstraction

**Owner:** FPS-002/FPS-003 evidence â†’ FPS-006 implementation

The API boundary must remain vendor-neutral. Backend technology is selected from runtime capability and measured behavior, not CPU vendor assumptions.

**Status: RESOLVED for the V1 codec abstraction by FPS-006 (2026-10-16).** Select native Media Foundation transforms from actual runtime subtype, classification, D3D11 capability and successful media-type/sample execution. Keep vendor-neutral IVideoEncoder / IVideoDecoder interfaces; retain exact opaque hardware-instance identity. The reference path uses Intel Quick Sync H.264 hardware encoding plus Microsoft H.264 decoding with D3D11 acceleration. A real WGC 1920x1080 local capture/encode/bounded-queue/decode/render run completed 180.020 s at 59.7824 fps, 10762 frames at every stage, zero queue drops and queue maximum 1. Explicit software H.264 encode/decode and padded-width regression runs passed. No Intel/AMD/NVIDIA branch selects the application backend. HEVC/AV1 remain capability-dependent; their missing native MF decoder pairs do not block H.264 V1. AMD/NVIDIA physical compatibility remains untested and is future evidence, not a vendor-specific interface assumption. See [FPS-006 qualification report](../development/reports/FPS-20261016-006-wip-audit-and-execution.md) for source identities, retained failures, resources and limitations.

## DG-005 â€” UI/application framework

**Status:** RESOLVED by FPS-003

**Owner:** FPS-003

Selected V1 baseline:

- C++20
- Win32 application shell
- D3D11 / DXGI rendering foundation
- CMake / MSVC
- no Qt runtime dependency

**FPS-003 evidence (2026-10-15):** the native application builds in Debug/Release locally and on Windows GitHub Actions, launches on the Windows 10 reference machine, initializes a D3D11 hardware swap chain, survives resize/minimize/maximize/close smoke tests, and keeps UI separate from platform/media diagnostics. DG-005 is RESOLVED for V1.

## DG-006 â€” Licensing/compliance boundary

**Owner:** begins FPS-002; must be resolved before FPS-009 reuse/fork integration and before FPS-015 distribution

Any reuse/link/fork decision involving GPL or third-party components must have an explicit distribution/compliance conclusion before it becomes part of the release path.

**FPS-002 evidence (2026-10-14):** current upstream metadata identifies Sunshine as GPL-3.0-only and Moonlight Qt as GPL-3.0+. The reuse/fork/link boundary remains OPEN and must be resolved before integration.

## DG-007 â€” Audio capture / codec policy

**Owner:** V1 Audio + Connectivity amendment â†’ FPS-006A evidence â†’ FPS-009 integration

Capture direction:

- WASAPI shared-mode loopback for Desktop/System Audio
- explicit render-endpoint loopback
- WASAPI capture endpoints for capture/input device audio
- native process-loopback only when the Windows runtime supports it

Current compatibility fact:

- current reference machine = Windows 10 build 19045
- Microsoft process-loopback API requires build 20348+
- therefore Application Audio is capability-dependent and must report Unsupported on the current reference machine rather than fake support

Preferred network-audio codec direction:

- Opus
- 48 kHz
- 10/20 ms real-time framing
- mono/stereo according to source

Decision inputs:

- local capture/playback latency
- encoder/decoder latency and resource use
- drift/underflow behavior
- device/source switching
- process-loopback compatibility
- codec packaging and licensing
- network loss/jitter behavior

**Rule:** Opus is the preferred evaluation baseline, not a permanent dependency until FPS-006A/FPS-009 evidence and DG-006 licensing review are complete.

## DG-008 â€” Direct Internet security / peer identity

**Owner:** V1 Audio + Connectivity amendment â†’ FPS-008 protocol â†’ FPS-009 transport â†’ FPS-010 connectivity

Direct IP/DDNS is a first-class V1 mode.

A public forwarded port must not accept unauthenticated plaintext control or media.

Required properties:

- mature authenticated encryption / secure transport
- replay-resistant handshake
- peer/session identity
- first-trust pairing/authorization
- stored peer identity verification on reconnect
- no custom cryptographic primitive
- compatible with the one-public-UDP-port operational target when practical

Tailscale may supply encrypted routing, but application peer authorization must remain route-independent.

**Rule:** Direct Internet cannot become GREEN until secure handshake and peer identity are proven on the exact transport selected by DG-001.
