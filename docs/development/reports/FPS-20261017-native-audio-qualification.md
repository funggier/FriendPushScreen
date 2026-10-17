# Native audio qualification — 2026-10-17
Status: SCOPED PASS — Native audio and selectable receiver output qualified on this localhost reference machine. This is not full V1 release acceptance.
Baseline: 1f86e54420e5b5a245e7e509bde3902eb5f02bf3. Branch: fps-006-encode-decode-local-loopback. Native parent implementation; main and sibling worktrees preserved.
User added an explicit Settings playback-device selector request at05:13:14 Asia/Bangkok and required steady audible output at06:12:52. Source candidate9d24f9ed61470ad2ab4fd7a8b5beef8fecffb381; Release app SHA256B1CC600361EAAF1783E9DE4AC66CEC09951544C5DB2E71CBF92F1B7D4A57E7BC.

## Implemented behavior
Audio Source selects No Audio, default System mix, an explicit output loopback or active input endpoint. Remote Audio Output selects Default Output Device or an explicit active render endpoint, retaining its stable ID. Mute and volume are local and persisted. Source changes create an independent audio generation; No Audio releases source/encoder and stops audio packets. Existing Share/Stop/Connect, video codec choices, painting fixes and separate trusted identities remain.

WASAPI requests shared48kHz stereo float with Windows conversion; Opus1.6.1 uses20ms frames, constrained VBR128kbit/s, packet <=900bytes. Official archive SHA2566ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1. Static Opus BSD notice is copied beside the portable app; project MIT license remains.

Audio device/codec operations run on their own COM-MTA worker. Device opens cannot hold the UI/transport queue mutex. Handoffs bound audio to6 datagrams per queue and controls to16; transport holds6 receive leases including consumer-held frames, jitter holds6 frames/120ms, renderer PCM <=2880frames/60ms. Video retains its separate2-unit/16MiB budget. Control dispatch precedes audio then video; MsQuic default wire scheduling remains provisional. A session/source revision prevents late old-source packets escaping No Audio; reliable controls keep their finite queue slot until accepted. Pending state is admitted without loss; inbound capability/latest source-format-state/newest Ready coalesce into at most five control kinds. Preferences save immutable snapshots on a separate serial writer and flush the latest revision on shutdown.

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
- Reviewed pre-fix candidate6b74, app SHA25672B0984DE71BB37499DA70FA1F301C9B1F422D54CB774D4B6C68650F104046BB: physical soak370 failed steady audio. During239seconds with the tone still active, A accumulated3687PLC/drop pairs while BPLC and underflow deltas stayed0. Whole299-second averages42.46fps/4498underflows also included47seconds after the tone fixture expired (CLI cap600 despite argument900); those aggregate values do not prove a capture cadence failure.
- One fresh whole-change review found four Important issues: jitter lost40ms lead after backlog eviction, expired queued audio survived a stall, reliable controls could disappear on saturation/concurrent retry, and preference disk I/O blocked the UI. Parent accepted all four for one consequential fix pass; no second review. RED371/372/373/374 reproduced each behavior before changes.
- GREEN376/377: all Release targets and full CTest3/3PASS; final377 durations3.77/7.13/5.72seconds. Debug378 all targets/CTest3/3PASS,5.54/7.09/6.60seconds. New tests cover +/-1ms arrivals after overflow,2-second stall without fresh packets, latestDisabled/Ready after40 source changes, concurrent queue refill,160-control inbound burst and100ms injected preference writes with final ordered flush. Existing TLS, independent-process shutdown, datagram lifetime and opaque Diagnostics paint checks pass.
- Actual apps A/B run from separate owned portable directories with distinct identities, UDP42801/42802, matched Release9d24 source and WGC moving-window fixtures. Matrix381 preserved12/13 checks: strict video generation passed, but a coherent440Hz amplitude ratio failed although RMS50/100=.5014. Sampling phase changes affect the long-window sinusoid projection; RMS is the direct gain measure. Corrected meter criterion without product changes; matrix382 observed13/13PASS, including RMS gain, mute/zero silence, USB->speaker routing/old-output silence, same-PC guard, NoAudio/source resume, LineIn activation and duplex output/video-generation preservation.
- Physical helper completion caveat:381/382 saved their full result before entering a redundant final restoration block that did not exit promptly; only those exact owned PowerShell processes were stopped after observing current source/output/gain state.13/13 is per-case readback evidence, not a fabricated clean helper exit. Final soak is separate and does not use that restoration block.

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
Five-minute steady audio/video, physical output continuity, the single whole-change review/fix pass and preference relaunch checks are complete as recorded below. Physical endpoint removal/default switching, external capture-card signal, newer-OS process capture, cross-machine A/V synchronization, encrypted TCP comparison, WAN QoS/adaptive bitrate, self-LAN/DDNS/Internet/Tailscale and physical second-PC acceptance remain open. No final installer/release/main merge.

Primary references checked during implementation:
- https://opus-codec.org/downloads/
- https://opus-codec.org/license/
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording
- https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants
- https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ne-audioclientactivationparams-audioclient_activation_type

## Accepted review fixes and remaining judgments
R15 Reliable outgoing control slots remain reserved until send accepts; the engine retains unadmitted controls and coalesces the newest Ready. Incoming source/format/state stays a complete latest-generation transaction, independently preserving capability/Ready. Cost: superseded generations are intentionally coalesced; queues remain finite.
R16 Backlog overflow keeps recent audio and reestablishes40ms lead. A stalled consumer discards pre-stall packets, preserving the replay watermark; the decoder rechecks250ms freshness. Cost: old buffered content is dropped instead of replayed. No claim that the source-derived reproduction exactly reconstructs the earlier physical trace.
R17 Preference writes use immutable coalesced snapshots on a serial thread; latest revision flushes on shutdown with visible final-save failure logging. Cost: a crash before flush may lose the newest preference, and slow disk can delay shutdown. This removes a proven blocking call chain; attribution of the earlier297ms video pause remains unproven.
R18 The jitter regression initially reached beyond the last generated input frame and correctly received end-of-stream PLC. Extended the generated stream beyond the observation window, retaining the steady-input zero-PLC assertion. The native tone fixture is now explicitly600seconds and encloses the entire final300-second observation.
R19 Physical gain uses phase-insensitive PCM RMS, with the original amplitude-based negative retained. Steady output additionally counts480-sample10ms blocks belowRMS.001 and longest silent run after the initial500ms; it does not infer perceptual quality solely from packet counters.

No Minor review items were returned. Deferred/unqualified judgments remain explicit: physical unplug/default-device change; external capture-card/LineIn signal fidelity; newer-OS process backend; cross-machine A/V sync; LAN/Internet/DDNS/Tailscale; encrypted-TCP comparison; WAN adaptation/QoS; permanent codec/transport decisions; installer/fullV1 acceptance. Retained existing video strictFAIL history. Authentication, endpoint selection and finite budgets are not bypassed to obtain a pass.
## Final measured qualification
Qualified source:9d24f9ed61470ad2ab4fd7a8b5beef8fecffb381. [Source CI](https://github.com/funggier/FriendPushScreen/actions/runs/38981368361): Windows x64 Debug and Release SUCCESS. Final documentation/evidence commit is checked separately after push; no product source changed after this qualified source.

Environment: Windows10Pro19045 x64, CoreUltra5 245K integrated graphics,14 logical CPUs,32GB. Two real portable apps/identities on localhost42801/42802, 1080p60 owned WGC moving-window fixtures, ASUS48k loopback tone and USB192k default render conversion. No physical second computer is available.

Final simultaneous AV observation:300.487seconds wall time,299seconds between logged counter endpoints,06:28:07–06:33:08 Asia/Bangkok. The600-second owned tone enclosed the entire observation. Both audio sources and playback stayed active; no audio/video generation changed.

| Measured interval result | A | B |
|---|---:|---:|
| Video send/render fps |59.84/59.83|59.83/59.84|
| Audio send/decode20ms frames per second |50/50|50/50|
| Audio drops / PLC / render underflows / overruns |0/0/0/0|0/0/0/0|
| Video recoveries / transport drops |0/0|0/0|
| PCM frames played |14,352,000|14,352,000|
| Sampled jitter peak / queue bound |1/6|2/6|
| Renderer historical queue peak / bound,frames |1920/2880|1920/2880|
| Latest-packet age p50/p95/max,ms |41.90/43.62/49.10|51.79/57.71/59.45|
| Age start/end20-sample means,ms |42.137/42.170|52.229/52.479|
| Mean process CPU,total machine |1.275%|1.328%|
| Peak working set,MiB |243.25|242.67|

A separate300-second native loopback meter observed the actual combined receiver output:14,399,009captured PCM frames,29,947measured10ms blocks after the initial500ms,0blocks belowRMS.001, longest silent run0ms, blockRMS range.0025402–.00275775. Both apps mixed the same440Hz fixture into one endpoint; the combined amplitude is phase-dependent and is not a gain test. Isolated receiver gain/routing is qualified by matrix382's13/13cases and physical RMS50/100 ratio. Packet-age metrics use the same-host capture QPC and measure decode/queue age; they are not an acoustic DAC latency or cross-machine A/V-sync claim.

Persistence/relaunch: chose distinct Speakers/USB endpoint IDs in A/B, muted both and set37%/63%; graceful close flushed preferences. Reopening showed the correct endpoint captions, Unmute state and respective volume, with Start Share (no auto-share). Default Output Device was then selected and persisted as an empty endpoint ID. Original owned test preferences were restored after qualification; identities/trust were preserved.

Visual QA: actual Settings output selector, status, mute/volume and scroll layout inspected. Initial screenshots under normal capture exclusion correctly omitted the app, so their paint counts were not accepted as visual evidence. Relaunched only owned A with the existing diagnostic feedback-mask fallback and inspected its visible UI. Four native Diagnostics internal-scroll rounds matched forced clean repaint with0stale pixels each. Ordinary capture exclusion remains enabled outside this diagnostic fallback.

Cleanup verified at10:19 Asia/Bangkok:0owned app, fixture or audio helper processes remain. The completed matrix helper caveat above remains recorded. The linked worktree, private portable identities, ignored raw evidence and sibling worktrees are retained. No main merge, tag, installer or release.

Machine-readable curated evidence: [FPS-20261017-native-audio.json](../evidence/FPS-20261017-native-audio.json). Raw failure/qualification results, task briefs, progress ledger and the single fresh review remain in the ignored task workspace.

R20 Finish by retaining the existing named branch/worktree and pushing the authorized evidence, per the approved plan; no repeated integration-menu approval is needed. Cost: this evaluation branch remains separate from main. The next transport/remote/recovery gates are explicit future work, not represented as completed by this audio subgate.