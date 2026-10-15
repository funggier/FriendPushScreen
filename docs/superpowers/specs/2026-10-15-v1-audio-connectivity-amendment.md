# FriendPushScreen V1 Audio + Connectivity Architecture Amendment

**Status:** APPROVED DIRECTION / WRITTEN AMENDMENT

**Date:** 2026-10-15

**Issue:** #7 â€” V1 Architecture Amendment â€” Audio + Connectivity

**Branch:** `fps-v1-audio-connectivity-amendment`

**Applies before:** FPS-004 and all later V1 work

**Overrides where conflicting:** `docs/development-plan/ScreenLink_V1_Architecture_and_Development_Plan_TH.md`

---

## 1. Purpose

FriendPushScreen V1 is no longer video-only.

V1 now carries:

- real-time video
- optional shared machine audio
- control/session metadata

FriendPushScreen still does **not** become a voice-chat application. Discord or another external application may continue to provide voice/chat independently.

This amendment also makes all three intended connectivity paths first-class V1 modes:

- LAN
- Direct IP / DDNS
- Tailscale

The media/control protocol must remain the same across these paths. Only endpoint discovery/routing changes.

---

## 2. V1 connectivity model

### 2.1 Connection modes

The UI and runtime model expose:

```text
Connection Mode
- LAN
- Direct IP / DDNS
- Tailscale
```

Manual endpoint fields:

```text
Host: hostname / IPv4 / IPv6
Port: numeric port
```

Examples:

```text
LAN:
192.168.1.25:42800

Direct IP:
203.0.113.25:42800

Direct DDNS:
friend-a.example.net:42800

Tailscale:
100.x.x.x:42800
or Tailscale hostname
```

The examples are illustrative. No address or hostname is hardcoded into the product.

### 2.2 One transport/session architecture

FriendPushScreen must not implement a different media protocol for each connection mode.

Conceptually:

```text
Capture / Encode / Audio
          |
          v
Secure FriendPushScreen Session
  - control logical channel
  - video logical channel
  - audio logical channel
          |
          v
Endpoint / Route
  - LAN
  - Direct IP / DDNS
  - Tailscale
```

The connection mode chooses how the peer endpoint is reached.

It does not change:

- control message schema
- media packet semantics
- capability negotiation
- stream identifiers
- recovery rules
- A/V timestamp domain

### 2.3 Single-port target

V1 should target one public UDP port for Direct IP/DDNS when the selected transport permits it.

Preferred operational model:

```text
UDP 42800 (example configurable port)
  - secure handshake
  - control
  - video
  - audio
  - heartbeat
  - reconnect/session state
```

The exact port number is configurable and is not architecture-significant.

The single-port goal is a deployment requirement, not permission to invent custom cryptography or an unsafe multiplexing protocol.

If measured transport/security constraints require more than one port, FPS-009 must document the evidence and obtain an explicit architecture decision.

### 2.4 Direct Internet is first-class V1

Direct IP/DDNS is not a debugging-only fallback.

It must support production V1 use where the user controls:

- public IP or DDNS
- router/NAT port forwarding
- firewall rules

Direct Internet qualification must prove authentication, encryption, reconnect, endpoint diagnostics, and bounded media behavior.

---

## 3. Direct Internet security

A forwarded public port must never expose an unauthenticated plaintext FriendPushScreen session.

Before control/media data is accepted, the session requires:

- peer authentication
- encrypted transport
- replay-resistant handshake
- session identity
- explicit pairing/authorization on first trust

Recommended UX:

```text
First connection:
Host + Port
  -> secure handshake
  -> pairing code / fingerprint confirmation
  -> store peer identity

Later connections:
Host + Port
  -> verify stored peer identity
  -> encrypted session
```

The application must not implement its own novel cryptographic primitive.

FPS-009 owns the mature secure-transport choice.

Tailscale already provides an encrypted overlay, but FriendPushScreen must still preserve its own peer/session identity model so application authorization semantics do not depend on route type.

---

## 4. Single-machine development strategy

A second physical Windows PC is not required for ordinary development tasks.

FriendPushScreen must support a dual-instance development mode.

### Level 1 â€” localhost

```text
Instance A
127.0.0.1:42800
      |
      v
Instance B
127.0.0.1:42801
```

This qualifies:

- protocol parsing
- secure handshake structure
- stream multiplexing
- reconnect
- packetization
- local encode/decode integration
- two independent application states

### Level 2 â€” local machine LAN stack

Two instances may communicate through the machine's own LAN address and distinct ports.

This exercises more of the normal Windows network stack than an in-memory loop.

### Level 3 â€” isolated peer simulation

When needed, a Windows Sandbox or VM may act as a second logical peer on the same physical computer.

This is optional until a task specifically requires stronger isolation.

### Physical two-peer qualification

A real second Windows machine remains desirable for final network/release acceptance but does not block intermediate implementation tasks when equivalent local evidence is possible.

The qualification report must distinguish:

- dual-instance localhost
- single-host LAN-stack
- VM/Sandbox
- physical two-peer

No category may be mislabeled as another.

---

## 5. Audio product model

### 5.1 Audio is optional and independent from video

Video sharing may run with or without audio.

Audio state:

```text
Disabled
Starting
Active
Muted locally by receiver
Source lost
Error
```

Changing audio source must not require stopping the video share or reconnecting the peer session.

If audio cannot be reconfigured in-place, only the audio logical stream may restart.

### 5.2 Sender Audio Source dropdown

The sender UI uses one dropdown, not multiple source checkboxes.

Conceptual UI:

```text
Audio Source
[ Desktop / System Audio                  v ]
```

Dropdown groups:

```text
No Audio

System
  Desktop / System Audio
  Default Output Device

Output Devices
  Speakers (...)
  HDMI / Display Audio (...)
  USB Audio Output (...)

Input / Capture Devices
  USB Capture Audio (...)
  Other accessible capture endpoint (...)

Applications
  Firefox
  Chrome
  VLC
  Game.exe
  ...
```

Exact entries are capability-driven and generated from live Windows audio state.

### 5.3 No Audio

When `No Audio` is selected:

- audio capture stops
- audio encoder stops
- no audio media packets are sent
- video/control session remains active
- peer sees audio state = disabled

### 5.4 Desktop / System Audio

Primary V1 system-audio path:

- WASAPI
- shared mode
- loopback capture on a render endpoint

The default option captures the Windows audio mix for the selected/default playback endpoint.

A render endpoint selected explicitly from the dropdown uses loopback on that endpoint.

### 5.5 Input / capture device audio

Accessible Windows capture endpoints may be listed as source choices.

Examples include:

- USB capture-card audio
- USB audio interface
- line input
- microphone-class endpoint

A microphone-class endpoint appearing as a generic capture device does **not** turn FriendPushScreen into voice chat.

V1 does not add:

- acoustic echo cancellation
- noise suppression suite
- voice activation
- conference mixing
- microphone-chat UX

### 5.6 Application audio

Application/process audio is a V1 capability target but is runtime-dependent.

Preferred native path:

- `ActivateAudioInterfaceAsync`
- `AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`
- include target process tree

Microsoft documents this process-loopback API with a minimum supported client of Windows build 20348.

The current reference machine is Windows 10 build 19045.

Therefore:

> Native per-application loopback is **not available on the current reference machine through this official API**.

Required UI behavior on an unsupported machine:

- Application Sources section may be hidden or disabled
- diagnostics must report why
- the program must not fake support
- System Audio and device sources remain available

FriendPushScreen must not use undocumented process injection or audio-hooking solely to force this feature onto unsupported Windows builds.

A later approved compatibility backend may extend support if it is safe, maintainable, and license-compatible.

---

## 6. Receiver audio controls

The receiver has local playback controls for the incoming peer audio stream.

Required V1 controls:

```text
Remote Audio
[Mute / Unmute]
Volume [0----------------100]
```

Rules:

- mute is receiver-local by default
- volume is receiver-local
- receiver mute does not change sender source selection
- receiver mute does not stop sender video
- receiver mute does not require reconnect
- mute state may persist as a local preference
- peer is not required to know the receiver's local volume

A future bandwidth-saving `suspend remote audio` command is not required for V1.

---

## 7. Audio capture architecture

Preferred native capture technology:

```text
WASAPI
```

Source abstraction:

```text
IAudioSource
  - NoneAudioSource
  - WasapiSystemLoopbackSource
  - WasapiRenderEndpointLoopbackSource
  - WasapiCaptureEndpointSource
  - ProcessLoopbackSource (runtime capability required)
```

The interface must expose at minimum:

- source ID
- display name
- source kind
- runtime support state
- sample format
- channel count
- sample rate
- start/stop
- source-loss/error state
- timestamped audio frames

UI types must not cross into the audio core.

---

## 8. Audio codec policy

Audio transport should use a real-time codec rather than raw PCM over the Internet.

Preferred baseline for evaluation:

```text
Opus
48 kHz
10 or 20 ms frames
mono/stereo according to source
adaptive bitrate within a bounded profile
```

Reasons:

- low-latency interactive design
- robust behavior across network conditions
- small bandwidth relative to video
- mature ecosystem

This is a preferred direction, not yet a permanent dependency decision.

FPS-006A must qualify local audio encode/decode behavior.

FPS-009 must resolve codec packaging/licensing and transport integration before network audio becomes permanent.

Raw PCM may be used only as a local diagnostic/loopback stage.

---

## 9. Audio/video timing

Video and audio use one session monotonic time domain.

Each media frame carries:

- stream identifier
- sequence information
- presentation timestamp
- format/config generation

Receiver architecture:

```text
Video packets -> video jitter/queue -> decode -> D3D11 present
Audio packets -> audio jitter buffer -> decode -> WASAPI render
                         |
                         +---- shared session clock ----+
```

V1 requirements:

- audio must not drift indefinitely from video
- source restart creates a new audio configuration generation
- stale audio from the old generation is discarded
- bounded jitter buffering
- audio underflow/overrun metrics are diagnostic data

The exact A/V synchronization policy is qualified during local and network loopback tasks.

---

## 10. Congestion/QoS policy

Audio consumes much less bandwidth than video and is perceptually sensitive to gaps.

Default congestion priority:

1. control/heartbeat/security state
2. audio continuity
3. video quality

Under bandwidth pressure, FriendPushScreen should reduce:

- video bitrate
- video resolution
- video frame rate

before intentionally destroying audio continuity.

Audio bitrate may adapt within its safe codec profile, but video remains the primary bandwidth control surface.

---

## 11. Control protocol additions

FPS-008 must include versioned audio capability/state messages.

At minimum:

```text
AudioCapabilities
  source kinds supported
  codec support
  max channels
  sample rates
  process-loopback capability

AudioSourceSelected
  source ID
  source kind
  config generation

AudioStreamState
  disabled / starting / active / lost / error

AudioFormat
  codec
  sample rate
  channels
  frame duration
```

Receiver-local mute and volume do not require peer messages in V1.

---

## 12. UI requirements beginning FPS-004

FPS-004 must reserve final UI state for:

### Sender side

- Audio Source dropdown
- selected audio source state
- unsupported source explanation
- audio active/error indicator

### Receiver side

- remote-audio mute/unmute button
- remote-audio volume slider
- incoming audio status

### Connection

- mode dropdown: LAN / Direct IP-DDNS / Tailscale
- Host
- Port
- Connect / Disconnect state
- current connection mode/path
- advanced diagnostics entry

FPS-004 creates the UI/state shell only.

It must not prematurely implement network media or audio capture.

---

## 13. Diagnostics additions

Future diagnostics must expose:

### Connection

- selected mode
- resolved endpoint
- transport state
- secure-session state
- Direct/LAN/Tailscale path
- RTT
- reconnect count

### Audio

- selected source
- source type
- source support state
- input sample rate/channels
- audio codec
- encoded bitrate
- frame duration
- queue/jitter depth
- underflow count
- overrun/drop count
- source restarts
- process-loopback capability/reason

Sensitive pairing credentials or private keys must never be exported.

---

## 14. Roadmap ownership

The existing task numbering is preserved.

One inserted task is added without renumbering later tasks:

### FPS-004 â€” Viewer UI & Settings Shell

Add:

- Connection Mode dropdown
- Host / Port fields
- sender Audio Source dropdown
- receiver mute/unmute
- receiver volume slider
- capability/error state shell

No audio capture/network media yet.

### FPS-005 â€” Unified Capture Sources

Remains focused on video capture.

Audio-device enumeration may be displayed by shared diagnostics but does not expand the video source abstraction.

### FPS-006 â€” Encode / Decode / Local Loopback

Remains the video local-loop milestone.

### FPS-006A â€” Audio Capture / Encode / Local Loopback

New task.

Required outcomes:

- WASAPI system loopback
- explicit render endpoint loopback
- capture endpoint source
- process-loopback capability probe
- local PCM loopback
- local real-time audio codec encode/decode
- receiver playback
- source switching without stopping video shell
- local mute/volume behavior
- latency, underflow, drift, resource evidence

### FPS-007 â€” Feedback Protection

Video feedback protection remains unchanged.

### FPS-008 â€” Control Protocol

Expanded to include:

- connection endpoint/mode state
- audio capabilities
- audio source/config generation
- audio stream state
- audio format negotiation

### FPS-009 â€” Media Transport Decision & Network Streaming

Broaden from network video to network media.

Must decide:

- transport implementation
- secure handshake
- logical control/video/audio multiplexing
- one-port feasibility
- audio/video packet timing
- licensing

### FPS-010 â€” Connectivity Modes

Broaden to qualify:

- localhost dual-instance
- LAN
- Direct IP
- DDNS
- Tailscale
- port forwarding documentation
- secure Direct Internet
- reconnect/path diagnostics

### FPS-011 â€” One-Way Production Flow

A -> B production flow includes optional audio.

### FPS-012 â€” Two-Way Sharing

Each direction has independent:

- video source/state
- audio source/state
- receiver-local mute/volume

### FPS-013 â€” Quality Controller

Add audio continuity priority and audio/network metrics.

### FPS-014 â€” Recovery Qualification

Add:

- audio source lost
- endpoint removed
- default device changed
- audio decoder/render failure
- audio restart without video restart

### FPS-015 â€” Installer & Distribution

Must package and document any approved audio codec/runtime dependency and its license notices.

### FPS-016 â€” V1 Acceptance & Release

Must include audio and all three connectivity paths.

---

## 15. Qualification rules

V1 cannot be called complete unless applicable evidence exists for:

### Audio

- No Audio works without audio packets
- Desktop/System Audio capture on supported Windows
- explicit output-device loopback
- at least one capture/input endpoint when available
- capture-card audio when exposed as a Windows audio endpoint
- Application Audio on a supported OS, **or** correct unsupported behavior on a runtime below the process-loopback minimum
- sender can change audio source without stopping video share
- receiver mute/unmute works
- receiver volume works
- bounded local audio latency
- audio/video drift remains bounded
- source-loss recovery does not crash the application

### Connectivity

- dual-instance localhost
- LAN endpoint
- Direct IP/DDNS endpoint
- Tailscale endpoint
- secure Direct Internet handshake
- reconnect
- endpoint/path diagnostics
- port-forward setup documented
- one-port target qualified or evidence-backed exception documented

Final physical two-peer acceptance must not claim localhost/VM evidence is equivalent to an actual remote Windows peer.

---

## 16. Explicitly still out of V1

Even with shared audio in V1, the following remain out:

- integrated voice chat
- conference call system
- acoustic echo cancellation
- voice activity detection
- automatic microphone gain processing
- remote keyboard/mouse control
- gamepad forwarding
- file transfer
- clipboard synchronization
- public matchmaking
- 3+ peers
- recording/replay buffer
- custom cloud video/audio relay service
- browser/mobile/macOS/Linux clients

---

## 17. Compatibility note for the current reference machine

Current reference environment:

```text
Windows 10 build 19045
```

Expected native V1 audio capability on this machine:

```text
Desktop/System Audio       supported target
Render endpoint loopback   supported target
Capture endpoint audio     supported target
Application process audio  unsupported by Microsoft's process-loopback API
```

This limitation is a capability result, not an implementation failure.

The UI must communicate it clearly.

---

## 18. Decision status

Approved now:

- V1 includes optional shared audio
- sender uses one Audio Source dropdown
- receiver has mute/unmute and local volume
- System Audio is supported
- accessible device audio is supported
- per-application audio is a V1 capability target when runtime supports it
- LAN / Direct IP-DDNS / Tailscale are first-class V1 connection modes
- Direct Internet is first-class
- one transport/session architecture serves all connection modes
- one public UDP port is the operational target
- single-machine dual-instance development is first-class

Still evidence-gated:

- final audio codec/dependency
- final secure network transport
- exact cryptographic/session library
- final one-port transport feasibility
- exact A/V jitter/sync policy
- any compatibility backend for per-application capture on Windows 10 build 19045

---

## 19. Platform references for audio compatibility

Microsoft Learn references used for this amendment:

- WASAPI Loopback Recording â€” system/render-endpoint loopback capture
- Application Loopback Audio Capture sample â€” process-tree audio capture
- `AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK` â€” documented minimum supported client: Windows build 20348
- `AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS` â€” process-tree include/exclude parameters
- WASAPI Audio Sessions â€” session grouping/control semantics

Implementation must re-check current SDK/runtime documentation when FPS-006A begins rather than treating this 2026-10-15 compatibility note as permanent platform truth.
