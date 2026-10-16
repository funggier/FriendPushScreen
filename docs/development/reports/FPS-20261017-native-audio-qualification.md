# Native audio qualification — 2026-10-17
Status: qualification in progress; this is a scoped localhost increment, not V1 release acceptance.
Baseline: 1f86e54420e5b5a245e7e509bde3902eb5f02bf3. Branch: fps-006-encode-decode-local-loopback. Native parent implementation; main and sibling worktrees preserved.
User added an explicit Settings playback-device selector request at05:13:14 Asia/Bangkok.

## Implemented behavior
Audio Source selects No Audio, default System mix, an explicit output loopback or active input endpoint. Remote Audio Output selects Default Output Device or an explicit active render endpoint, retaining its stable ID. Mute and volume are local and persisted. Source changes create an independent audio generation; No Audio releases source/encoder and stops audio packets. Existing Share/Stop/Connect, video codec choices, painting fixes and separate trusted identities remain.

WASAPI requests shared48kHz stereo float with Windows conversion; Opus1.6.1 uses20ms frames, constrained VBR128kbit/s, packet <=900bytes. Official archive SHA2566ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1. Static Opus BSD notice is copied beside the portable app; project MIT license remains.

Audio device/codec operations run on their own COM-MTA worker. Device opens cannot hold the UI/transport queue mutex. Handoffs bound audio to6 datagrams per queue and controls to16; transport holds6 receive leases including consumer-held frames, jitter holds6 frames/120ms, renderer PCM <=2880frames/60ms. Video retains its separate2-unit/16MiB budget. Control dispatch precedes audio then video; MsQuic default wire scheduling remains provisional. A session/source revision prevents late old-source packets escaping No Audio; reliable retry preserves FIFO.

Authenticated QUIC DATAGRAM carries audio inside the existing pinned mutualTLS1.3 connection. ALPNfriendpush/2 requires matched new builds. Audio controls have versioned strict fixed-format/capability/source/state/ready framing. Session/connection/generation/length/duration/sequence/age checks precede decode. The currently localhost-only clock uses the same-host QPC domain. Same-host endpoint hashes block digital recapture when an incoming/own loopback endpoint equals the selected playback endpoint. No OS default device, firewall, router or trust-store change.

## Evidence already observed
- Task1 initial behavior RED331 -> model/real Opus GREEN336; duplicate-after-consumption RED335 -> watermark fix.
- Native WASAPI RED337 -> full Release all-target/CTest2/2 GREEN341. PCM local299frames nativeUSB192k->48k, mean capture->encode17.17ms/max27.20ms. Opus299frames mean17.30/max28.63ms. Explicit ASUS48k->USB output499frames/10s,478560rendered frames, mean18.85/max28.71ms,0overruns. Same-device local cases stop capture before replay.
- Audio control/datagram RED343 -> GREEN344. Real held audio receive credits stay at6 while video's2 slots are full; release returns audio credits.
- Settings output selector/persistence/controller RED345 -> GREEN346/347.
- First actual app/independent-process candidate failed: synchronous StreamPriority SetParam under the callback mutex produced a cross-process shutdown deadlock. Bounded separate-process RED355 -> removal of those calls -> all Release targets/CTest3/3 GREEN357. Authentication/negative TLS checks retained.
- First real audio matrix:12/13 checks passed. USB440Hz amplitude100%=.0300399,50%=.0150205,0%=8.53e-7,muted=3.07e-7. Speaker endpoint=.0282864; priorUSB silent. Same-host guard,No Audio,source resume,LineIn activation/Opus and duplex playback passed. Strict unchanged video generation failed; raw evidence preserved under matrix-before-worker.
- Slow400ms native-device surrogate RED361 -> separate audio worker. Reliable retry FIFO RED364 -> corrected dispatch. Release all targets/CTest3/3 GREEN365 (3.86/7.11/6.49s); Debug all targets/CTest3/3 GREEN367 (4.08/7.96/6.88s).
- Second real output matrix after isolation: all12 audio functionality checks pass again; strict unchanged video generation remains FAIL on one consumer recovery around output change. Actual video stays connected/receiving and advances. This is not relabeled a full no-pause pass. The earlier video baseline already records occasional250ms consumer recovery and UI/native codec pauses.
- Current physical Release app SHA25672B0984DE71BB37499DA70FA1F301C9B1F422D54CB774D4B6C68650F104046BB. Two real portable folders/identities, UDP42801/42802; WGC owned moving fixtures and low-level WASAPI meter independently observe playback. Final soak, review, persistence-relaunch, pixel/Diagnostics checks, cleanup and exact CI pending.

## Rulings, costs and preserved limitations
R1 Existing linked isolated worktree reused. Cost: video/audio share feature branch history; this increment is reviewed from the exact baseline above.
R2 Approved amendment and explicit autonomous Native authorization permit implementation without another routine confirmation. No merge/release/publication gate is waived.
R3 Opus/static secure audio remains an evaluated localhost dependency. Cost: permanent codec/transport selection and DG001/006/008 still open.
R4 Advertised supplementary sdd helper and TDD reference resources could not be read. Equivalent native task briefs/ledger/evidence preserve core workflow; no claim those helpers ran.
R5 Retain owned portable identities/evidence and all sibling worktrees. Cost: ignored test data retained; only exact owned runtime processes are closed.
R6 Upstream Opus CMake sets MSVC_CLANG_CL to list false;BOOL, producing a GCC-only source flag on MSVC. A scoped build-property shim removes that flag while keeping SIMD dispatch. Disabling SSE4 alone was an observed failed attempt (upstream AVX declarations), not final configuration.
R7 ALPN2 is a deliberate matched-build boundary. Cost: prior ALPN1 evaluation builds do not interoperate.
R8 Unit video-state preservation drains preexisting fake500fps credits before audio configuration. Cost: unit test proves state separation; real video continuity is measured separately, including its strict FAIL.
R9 Remove synchronous stream-priority setters that invert callback-lock ordering. Cost: only application dispatch priority is claimed; congested-WAN scheduling/adaptation remains open.
R10 Separate COM audio thread and bounded handoffs added after physical switching exposed shared transport-worker device latency. Cost: one extra thread, finite additional queues and explicit stale-source cancellation.
R11 Real Line In endpoint starts/captures/encodes; no external cable/card signal is available for fidelity verification. No capture-card audio or unplug/default-device removal acceptance is claimed.
R12 Reference Windows19045 correctly disables process capture (documented API minimum20348). On newer OS the API capability may exist, but the process backend is explicitly unqualified and no application entry is advertised.
R13 Qualification harness defects (UI owner lifetime, shared stdout reading, pinned Process.Handle for trustworthy exit codes) were corrected and raw failures retained. No prior PASS file can satisfy the process regression: old result files are cleared and native exit codes checked.
R14 Strict unchanged-generation physical video checks remain separately visible; the accepted scope must distinguish functional audio/routing from full uninterrupted V1 video. Final review decides any consequential in-scope defect; remaining known recovery/performance work stays evidence-gated.

## Open acceptance
Five-minute simultaneous audio/video drift/resource observations and final review are pending. Physical endpoint removal/default switching, external capture-card signal, newer-OS process capture, cross-machine A/V synchronization, encrypted TCP comparison, WAN QoS/adaptive bitrate, self-LAN/DDNS/Internet/Tailscale and physical second-PC acceptance remain open. No final installer/release/main merge.

Primary references checked during implementation:
- https://opus-codec.org/downloads/
- https://opus-codec.org/license/
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants
- https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ne-audioclientactivationparams-audioclient_activation_type
