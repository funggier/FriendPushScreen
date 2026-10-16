# Current Development Task

## FPS-009 — native No Audio peer video evaluation

**Status:** ACTIVE — the scoped localhost implementation is usable and tested; the permanent transport decision and broader qualification remain open.

**Branch/worktree:** fps-006-encode-decode-local-loopback. Main and the preserved older worktrees are unchanged.

The user approved [the secure dual-instance design](../../superpowers/specs/2026-10-16-secure-dual-instance-video-design.md) and continuous Native execution on 2026-10-16. [Plan](../../superpowers/plans/2026-10-16-secure-dual-instance-video.md) and [qualification report](../reports/FPS-20261016-native-peer-video-qualification.md).

Implemented and exercised in two actual portable copies on ports 42801/42802:

- Pinned MsQuic 2.6.3/OpenSSL, mutual TLS with normal validation and a full-DER SHA-256 peer pin, separate DPAPI identities and explicit public-file pairing.
- Bounded versioned protocol/session, independent share/receive state, consumer-paced media credits, coordinated fresh-generation recovery and accurate errors.
- Independent native CPU/GPU encoding and decoding, bitrate/GOP/encoding latency controls, no-source reception and simultaneous distinct moving pictures.
- Checked capture exclusion and CPU/GPU monitor-mask fallback, including physical DDA/WGC owned-source checks.
- Real Share/Connect/Stop Share/Disconnect/pairing controls; viewport restoration, source switch/loss and missing-decoder retry.

User-reported defects were reproduced and corrected. Status updates preserve controls and dropdown choices; Hide Settings uses the dark erase brush. The final repeated burst flicker came from the owned GDI test source exposing a background-only intermediate paint. Complete offscreen source painting produced zero unexpected pixels in 10,783 samples over three minutes, and the user confirmed no flicker. Diagnostics mouse scrolling used a transparent background for a read-only EDIT; the opaque-background fix produced zero stale pixels in four physical scroll rounds and the user confirmed it.

Current source is 8f33876f52c9e08e20c7aef44d5ba650a89a0b11; the physical app SHA256 is F117743C53205EE1494CEF4A303417FF01B7671BD0F62564997CEFB3A295D01A. Final native Debug/Release builds and CTest each passed 2/2. Exact-commit CI and measured results are recorded in the report/evidence.

Three prior five-minute a21 runs reached approximately 59.14–59.62 fps with zero transport drops. Automatic recovery caused their stricter unchanged-generation diagnostic to fail; that result remains visible in the report. A final five-minute duplex observation on the corrected source is recorded separately.

Remaining decision/acceptance inputs:

- Same-workload encrypted TCP comparison and scheduled synthetic audio multiplexing; retain DG-001 as provisional.
- Full DG-006 distribution/licensing and DG-008 Direct Internet qualification.
- Physical negative-origin monitor/card network paths, self-LAN, DNS/DDNS, Internet, Tailscale and a physical second PC.
- FPS-006A audio and the remaining full V1 roadmap.

FPS-006 and its independent codec controls remain GREEN on their recorded sources. FPS-007/FPS-008 and the FPS-010 localhost subgate have scoped evidence here, not full audio or remote-network acceptance. No release or main merge is implied.
