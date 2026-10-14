# Architecture Decision Gates

These choices are intentionally unresolved. The owning task must gather evidence before selecting an option.

## DG-001 — Network video transport

**Owner:** FPS-002 evidence → FPS-009 decision

Candidates include:

- reuse a compatible existing Sunshine/Moonlight path
- fork/adapt selected components
- implement a native FriendPushScreen transport

Decision inputs:

- end-to-end latency and queue behavior
- packet loss/recovery behavior
- Direct P2P practicality
- security/session identity requirements
- maintenance burden
- GPL/license/distribution consequences

**Rule:** no permanent transport choice before reference-stack benchmark and licensing review.

**FPS-002 evidence (2026-10-14):** Tailscale is operational with UDP available and stable mapping indication, but no second physical Windows peer is available. Sunshine/Moonlight were not installed locally. FPS-002 closes on the local reference baseline; two-peer Direct/DERP and end-to-end stream measurements are explicitly deferred to FPS-009/FPS-010. DG-001 stays OPEN.

## DG-002 — Monitor capture backend policy

**Owner:** FPS-002 evidence → FPS-005 implementation

Candidates include Windows Graphics Capture and DXGI Desktop Duplication, potentially selected per source/runtime.

Decision inputs:

- latency
- capture exclusion behavior
- resize/display-change handling
- GPU-copy behavior
- Windows 10/11 compatibility

**FPS-002 evidence (2026-10-14):** Windows Graphics Capture reports supported; Desktop Duplication is available and functional; `WDA_EXCLUDEFROMCAPTURE` can be set/read as `0x11` on an owned top-level window. Native WGC-vs-DDA latency and real viewer-exclusion behavior are not yet compared, so DG-002 stays OPEN.

## DG-003 — Capture-card backend fallback

**Owner:** FPS-002 evidence → FPS-005 implementation

Preferred direction is Media Foundation with a compatibility fallback only if real hardware demonstrates a need.

Decision inputs include actual runtime modes exposed by Onten OTN-332 and at least one generic-device path when available.

**FPS-002 evidence (2026-10-14):** the reference USB3 capture device enumerates generically as `USB3 Video` and advertises/produces 1080p60. MJPEG DirectShow capture sustained ~60 fps in a short probe; uncompressed YUYV showed buffer pressure and the live DirectShow→QSV path exposed timestamp anomalies. This supports evaluating Media Foundation as planned, but does not yet qualify it.

## DG-004 — Hardware codec abstraction

**Owner:** FPS-002/FPS-003 evidence → FPS-006 implementation

The API boundary must remain vendor-neutral. Backend technology is selected from runtime capability and measured behavior, not CPU vendor assumptions.

**FPS-002 evidence (2026-10-14):** Intel QSV H.264/HEVC/AV1 encode and decode are available and were exercised; simultaneous H.264 1080p60 encode/decode completed in a short probe. Intel-side feasibility is established, but AMD/NVIDIA remain untested, so the cross-vendor gate stays OPEN.

## DG-005 — UI/application framework

**Owner:** FPS-003

The existing specification mentions Qt deployment considerations but does not establish an irreversible framework decision. FPS-003 must choose a Windows application stack based on capture/render integration, deployment burden, supported OS versions, and maintainability.

## DG-006 — Licensing/compliance boundary

**Owner:** begins FPS-002; must be resolved before FPS-009 reuse/fork integration and before FPS-015 distribution

Any reuse/link/fork decision involving GPL or third-party components must have an explicit distribution/compliance conclusion before it becomes part of the release path.

**FPS-002 evidence (2026-10-14):** current upstream metadata identifies Sunshine as GPL-3.0-only and Moonlight Qt as GPL-3.0+. The reuse/fork/link boundary remains OPEN and must be resolved before integration.
