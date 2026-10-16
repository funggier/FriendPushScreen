# Current Development Task

## FPS-006A — Native shared audio and selectable playback

**Status:** ACTIVE — parent implements and qualifies the approved audio increment.
**Branch/worktree:** fps-006-encode-decode-local-loopback. Main/sibling worktrees preserved.
Authority: approved V1 audio amendment, autonomous Native execution, latest continue and explicit Settings output-device request on2026-10-17.
[Design](../../superpowers/specs/2026-10-17-native-audio-design.md) · [Plan](../../superpowers/plans/2026-10-17-native-audio.md).

Implemented: pinned static Opus1.6.1, WASAPI shared48k stereo with native device conversion, System/render/input sources, independent audio session/generation and six-datagram QUIC budget, bounded jitter/PLC/render, local mute/volume and stable source/output selectors with persisted IDs. Reference19045 correctly reports process-loopback unsupported. Same-host endpoint guard prevents digital recapture loops.

Local physical PCM/Opus probes passed:192kUSB->48k conversion; ASUS48k source->USB playback499frames/10s,0render overruns. Native Release all targets/CTest3/3 pass including real independent-process transport/shutdown regression. Actual two portable apps audio/video, gains/routing/source changes, soak, whole-change review and finalDebug/Release+exactCI are in progress.

Previous FPS-009 NoAudio video evaluation is preserved in [its report](../reports/FPS-20261016-native-peer-video-qualification.md), baseline1f86e54420e5b5a245e7e509bde3902eb5f02bf3. Earlier strict unchanged-generation video FAILs remain historical evidence.

FPS-009 permanent transport decision is provisional/awaiting remaining comparison and remote qualification. DG001/006/008 and LAN/DDNS/Internet/Tailscale, physical second peer and fullV1 audio acceptance remain open. No release or main merge.
