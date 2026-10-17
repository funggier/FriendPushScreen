# Current Development Task

## v1.0.0 release — requested 2026-10-17

**Status:** v1.0.0 RELEASE SNAPSHOT. Publication status is authoritative in the GitHub release and its exact-commit manifest. The user explicitly requested documentation and release after the bounded Host/IP and single saved-device removal work, then specified friendpushscreen-v1.0.0-portable.zip.
**Branch/worktree:** fps-006-encode-decode-local-loopback. Native inline implementation. No main merge or historical tag modification.

Completed product increments: source0c9f40398380563f3ac11da7b797112703fc6b54 unifies Host/IP/Port and bounded async DNS; sourcedfa9edeb938ecb441631d0547e8f5b98128b7efa displays/removes the one saved identity. Native Release/Debug all3CTest passed. Actual final GUI21/21 verifies delete, clear media/metrics, remote loss, restart unpaired/no listener, unchanged Host/Port, blocked Connect, exact known re-pair and duplex DDNS restored. Both bounded fresh reviews completed without findings. Owned test apps/fixtures were closed.

Self-LAN, public-name DDNS through ports8996/8997 and own Tailscale address were exercised on this PC. No physical remote peer. DDNS29.9fps and self-LAN59.95fps short runs had0drop/0recovery; own-Tailscale run had1recovery and changed generations. Preserve the strict failure.

Release process: publish only after final local build/test/package startup, exact release-commit CI, immutable tag and post-publication asset readback. Source/document archives come from that tag. Publication manifest and checksums bind exact final artifacts.

## Evidence

- docs/development/reports/FPS-20261017-native-routes-pairing.md and matching JSON.
- docs/releases/v1.0.0.md and docs/USER_GUIDE_TH.md.
- Earlier status/viewer and native-audio dated2026-10-17 reports remain authoritative for their original scopes.

## Next boundary

After this requested release, no next task is automatically activated. The approved Task6 identical encoded-workload TLS/TCP/control/scheduled-audio comparison remains outstanding. Self-LAN is now evidenced; physical peer WAN/LAN/overlay, wider recovery/audio fidelity, remote timing, installer and full V1 acceptance remain open. DG001/006/008 remain open and permanent transport selection provisional.
