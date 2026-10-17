# Current Development Task

## FPS-006A — Native audio and selectable playback: scoped qualification complete

**Status:** SCOPED PASS on the reference Windows10 machine and two real localhost portable instances.
**Branch/worktree:** fps-006-encode-decode-local-loopback, preserved. Main and sibling worktrees untouched.
Authority: approved V1 audio amendment, autonomous Native execution, explicit Settings output selector and steady-audio requirement.

[Qualification report](../reports/FPS-20261017-native-audio-qualification.md) · [Curated evidence](../evidence/FPS-20261017-native-audio.json) · [Completed plan](../../superpowers/plans/2026-10-17-native-audio.md).

Implemented: actual WASAPI System/output-loopback/input capture, pinned Opus1.6.1, authenticated QUIC audio with independent generations and finite queues; local output selection, mute and volume with persisted stable IDs. NoAudio releases capture/send. Same-host endpoint guard prevents recapture loops. Windows19045 correctly marks process capture unsupported.

Verified source9d24f9ed61470ad2ab4fd7a8b5beef8fecffb381, appSHA256B1CC600361EAAF1783E9DE4AC66CEC09951544C5DB2E71CBF92F1B7D4A57E7BC: all native Debug/Release targets and CTest3/3, exact-source CI both configurations,13/13 physical audio/routing cases,5min duplex50audio frames/s with0drops/PLC/underflows,0silent10msblocks at measured output,59.8fps video with0recoveries and unchanged generations in the final run. Actual close/relaunch preserved both endpoint/mute/volume choices. Visible Diagnostics scroll repaint4/4. Single whole-change review and one consequential TDD fix pass completed.

Owned apps/fixtures/helpers are closed; original portable test preferences restored, identities retained. No main merge or release.

Next gated scope: FPS-009 transport comparison/permanent decision and FPS-010 remote connectivity; DG001/006/008 remain open. Physical endpoint removal/default switching, external capture-card signal fidelity, newer-OS process backend, LAN/DDNS/Internet/Tailscale, physical second peer and fullV1 acceptance remain unqualified. Earlier strict video-continuity FAILs are retained in the reports; a single final scoped PASS does not erase them.
