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

## DG-002 — Monitor capture backend policy

**Owner:** FPS-002 evidence → FPS-005 implementation

Candidates include Windows Graphics Capture and DXGI Desktop Duplication, potentially selected per source/runtime.

Decision inputs:

- latency
- capture exclusion behavior
- resize/display-change handling
- GPU-copy behavior
- Windows 10/11 compatibility

## DG-003 — Capture-card backend fallback

**Owner:** FPS-002 evidence → FPS-005 implementation

Preferred direction is Media Foundation with a compatibility fallback only if real hardware demonstrates a need.

Decision inputs include actual runtime modes exposed by Onten OTN-332 and at least one generic-device path when available.

## DG-004 — Hardware codec abstraction

**Owner:** FPS-002/FPS-003 evidence → FPS-006 implementation

The API boundary must remain vendor-neutral. Backend technology is selected from runtime capability and measured behavior, not CPU vendor assumptions.

## DG-005 — UI/application framework

**Owner:** FPS-003

The existing specification mentions Qt deployment considerations but does not establish an irreversible framework decision. FPS-003 must choose a Windows application stack based on capture/render integration, deployment burden, supported OS versions, and maintainability.

## DG-006 — Licensing/compliance boundary

**Owner:** begins FPS-002; must be resolved before FPS-009 reuse/fork integration and before FPS-015 distribution

Any reuse/link/fork decision involving GPL or third-party components must have an explicit distribution/compliance conclusion before it becomes part of the release path.
