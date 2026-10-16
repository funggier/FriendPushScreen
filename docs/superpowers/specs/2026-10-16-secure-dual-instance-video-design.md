# Secure dual-instance video: design for the next network milestone

**Status:** APPROVED DESIGN — user approval 2026-10-16 09:23:50 Asia/Bangkok; Native implementation authorized continuously, without further routine confirmation. This is not a network qualification.
**Date:** 2026-10-16 (Asia/Bangkok)
**Source baseline:** 23038b26675a991b34da309c2daee8dbbcaf3c05
**Execution workspace:** existing canonical linked worktree, fps-006-encode-decode-local-loopback.

## 1. Goal and authority

เป้าหมายตามคำขอ: เปิด FriendPushScreen สองชุดจากคนละโฟลเดอร์บนเครื่องเดียว ตั้ง Share Port คนละหมายเลข แล้วส่ง–รับวิดีโอผ่านพอร์ตจริงได้ทั้ง A → B และ B → A หากส่วนใดยังไม่ครบให้พัฒนาต่อ

The approved [V1 amendment](2026-10-15-v1-audio-connectivity-amendment.md) already requires this development strategy. Reuse the qualified capture/MF H.264/D3D11 foundation and independent encoder/decoder controls. The new work adds the absent peer/session/media transport rather than relabeling a local codec loop as a connection.

This is an architectural change: no networking subsystem exists at the baseline. The user reviewed and approved this written design from commit 000c6a03164c1f1f15902eb3565ea357ab250589, selected Native, and explicitly authorized plan preparation and implementation without further confirmation. Preserve that authorization rather than repeating the handoff gate.

DG-001, DG-006 and DG-008 remain open until their required evidence exists. Existing FPS-006 results remain GREEN for their original local-loop scope and source identities.

## 2. Measured starting point

Actual same-binary portable copies were launched on CDQ-P / Windows 10 build 19045:

| Copy | Own Share Port | Remote endpoint | Data directory |
|---|---:|---|---|
| A | 42801 | 127.0.0.1:42802 | peer-a/data |
| B | 42802 | 127.0.0.1:42801 | peer-b/data |

Both binaries SHA256: 7F0FAC0675184092A0ACEAC69397ED8D8C40AEE25DC6C216757CC4007F40CA81.

Start Share and Connect only produced Requested state. Both logs explicitly report inactive media/transport; TCP/UDP ownership checks and PID-filtered netstat found no application endpoints. A separately started real WGC/H.264 GPU local preview; B still had no received picture or source. Result: **BLOCKED_NOT_IMPLEMENTED**, not a network test pass. See the [baseline report](../../development/reports/FPS-20261016-dual-instance-localhost-baseline.md).

## 3. Scope of the next usable increment

Deliver actual authenticated H.264 video with No Audio, first one way and then both ways, using two independent instances. No Audio is an existing valid V1 state, not a removal of eventual optional audio requirements.

Required behavior:

- separate portable settings, log and identity stores;
- one application protocol across localhost/LAN/Direct IP-DDNS/Tailscale;
- secure peer authentication and explicit first-trust authorization;
- real encoded access units across OS network endpoints, decoded only at the receiving copy;
- independent local sharing, remote viewing and local preview states;
- existing CPU/GPU choices apply separately to outgoing encoding and incoming decoding;
- Stop Share, Disconnect, source loss/change, port errors and restart have observable correct outcomes;
- bounded queues and recovery through a new random-access frame;
- FPS-005/FPS-006 viewer, Settings, hotkeys and codec controls remain usable.

Audio capture/Opus, final WAN/NAT/Tailscale acceptance, adaptation, installer and full V1 release retain their roadmap owners and are not declared complete by this increment. Do not add plaintext Internet transport, custom cryptographic primitives or remote input control.

## 4. Transport candidates and recommendation

| Candidate | Benefit | Constraint and decision |
|---|---|---|
| MsQuic + OpenSSL | Native C API; QUIC encryption and independent streams fit the configurable UDP-port target | Recommended first isolated evaluation; exact Windows 10 load, certificate validation and video behavior must pass |
| libdatachannel + libjuice + OpenSSL | Existing WebRTC H.264 framing, DTLS-SRTP and data channels | Alternative evaluation; authenticate signaling and measure the complete port footprint, including pairing/signaling |
| TLS 1.2/TCP using Schannel | Native Windows 10 encrypted correctness/throughput reference | Benchmark reference only; cannot silently replace the approved UDP-port target |

MsQuic's default Windows Schannel path needs TLS 1.3 support absent from build 19045. Its documented OpenSSL build removes that OS TLS dependency. Do not change OS security registry settings to make the default path appear supported.

Candidate package selection is pinned and rechecked at execution, with native DLL/provider/import/version/hash evidence. Current source-build instructions demand a newer SDK than this machine's installed 10.0.19041.0, so evaluate the official native OpenSSL package first; merely finding a NuGet package is not proof that its native dependencies load. MsQuic upstream is MIT; the OpenSSL package declares MIT AND Apache-2.0. libdatachannel is MPL-2.0; record the actual dependency graph and notices before any distribution decision.

**Decision rule:** no permanent transport selection from documentation alone. First prove native startup and authentication; then compare identical encoded access units and settings for latency, bounded buffering, resource use, recovery and actual port usage, including interleaved control/video and a scheduled synthetic audio-channel load in the transport probe. Synthetic load is not audio capture/playback qualification. Record an evidence-backed DG-001 decision before permanent application integration. A development-branch integration can remain explicitly provisional while any full DG-001 inputs are outstanding; never label its untested audio/Internet paths GREEN. If the first candidate cannot pass its security/compatibility gate, evaluate the alternative; an exception to the public UDP-port target requires its explicit architecture decision.

## 5. Components and ownership

| Unit | Responsibility and boundary |
|---|---|
| Peer identity/trust store | Local certificate/private-key lifecycle, imported peer public identity and expected pin; no UI or video types |
| Secure transport adapter | Listen/connect/send/receive/cancel, authenticated peer identity, bounded immutable byte buffers and metrics; no capture or Win32 control types |
| Versioned control session | Capabilities, authorization-ready state, share/stop/format generation, heartbeat, keyframe requests and reconnect |
| Outgoing video stream | Qualified capture/normalization/clock/selected encoder to encoded access units; preserve original timing and frame metadata |
| Incoming video stream | Validate format/generation/sequence, bounded queue, selected decoder, decoded D3D11 frame handoff |
| Application coordinator | Deferred user commands, sharing/receiving/preview ownership and UI snapshots |
| Feedback protection | Exclude viewer/settings windows where supported and qualified black-mask fallback before monitor frames are encoded |

Use the existing IVideoEncoder/IVideoDecoder interfaces; do not move UI preferences into the transport core. Preserve VideoLocalLoop as the local diagnostic path. Share the capture/normalization/encoding building blocks without encoding the same outgoing source twice. Network callbacks enqueue bounded work and wake the coordinator; they must not call Win32 UI, capture APIs or the native codec from the transport callback.

## 6. Trust and pairing

Use mature transport cryptography with mutually authenticated persistent identities. Disable 0-RTT initially. No application control or media is accepted before the expected peer is authorized.

The first proposed UX is explicit public-identity file pairing: export a bounded public certificate bundle; import the friend's bundle, display its full SHA256 fingerprint, and require explicit confirmation against the independently received fingerprint. The file contains no private key or password. Each side authorizes the other before connecting. This avoids assuming that an unimplemented in-band pairing protocol is already authenticated.

Private keys remain in the application's own data store protected with Windows DPAPI and suitable file access. Do not change the machine certificate trust store. Reject a copy of one's own identity, changed/unexpected peer key, expired/unusable credentials and malformed import. Never replace an established pin silently.

The MsQuic/OpenSSL evaluation must prove its actual certificate callback/pinning path for both client and server. The checked v2.6.3 provider supports a concrete evaluation path: select USE_TLS_BUILTIN_CERTIFICATE_VALIDATION on Windows, load the imported public identity using SET_CA_CERTIFICATE_FILE, request INDICATE_CERTIFICATE_RECEIVED and USE_PORTABLE_CERTIFICATES, supply both roles' own credentials, and require client authentication on the listener. The CA-file configuration adds to default trust locations; synchronously reject any actual DER leaf whose SHA256 differs from the selected imported peer. Do not assume Schannel-only deferred validation works with OpenSSL or ship a validation bypass. If this cannot be proven on the pinned native build, that candidate fails the gate.

Use a standard self-signed identity certificate with digitalSignature, clientAuth/serverAuth and a stable DNS SAN. The imported bundle records that SAN and public DER certificate; network Host/Remote Port is resolved separately. Keep normal certificate validity/purpose/name validation while routing to the resolved endpoint; do not add localhost/LAN/Tailscale addresses as unchecked alternative identities. Full-certificate pinning means renewal explicitly requires re-pairing. These are API/source feasibility findings, not proof of a working Windows 10 handshake.

Copies A/B start with empty separate identity stores; copying a previously paired entire data directory is not the test setup. Pairing acceptance in automation may authorize only the two known test-created public identities, never an arbitrary incoming peer.

## 7. Endpoints and connection lifecycle

Share Port is the instance's listening port; Host/Remote Port identifies the peer. One authenticated connection can carry both video directions, so users need not create two opposing connections. Simultaneous Connect requests must converge deterministically to one session, without duplicate video or oscillation.

Use explicit distinct ports for same-host copies. MsQuic warns that same-port listeners in separate processes can receive undefined delivery, so do not rely on a second listener failing automatically. Reserve/detect same-host application port conflicts and report them visibly.

Start Share with a valid source starts the local listener and waits for an authorized peer before network encoding; Connect starts the local listener and attempts the configured remote endpoint. Neither action requires the other side to share before it can receive. Stop Share preserves an established connection/listener; Disconnect cancels listening, connection/reconnect and outgoing network sharing while preserving source selection and portable preferences. The evaluation listener accepts only the requested loopback development endpoints until its security gate passes. Qualifying additional routes is a separate evidenced stage of the same protocol. Starting the app does not silently open an Internet-facing listener or modify a firewall/router.

Lifecycle: Offline → Listening/Connecting → Authenticating → Connected; sharing is independent. Failure moves to a concrete Error/Disconnected state. A trusted prior session may reconnect with bounded backoff while the user's connection intent remains active. Explicit Disconnect cancels reconnect; Stop Share stops only that outgoing media stream. Never resend buffered old-session frames after reconnect.

## 8. Protocol and media semantics

Negotiate a versioned control protocol and actual H.264 encode/decode capabilities before stream start. Advertise Audio=Disabled/Unavailable honestly for this increment and reserve typed audio capability/state fields for FPS-006A.

Control includes Hello/Capabilities, ShareStart, VideoFormat, ShareStop, KeyframeRequest, Ping/Pong and explicit Error/Goodbye. Each record has a checked type, version and bounded length. Reject unknown mandatory versions/types, truncated/oversized fields, integer overflow and unsupported formats before allocation/decoder submission.

A video record identifies session, stream, format generation, monotonic sequence, presentation time/duration, padded and visible dimensions, random-access status and encoded byte length. No raw pointers, native struct layout or GPU handles cross the process boundary. Serialize explicit fixed-width fields in a documented byte order.

Initial limits: control record ≤64 KiB; access unit ≤8 MiB; at most two queued access units and 16 MiB queued bytes per direction. Limits apply before allocation, including in-flight and partially received work. Initial advertised dimensions are even-coded sizes with each edge ≤4096 and visible pixel count ≤3840×2160; reject larger sources with an explicit capability error instead of silently allocating unbounded textures. Unit tests cover portrait and padded source geometry.

Evaluate a reliable control stream and independent cancelable access-unit streams first. No single unbounded reliable video byte stream. Compare a datagram path only with negotiated datagram-size enforcement and bounded fragmentation/reassembly. Expire incomplete or stale work; do not decode partial access units. On dependency loss, discard dependent frames, request/await a fresh IDR with required parameter sets and restart the decoder at the new generation.

A format/source change invalidates old-generation buffered media. A codec/bitrate change affects only the required outgoing or incoming stream; it does not clear the selected source or peer trust.

## 9. Viewer and feedback behavior

The main viewer shows incoming peer video when receiving; local Preview is an explicit separate state and is labeled as such. A copy with no local capture source must still receive. Do not claim reception using one's own capture. Settings remains an overlay and preserves full-frame aspect fit.

Start Share validates the selected source and opens the outgoing stream. Stop Share/peer disconnect/source loss clears or marks the remote image according to the explicit state; no indefinitely frozen picture suggesting live reception. Receiving continues when the user stops only their own sharing.

Before monitor two-way sharing, qualify exclusion of both application viewers and settings where supported. Otherwise apply a checked monitor-coordinate/DPI-aware black mask before encoding. Self-viewer window capture is rejected with a clear reason. Use owned synthetic windows for initial tests, then qualify monitor/window/capture-card behavior separately.

## 10. Failure and lifetime handling

Bound connect/resolve attempts, heartbeat detection, transport queues and shutdown. Keep network operations off the UI thread; post deferred state changes to avoid the already observed WGC synchronous-callback failure.

Test unavailable endpoint, duplicate port, peer close/restart, receiver stall, bad/truncated input, format change, missing codec and declined/changed trust. Stop/Disconnect invalidates the session generation, cancels transport work, releases owned resources and prevents callbacks reaching destroyed UI objects.

Keep the decoder's qualified real-time low-latency mode. The earlier synchronous native-MFT buffered-mode hang is a known limitation, not fixed by a socket timeout; do not claim a watchdog can forcibly cancel an arbitrary native codec call. Retain any new native stall as failed qualification evidence and resolve it before claiming that affected path.

## 11. Development sequence and roadmap

This design reprioritizes the requested No Audio network video increment; it does not renumber or close unrelated tasks.

1. Retain FPS-006 baseline and this failed network attempt.
2. Review this written design, then write/review the implementation plan and select execution method.
3. Perform isolated secure-transport evaluation and record the DG-001/DG-006/DG-008 findings; no production exposure from an unevaluated probe.
4. Implement/qualify FPS-007 feedback protection and FPS-008 control/session model, with only one primary task ACTIVE at a time.
5. Integrate selected secure video transport under FPS-009.
6. Qualify FPS-010 dual-instance localhost, then the single-host LAN-stack path.
7. Continue remaining optional-audio and V1 tasks from the roadmap with their own outcomes and evidence.

The implementation plan must resolve exact files/APIs and tests after the transport/security probe assumptions are checked. An isolated probe is not a permanent backend, and a successful localhost run is not a finished V1 release.

## 12. Acceptance evidence

| Check | Required evidence |
|---|---|
| Actual process separation | Two executable paths, PIDs, data/identity hashes, source SHA and binary/dependency hashes |
| Actual network use | OS port/process ownership, authenticated session state, transmitted/received byte and AU counters |
| A → B | A captures owned moving fixture; B has no capture source; B decodes/renders A's changing pattern/counter |
| B → A and simultaneous | Distinct owned A/B patterns; independent share/receive counters and separate codec choices |
| Secure trust | Unpaired/wrong/changed/self identity and declined trust cannot reach the control/media decoder |
| Controls | Independent CPU/GPU pairs, bitrate/keyframe/encoding latency, persisted restart state |
| Performance | Five-minute hardware 1080p60 runs, aiming ≥59 rendered fps per direction; report measured fps and p50/p95/p99 latency/resources without hiding failures |
| Bounds/recovery | Maximum count/bytes/age, stalled receiver, large IDR, lost/canceled AU, format generation, reconnect and repeated shutdown |
| Viewer regression | No-source receive, Stop Share versus Disconnect, source loss, Settings, resize/aspect-fit, F11/Esc and feedback protection |
| Build/review | Full Debug/Release CTest, meaningful protocol/identity/queue tests, independent review and exact-commit CI |

Use the same encoded source and settings for candidate comparisons. Measure same-host QPC timings explicitly as same-host evidence; later remote clock alignment is not implied. Record payload integrity independently from metadata and verify decoded content with the owned pattern. A receiver's own preview is never acceptable substitute evidence.

No physical second PC is required for the above development gate. Label localhost, self-LAN, VM/Sandbox and physical remote-machine results separately.

## 13. Primary-source references checked 2026-10-16

- [MsQuic platform/TLS provider support](https://microsoft.github.io/msquic/msquicdocs/docs/Platforms.html)
- [MsQuic current build requirements](https://github.com/microsoft/msquic/blob/main/docs/BUILD.md)
- [Microsoft Schannel TLS support](https://learn.microsoft.com/en-us/windows/win32/secauthn/protocols-in-tls-ssl--schannel-ssp-)
- [Official native OpenSSL package](https://www.nuget.org/packages/Microsoft.Native.Quic.MsQuic.OpenSSL)
- [MsQuic credential configuration](https://microsoft.github.io/msquic/msquicdocs/docs/api/QUIC_CREDENTIAL_CONFIG.html)
- [Checked v2.6.3 OpenSSL provider source](https://github.com/microsoft/msquic/blob/v2.6.3/src/platform/tls_openssl.c)
- [OpenSSL trust anchors, purpose and name verification](https://docs.openssl.org/3.5/man1/openssl-verification-options/)
- [MsQuic ListenerStart and same-port caveat](https://github.com/microsoft/msquic/blob/main/docs/api/ListenerStart.md)
- [MsQuic source license](https://github.com/microsoft/msquic/blob/main/LICENSE)
- [libdatachannel overview/dependencies/license](https://github.com/paullouisageneau/libdatachannel)
- [libdatachannel native API/media documentation](https://github.com/paullouisageneau/libdatachannel/blob/master/DOC.md)

Documentation and upstream main may change. The evaluated implementation must record its exact version, hashes, provider and supported APIs instead of treating these links as runtime proof.

## 14. Design self-review

Reviewed against the actual source and approved amendment: local-loop/network claims are separate; audio remains optional for this increment and required where applicable for eventual V1; no permanent transport/license/security gate is closed in advance; candidate rejection paths and bounds are explicit; Windows 10 is preserved; no UI Auto connection mode is introduced; existing worktrees/main/tags/releases remain untouched. Written-spec approval and continuous Native execution authorization were received at 2026-10-16 09:23:50 Asia/Bangkok. Implementation and runtime qualification remain to be performed.
