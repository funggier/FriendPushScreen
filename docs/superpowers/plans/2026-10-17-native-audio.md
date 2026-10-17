# Native WASAPI / Opus audio implementation plan
> Implement inline with executing-plans. Native selected; no routine confirmation pause. Spec: docs/superpowers/specs/2026-10-17-native-audio-design.md.
Goal: selectable real shared audio with local receiver controls and secure localhost duplex qualification.
Architecture: device/codec/audio timing in worker, independent audio generations and budgets, existing video controller as session adapter.
Tech: C++20, WASAPI shared mode, Opus1.6.1 static, existing MsQuic2.6.3.
BASE: 1f86e54420e5b5a245e7e509bde3902eb5f02bf3.

## Global constraints
Native parent implementation; preserve current isolated worktree and all user work. No main merge/release. No crypto/trust bypass, firewall/router/system-default changes, scratch implementation, private identity export, or unbounded queues. Audio changes must not reset H.264. Report physical, localhost, fixture and unavailable cases accurately. Close owned test processes. Use named behavior tests first, observe RED then GREEN, run full Release CTest each task; final Debug+Release all targets/CTest. Commit each completed task plus ledger. One final fresh whole-change review; one consequential fix pass, record all deferrals.

## Review Focus
1. Stale/forged session, generation and malformed Opus records must never reach playback (AudioWireRejectsInvalidAndStale).
2. Reorder, missing frames, duplicate floods and a 2-second stall must remain bounded and resume recent audio (AudioJitterBoundsReordersAndRebases).
3. No Audio and switching endpoints must release capture/send while video generation and session stay unchanged (AudioSourceSwitchLeavesVideoRunning).
4. Same-PC render recapture, output/default-device loss and mute0/50/100 must not produce feedback or resume on an unsafe endpoint (AudioRoutingGuardAndGain).
5. Audio congestion must not consume video's two-unit credit or grow retained datagram contexts (AudioDatagramBudgetIndependent).

## File map / interfaces
Create src/audio/audio_model.{h,cpp}: source descriptor, AudioPacket, fixed Opus format, wire encoder/decoder, generation-aware jitter, gain/routing rules.
Create src/audio/opus_codec.{h,cpp}: RAII Opus encoder/decoder, 960 stereo float frames, encode <=900, PLC bounded20ms, reject unexpected packet duration/channels.
Create cmake/opus.cmake + third-party/opus-notice.txt: official1.6.1 archive SHA6ffcb593207be92584df15b32466ed64bbec99109f007c82205f0194572411a1, no optional ML model.
Create src/audio/wasapi_audio.{h,cpp}: active endpoint catalog; IAudioSource Start/Stop/Poll; shared render48k float, padding-bounded write; lost/default changes.
Create src/audio/peer_audio.{h,cpp}: worker-side capability/format/ready/state machine, independent generations, codec+jitter+render, source/mute/volume/output selection, snapshots.
Modify network secure_peer_transport.*: Audio event/SendAudio, authenticated datagrams <=1024, independent6 count lifetime credits and metrics.
Modify app peer_video_controller.*: audio configuration/snapshot; worker session synchronization, audio event interception and priority scheduling.
Modify app application.*, ui ui_state/settings_store/settings_panel/ui_command: real catalog source IDs and receiver endpoint, persistent preferences, functional mute/volume, compact status+diagnostics.
Create tests/audio_tests.cpp, tests/peer_audio_tests.cpp, tools/audio_probe/main.cpp and registered CMake targets. Expand native peer probe to cover datagrams if suitable.

## Task 1 - Audio model and real codec
- [x] Record BASE/brief and failing AudioWireRejectsInvalidAndStale, AudioJitterBoundsReordersAndRebases, AudioRoutingGuardAndGain, OpusRoundTripAndPlc. Test source removal/generation and malformed packet boundaries, actual low-level codec roundtrip/correlation.
- [x] Minimal implementation, pin dependency/retain notices; no UI types. Build all Release, full CTest; record and commit.

## Task 2 - Native WASAPI and local probe
- [x] Record BASE/brief. Test invalid IDs, No Audio lifecycle, safe output gain, format bounds before implementation. Runtime probe --inventory, --self-test owned tones with explicit endpoints, PCM/Opus roundtrip stats.
- [x] Event-driven shared48k stereo float capture/render on worker, device loss/default detection, no OS mutation. Run full CTest and actual endpoint probe; commit evidence.

## Task 3 - Secure audio and application controls
- [x] Record BASE/brief. RED peer tests for AudioDatagramBudgetIndependent and AudioSourceSwitchLeavesVideoRunning plus late-ready, no capability, session restart, source loss and receiver guard.
- [x] Add datagrams with consumer-held6-count credits, versioned strict audio controls+receiver-ready barrier, independent source restart. Queue control before audio before video. Integrate real source/output lists, gain settings and diagnostics without periodic list rebuild/flicker.
- [x] Full Release build/CTest plus native secure peer audio tests. Commit.

## Task 4 - Real dual application qualification and review
- [x] Deploy built app to two owned portable folders (distinct identities/ports); exercise real video+audio tone routing, both directions, mute/volume/source/No Audio/disconnect/source loss. Capture counters, bounded queues/latency/drift/CPU; distinguish unavailable hardware.
- [x] Full Debug+Release all targets/CTest; one fresh review against spec/plan/BASE..HEAD and Review Focus above. TDD one consequential fix pass, rerun affected checks/full suite.
- [x] Update CURRENT/ROADMAP/qualification matrix/report and ledger including exhaustive rulings/costs/deferrals. Preserve prior strict video FAILs. Push current branch and verify exact-head Debug/Release CI. Close owned test applications and verify. No main merge.

Completion scope: local/native and authenticated two-instance localhost audio/output subgate passed; see dated report and curated JSON. Physical unplug/default changes, external signal fidelity, process backend on newerOS and remote/WAN/V1 acceptance stay open. One review and one consequential fix pass completed; no second review. Source9d24 finalDebug/Release3/3 and exact sourceCI successful; documentation-head CI is verified after push. Worktree preserved and owned runtime closed.