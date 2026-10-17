# Secure Dual-instance Video Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking. The user selected Native and explicitly waived further routine confirmation checkpoints on 2026-10-16 09:23:50 Asia/Bangkok.

**Goal:** Two portable FriendPushScreen instances on the reference Windows 10 PC exchange authenticated H.264 video over different real network ports, in both directions, with persisted independent codec choices and verified recovery.

**Architecture:** A pinned MsQuic/OpenSSL adapter supplies mutual certificate authentication, one ordered control channel and bounded independent access-unit streams. Versioned session messages drive selected MF H.264 sender/receiver pipelines and the existing D3D11 viewer. Peer identity, transport, protocol, media and application/UI coordination stay separately testable.

**Tech Stack:** C++20, Win32, D3D11, Media Foundation, Windows CNG/CryptoAPI/DPAPI, CMake/MSVC, MsQuic OpenSSL 2.6.3 evaluation candidate.

**Spec:** docs/superpowers/specs/2026-10-16-secure-dual-instance-video-design.md, approved by the user from commit 000c6a03164c1f1f15902eb3565ea357ab250589.

## Global Constraints

- Work in the existing isolated canonical worktree; do not reset/clean other work, main, tags or releases.
- Implementation remains Native; use one fresh independent whole-change reviewer after implementation, not implementer agents.
- No further routine approval gate: the user's explicit instruction overrides skill handoff prompts.
- Native baseline Windows 10 build 19045; no OS security registry workaround.
- No Audio is a valid intermediate state; do not claim optional audio, WAN/Tailscale or full V1 release is complete.
- Preserve route-independent protocol, explicit peer trust, no certificate-validation bypass, no 0-RTT and no machine trust-store changes.
- Pinned dependency URL/version/hash and actual loaded DLL/provider evidence precede permanent dependency selection.
- Control record ≤64 KiB; access unit ≤8 MiB; two outstanding media units and 16 MiB per direction, including partial/in-flight work.
- Coded edges ≤4096 and visible pixels ≤3840×2160; checked arithmetic and even coded dimensions.
- Preserve source selection, CPU/GPU policy, bitrate/GOP/encoder latency, real-time decoder mode, Settings overlay and full-frame aspect fit.
- One primary roadmap task ACTIVE at a time; prerequisite implementation/qualification and provisional transport decisions are recorded separately.
- Distinguish local-loop, same-host real-network, self-LAN and physical remote-peer evidence.
- Every implementation unit has a meaningful RED → GREEN cycle and the full relevant CTest suite after its changes.
- A security/compatibility failure selects the documented alternative; it never enables plaintext or a validation bypass.

## Review Focus

1. A copied or replaced identity must not be silently trusted; self, expired, wrong-SAN and changed-key credentials must fail before decoding. Covered by Task 1 and Task 6.
2. Simultaneous Connect, disconnect during callbacks and stale frames after restart must not create duplicate sessions, use freed state or show an old live picture. Covered by Task 3 and Task 6.
3. Queue limits include partially received and retained send buffers, not only completed messages; malformed sizes must fail before allocation. Covered by Task 1, Task 3 and Task 6.
4. Sender/receiver source independence: B with no capture source receives A, and Stop Share on B preserves reception. Covered by Task 4–6.
5. Non-primary/negative-coordinate monitors, portrait/odd sizes and moving viewer rectangles must preserve aspect-fit and feedback masking. Covered by Task 2, Task 4 and Task 6.

## File Map

| Area | Files | Responsibility |
|---|---|---|
| Dependency | cmake/msquic.cmake; third_party/notices/msquic-*; CMakeLists.txt | Pinned native package/header acquisition, runtime copy and notices |
| Identity | src/network/peer_identity.h/.cpp | Ephemeral native certificate generation, DPAPI identity persistence, public pairing import/export and exact pin |
| Transport | src/network/secure_peer_transport.h/.cpp | MsQuic handle/callback lifetime, authenticated connection and bounded message streams |
| Protocol | src/network/peer_protocol.h/.cpp; peer_session.h/.cpp | Explicit endian wire records, session state/generation, heartbeat/reconnect intent |
| Feedback | src/capture/feedback_protection.h/.cpp | Capture exclusion, coordinate clipping and pre-encode BGRA masking |
| Media | src/media/video_frame_normalizer.h/.cpp; video_peer_stream.h/.cpp | Shared padding/cropping plus independent outgoing encoder/incoming decoder |
| Coordinator | src/app/peer_video_controller.h/.cpp; application.h/.cpp | Capture/share/receive ownership, deferred events and renderer routing |
| UI | src/ui/ui_state.h/.cpp; ui_command.h; settings_panel.h/.cpp; main_window.cpp | Pairing actions, accurate connection/share/codec state and control behavior |
| Tests/probes | tests/peer_identity_tests.cpp; peer_protocol_tests.cpp; peer_session_tests.cpp; feedback_protection_tests.cpp; video_peer_stream_tests.cpp; tools/peer_probe/main.cpp | Real native security/transport tests, pure model tests and cross-process probe |
| Delivery | docs/development/reports; evidence; tasks/CURRENT.md; ROADMAP.md; architecture/DECISION-GATES.md; README.md | Actual results, limitations, reproducible manual two-folder instructions |

## Task 1: Pinned native secure transport and identity

**Files:** Create dependency/identity/transport/probe files from File Map and tests/peer_identity_tests.cpp; modify CMakeLists.txt and tests/test_main.cpp.

**Interfaces:**
- Produces PeerIdentity::LoadOrCreate(path,error), PublicBundle(), Fingerprint(), StableName(), PfxBytes(); immutable public certificate data and DPAPI-only persistent private identity.
- Produces ParsePeerBundle(span,error), SaveTrustedPeer(path,bundle,error), LoadTrustedPeer(path,error).
- Produces SecurePeerTransport::Start(identity,peer,port,error), Connect(host,port,error), SendControl(bytes), SendVideo(bytes), PollEvents(), Metrics(), Stop(); event kinds Connected, Disconnected, Control, Video, MediaLost, Error.
- All send ownership is explicit, every count/byte cap is enforced, callbacks never access UI/media objects.

- [x] Pin/download/inspect official native OpenSSL 2.6.3 package and matching headers; record archive/runtime SHA256, imports, signature/provider and notices. Configure only project-local dependencies.
  Expected: native x64 DLL loads on 19045; no Schannel-only path or missing import.
- [x] Write identity behavior tests before implementation: stable restart identity, different roots give different keys, public-only bounded bundle roundtrip, damaged DPAPI blob fails without replacement, malformed/oversized bundle rejection, own identity rejection.
- [x] Run compiled tests with minimal declared/unimplemented API bodies.
  Expected: named assertions fail because identity behavior is absent; compilation errors alone do not count as RED.
- [x] Implement standard native certificate/key generation and DPAPI persistence, strict public bundle format, no global certificate trust installation. Preserve normal validity, purpose and SAN checks.
- [x] Write and run actual secure-peer probe assertions before complete transport: known mutual identities exchange deterministic payloads; unknown/wrong pin/missing client/expired/wrong SAN fail; partial/truncated receive and two outstanding-send limits are exercised.
  Expected: missing transport fails the exchange assertion; there is no mock TLS success.
- [x] Implement OpenSSL validation flags, portable-DER synchronous pin rejection, explicit native-library loading, real UDP listener and connection, ordered length-prefixed control stream and separate bounded reliable AU streams. Stop closes handles only after callback ownership is safe; overflow/abort emits MediaLost.
- [x] Run full Debug/Release CTest and native secure-peer self-test, then a server/client probe in two OS processes with distinct folders/ports. Record startup, security negative tests, payload digests and endpoint owners.
  Expected: all tests pass, only known pinned peers exchange payload, queues stay within caps.
- [x] Commit dependency/identity/transport foundation. Record evaluation outcome and any provisional DG-001/DG-006/DG-008 conclusions; no WAN/video-completion claim.

## Task 2: Feedback protection

**Files:** src/capture/feedback_protection.h/.cpp; tests/feedback_protection_tests.cpp; main_window.cpp; CMakeLists.txt.

**Interfaces:**
- Produces ClipFeedbackMasks(capture_rect,window_rects,width,height) → bounded local pixel rectangles.
- Produces FeedbackProtection::ConfigureOwnedWindow(HWND), Apply(CapturedFrame&,CaptureSourceDescriptor,error).
- Consumes CapturedFrame BGRA CPU/GPU representation and actual monitor/window geometry.

- [x] Write RED tests: negative screen origins, clipping/off-screen rectangles, overlapping masks, portrait/odd sources, self-viewer window rejection and CPU black masking preserve unmasked pixels.
  Expected: absent masking fails the relevant pixel/geometry assertions.
- [x] Implement capture affinity on owned top-level windows plus checked fallback mask before encoding; preserve non-masked pixels and cursor policy, no wrong-monitor coordinate reuse.
- [x] Qualify real DDA/WGC exclusion and forced fallback against owned moving windows; verify masked GPU readback while preserving synthetic reference colors outside masks.
  Expected: viewer rectangles are absent/black, remaining content matches reference, card/window sources are not accidentally masked using unrelated monitor coordinates.
- [x] Run full CTest, retain physical evidence and commit. Mark FPS-007 only for the behavior actually qualified.

## Task 3: Versioned protocol and session lifecycle

**Files:** src/network/peer_protocol.h/.cpp; peer_session.h/.cpp; tests/peer_protocol_tests.cpp; peer_session_tests.cpp; CMakeLists.txt.

**Interfaces:**
- Produces EncodeControl/DecodeControl and EncodeVideo/DecodeVideo with checked explicit fixed-width network byte order; peer wire format and VideoWireFrame are defined in this header.
- VideoWireFrame includes stream ID, generation, sequence/timing, coded/visible dimensions, bounded optional cursor, H.264 AU and same-host measurement timestamp.
- Produces PeerSession::OnTransportEvent, RequestShare, StopShare, Disconnect, Tick; actions queue validated control/media/state changes.
- Consumes authenticated transport only; no parser or session may cause pre-auth media delivery.

- [x] Write RED tests for roundtrips, every short header, excessive sizes, overflow, unsupported versions/type/format, invalid geometry/cursor, duplicate/old generation and reserved fields.
  Expected: unimplemented codecs fail roundtrip/rejection assertions.
- [x] Implement strict serializer/parser and immutable validated records. Entire encoded wire media remains within the 8 MiB record budget, with control independently capped.
- [x] Write RED session tests: no-source receive, independent share intent, Stop Share versus Disconnect, stale stop/format/frame after reconnect, heartbeat loss, bounded reconnect backoff and simultaneous-connect tie-break.
- [x] Implement Hello/capabilities and stream state, keyframe requests, heartbeat, deterministic one-session selection and fresh generations. No Audio is advertised honestly; reconnect only while user intent remains.
- [x] Run full CTest and transport probe with fragmented control reception and malformed peer records.
  Expected: no unvalidated or stale payload reaches a media action; failed connection attempts are explicit and bounded.
- [x] Commit protocol/session implementation and qualified FPS-008 model results.

## Task 4: Independent network sender and receiver codec pipelines

**Files:** video_frame_normalizer.h/.cpp; video_peer_stream.h/.cpp; video_local_loop.h/.cpp; tests/video_peer_stream_tests.cpp; tools/peer_probe/main.cpp; CMakeLists.txt.

**Interfaces:**
- Produces VideoFrameNormalizer::Normalize/RestoreVisible using the existing loop's behavior.
- Produces VideoPeerSender::Initialize(device,config,policy), Push(frame,sink), Poll(sink), RequestKeyframe(), Stop(), Info()/metrics.
- Produces VideoPeerReceiver::Configure(device,format,decoderConfig,policy), Push(VideoWireFrame,sink), Poll(sink), Reset(), Stop(), Info()/metrics.
- Consumes Task 3 VideoWireFrame; no UI types. Sink ownership and dropped-frame behavior are explicit.
- Existing VideoLocalLoop remains the qualified local-only path and uses the shared normalizer.

- [x] Write RED tests with existing encoder/decoder interfaces for visible geometry, sequence/timing preservation, independently chosen policies, old generation rejection, bounded pending metadata and awaiting-IDR recovery.
- [x] Extract shared normalization/cropping without changing local-loop semantics; implement sender-only encoding and receiver-only decoding, real-time decoder unchanged.
- [x] Run native same encoded bitstream across processes, initially synthetic owned patterns, then real WGC capture. Validate encoded payload digest and decoded content separately.
  Expected: a receiving process with no capture source renders the sender's changing content and cannot advance frame count from a local preview.
- [x] Run CPU/GPU combinations, padded/portrait geometry and force-IDR recovery after an intentionally aborted AU. Report hardware performance separately from software functional tests.
- [x] Run full Debug/Release CTest and prior local-loop regression probes; commit media integration.

## Task 5: Real UI connection/sharing and pairing flow

**Files:** peer_video_controller.h/.cpp; application.h/.cpp; UI files from File Map; UI state tests; README.md.

**Interfaces:**
- Produces PeerVideoController state snapshot, identity export/import, Listen/Connect/Disconnect, StartShare/StopShare, PushCapture and Poll(incoming frame sink).
- Consumes Tasks 1–4. Application owns capture and renderer; controller owns network encoder and remote decoder.
- Local Preview intent and capture-for-sharing intent are distinct. One captured outgoing frame is encoded once; incoming rendering has priority and is explicitly labeled.
- Pairing export/import actions are deferred normal UI commands with native file dialogs and explicit full-fingerprint confirmation.

- [x] Write RED state/command tests for actual connected/sharing/error statuses, no-source receiver, local preview independence, Stop Share preserving receive, Connect cancellation and codec-setting updates.
- [x] Implement controller; add Export Identity/Pair with Friend controls and public fingerprint/peer status. Persist trust separately from ordinary INI settings.
- [x] Replace inert Share/Connect request handlers, poll network without requiring a capture source, and wake idle UI on network activity. Keep network callbacks away from WGC/UI/codec calls.
- [x] Route outgoing source capture, optional explicit local preview and incoming frames correctly; preserve source selection on settings changes and clear stale remote output on stop/loss.
- [x] Add accurate codec, connection, stream/byte/FPS/queue/error diagnostics; remove stale transport-not-implemented text from actual runtime UI. No Auto connection mode.
- [x] Exercise real file pairing/confirmation for the two known owned copies and switch all four codec pairs through the UI while connected.
  Expected: both directions work with independent settings; user-facing state corresponds to actual transport/decoder state.
- [x] Run full CTest and app regressions; commit usable UI integration and manual two-folder instructions.

## Task 6: Cross-process qualification, faults and final review

**Files:** tools/peer_probe/main.cpp as needed for genuine missing checks; docs reports/evidence/tasks/decision gates/README.

**Interfaces:** Consumes the complete application and previous probe APIs; produces exact-source reports, binary/runtime identities and reproducible tests, not new product behavior unrelated to qualification.

- [x] Run two portable copies from isolated folders with distinct keys/settings/ports. Confirm socket owners and encrypted/pinned connection before measuring.
- [x] Prove A → B with B having no capture source, B → A, and simultaneous distinct owned patterns; record changing content and actual incoming counters.
- [x] Run five-minute hardware 1080p60 tests per direction and simultaneous sharing. Aim ≥59 rendered fps each, report actual fps, p50/p95/p99 stage latency, bitrate, CPU/GPU/memory, queue count/bytes/age and drops honestly.
- [x] Test Stop Share/Disconnect, source switch/loss, server/client restart, trust rejection/replacement/expiry, unavailable/duplicate ports, stalled receiver, malformed records and repeated shutdown. Write reproducing RED tests for any defect before fixing.
- [ ] Compare same encoded workload with the TLS/TCP reference and, if candidate evidence warrants it, the WebRTC alternative. Include interleaved control and scheduled synthetic audio load; retain DG-001 as provisional if full decision inputs remain outstanding.
- [x] Qualify self-LAN only after secure localhost tests; label all untested Internet/Tailscale/physical peer paths. No firewall/router change is needed for this local acceptance.
- [x] Run final Debug/Release build/CTest, full source regression and exact-commit CI. Prepare one fresh whole-change review with spec, plan, diff, ledger rulings and Review Focus verbatim. Re-grade findings; fix Critical/Important with RED→GREEN and the whole suite.
- [x] Preserve reports/evidence, update roadmap states only to demonstrated outcomes, commit/push exact final source and documentation and confirm both CI jobs/clean state. Do not merge main or release without a separate instruction.

## Qualification rulings recorded at delivery

Tasks 1-5 are implemented and verified. Checked qualification steps mean the tests were executed and their results preserved, including failed strict generation-continuity runs; they do not turn every FPS roadmap item GREEN.

The same-workload encrypted TCP/audio-load comparison and self-LAN/remote routes remain unchecked. The approved spec explicitly permits a provisional development-branch integration while the full DG-001 inputs are outstanding. This increment remains loopback-only No Audio. Physical negative-origin/card networking, full V1 audio and Internet results are not inferred from models or localhost.

The single fresh whole-change reviewer and one grouped Important fix pass completed. The later user-supplied fixture and Diagnostics defects received their own physical/compiled RED-to-GREEN checks. The native ledger and dated report retain failed harness attempts, source/binary identities, review re-grading and final CI.

## Plan self-review and authority

Every approved-spec section maps to Tasks 1–6 above. Cross-task interfaces are named and owned; file setup/documentation is folded into the task it serves. Existing source/build baseline CTest passed before implementation. Source-side media and cryptographic behavior is verified by real runtime probes in addition to model tests.

The user explicitly approved the spec and authorized plan preparation plus continuous Native implementation without further confirmation. Do not reintroduce the writing-plans review pause. Routine choices/rulings go to the ledger and must preserve the spec; irreversible/destructive/out-of-scope actions remain outside this authorization.

## 2026-10-17 route/release continuation

The later native-routes-pairing report supplies actual self-LAN and public-name DDNS NAT-loopback/own-Tailscale-IP evidence on8996/8997, plus the user-requested one saved-device Delete. Earlier loopback-only statements above describe the original increment. Same encoded TLS/TCP/control/audio-load comparison remains unchecked. The user requested a v1.0.0 release of the documented current feature boundary, with friendpushscreen-v1.0.0-portable.zip; no physical WAN or full V1 acceptance is inferred.
