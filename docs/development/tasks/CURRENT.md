# Current Development Task

## FPS-009 — native No Audio peer video evaluation

**Status:** ACTIVE — application implementation and two-process qualification.

**Branch/worktree:** `fps-006-encode-decode-local-loopback`, canonical linked worktree. Main and the preserved older worktrees are unchanged.

The user approved [the secure dual-instance design](../../superpowers/specs/2026-10-16-secure-dual-instance-video-design.md) and continuous Native execution on 2026-10-16. This reprioritizes the No Audio network increment ahead of FPS-006A; it does not close optional audio or full V1.

[Implementation plan](../../superpowers/plans/2026-10-16-secure-dual-instance-video.md).

Implemented and locally exercised:

- Pinned MsQuic 2.6.3/OpenSSL, native mutual TLS, normal certificate validation and full DER SHA-256 peer pin.
- Separate DPAPI-protected identities and explicit public-file pairing with full fingerprint confirmation.
- Versioned bounded control/video records, session/generation validation, independent share/receive state, heartbeat and bounded reconnect.
- Sender/receiver media pipelines using independently selected native CPU/GPU codecs.
- Real Share, Connect, Stop Share, Disconnect, Export Identity and Pair with Friend controls.
- Checked capture exclusion and CPU/GPU black-mask fallback; physical capture qualification remains open.
- Two actual portable application copies connected on different ports; no-source reception and distinct simultaneous moving patterns observed.

Current defect investigation: the first app run repeatedly reset the video stream and visibly flickered. Explicit remote decoder readiness alone did not solve it. Consumer-owned media credits now pace the sender; the first five-minute duplex run stopped the reset storm but reached only about 58 fps with rare deadline loss. Peer-only sync-interval-zero presentation is now under renewed actual application qualification. Do not call the new network increment GREEN until its remaining checks and final review pass.

Remaining acceptance work:

- Five-minute one-way and simultaneous hardware measurements, all four UI codec pairs, odd/portrait geometry.
- Actual Stop Share/Disconnect/source loss/restart and feedback masking checks.
- Secure transport comparison and explicit provisional decision-gate conclusions.
- Full Debug/Release regression, independent whole-change review, exact-final-commit CI and preserved evidence.

## Qualified prior scope

FPS-006 and its independent codec-settings follow-up remain GREEN on their recorded sources. Their sustained-performance claims remain tied to those sources.

- [FPS-006 report](../reports/FPS-20261016-006-wip-audit-and-execution.md).
- [Codec controls report](../reports/FPS-20261016-006-codec-settings-controls.md).
- [Earlier network-not-implemented baseline](../reports/FPS-20261016-dual-instance-localhost-baseline.md).
- [Native secure-byte transport foundation](../reports/FPS-20261016-native-secure-transport.md).

FPS-007 and FPS-008 code is integrated into this evaluation, with physical/runtime acceptance tracked here. FPS-010 localhost is a scoped subgate; DNS/LAN/Internet/Tailscale and a physical second PC are not claimed. DG-001/DG-006/DG-008 remain provisional/open for missing decision inputs. No release or main merge is implied.
