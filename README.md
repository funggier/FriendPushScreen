# FriendPushScreen

FriendPushScreen is a Windows real-time screen-sharing project for two peers who want higher-quality, lower-latency video plus optional shared machine audio while continuing to use an external service such as Discord for voice and chat.

> **Current state on this branch:** FPS-006 local H.264 preview and independent CPU/GPU controls are qualified. Native mutually authenticated peer video is implemented for a loopback-only No Audio evaluation; actual two-instance qualification is in progress.

## V1 product boundary

FriendPushScreen V1 targets **video + optional shared machine audio**.

V1 targets:

- Windows 10 Version 2004+ and Windows 11 x64
- two equal peers; either side may send and view
- one-way or simultaneous two-way sharing
- Monitor, Window, and generic Capture Card sources
- hardware encode/decode when available with software fallback
- H.264 compatibility baseline
- capability-dependent HEVC and AV1
- sender Audio Source dropdown: No Audio / System / Device / capability-dependent Application Audio
- receiver remote-audio mute/unmute and local volume
- LAN / Direct IP-DDNS / Tailscale connection modes
- secure Direct Internet requirement with a one-public-UDP-port operational target when feasible
- low-latency Direct P2P preference
- reconnect and video/audio source/device recovery
- explicit Share / Stop Share state
- feedback-loop protection
- no auto-share after restart

V1 intentionally excludes integrated voice chat/AEC, remote keyboard/mouse control, clipboard synchronization, file transfer, public accounts/matchmaking, recording, multi-user sessions, and mobile/browser clients. Application Audio is capability-dependent; unsupported Windows runtimes must report that limitation explicitly.

## Naming

The repository and project name is **FriendPushScreen**.

The master V1 document currently uses **ScreenLink** as a temporary product name. Until an explicit naming decision is made, references to ScreenLink in that document mean FriendPushScreen V1 and do not represent a separate repository.

## Authoritative project sources

When sources disagree, use this order:

1. live Git / GitHub / runtime evidence
2. current task and qualification documents
3. explicit approved architecture amendments
4. the V1 master specification
5. older reports or historical notes

Master specification:

`docs/development-plan/ScreenLink_V1_Architecture_and_Development_Plan_TH.md`

Authoritative V1 audio/connectivity amendment:

`docs/superpowers/specs/2026-10-15-v1-audio-connectivity-amendment.md`

Project documentation index:

`docs/README.md`

## Development flow

The implementation sequence is evidence-driven:

```text
Reference baseline / technical spike
        ↓
Windows app + hardware diagnostics
        ↓
Capture sources
        ↓
Video encode / decode / local loopback
        ↓
Audio capture / encode / local loopback
        ↓
Feedback protection
        ↓
Control protocol
        ↓
Secure media transport
        ↓
LAN / Direct IP-DDNS / Tailscale
        ↓
One-way production flow
        ↓
Two-way sharing
        ↓
Recovery / installer / V1 acceptance
```

The project must prove each layer before depending on the next one. In particular, network transport is not selected permanently until benchmark, security, audio/video multiplexing, and licensing evidence are available.

## Local workspace

Canonical development path:

```text
T:\Space\Projects\ProjectsProgram\FriendPushScreen
```

GitHub:

```text
funggier/FriendPushScreen
```

Visibility: public

## License

FriendPushScreen is licensed under the MIT License. See `LICENSE`.

Third-party components, if introduced later, remain subject to their own licenses and must pass the project's licensing/compliance gate before integration.

## Video codec settings

In **Settings → VIDEO / H.264**, choose the encoder and decoder independently:

- **Prefer GPU (allow CPU fallback)** keeps the existing default behavior.
- A named **[GPU]** or **[CPU]** entry selects that exact native Windows codec registration. GPU decoding can use the Microsoft H.264 decoder with D3D11 acceleration even though its MFT catalog class is software.
- Set target bitrate (1–200 Mbps), keyframe interval (1–600 frames), and **Low latency encoding**, then click **Apply Video**. Values persist after restart; active preview restarts its codec loop while preserving the capture source.
- Live status names the codecs and reports the CPU/GPU paths actually producing frames. An unavailable saved codec reports an error until another choice is applied.
- The decoder keeps its qualified real-time low-latency mode. CPU codec selection still uses D3D11 for color conversion and display.
- H.264 is the implemented native local-loop codec. HEVC/AV1 capability evidence does not make those codecs selectable here yet.

`FPS_FORCE_SOFTWARE_CODEC=1` remains a diagnostic override forcing both codecs to CPU; `FPS_RAW_PREVIEW=1` bypasses codecs. Both overrides are shown in video status.

## Current task

See:

- `docs/development/tasks/CURRENT.md`
- `docs/development/tasks/ROADMAP.md`
- `docs/development/QUALIFICATION-MATRIX.md`
- `docs/architecture/DECISION-GATES.md`

Build instructions are available in `docs/BUILDING.md`. The current executable includes native diagnostics, unified capture, and decoded H.264 local preview. Secure peer video is available for the loopback evaluation described below. Audio capture/playback and non-loopback connectivity remain later qualification tasks.

## Two portable instances on one PC

This development increment uses native H.264 over pinned TLS 1.3 QUIC. The transport candidate remains provisional. It currently accepts IPv4 loopback endpoints or `localhost`; DNS, LAN, Internet and Tailscale are not qualified by this increment.

1. Copy `FriendPushScreen.exe` and its pinned `msquic.dll` into two separate folders A and B. Create an empty `portable.flag` in each folder.
2. Open both copies. Give A Share Port `42801` and B Share Port `42802`.
3. Use **Export Identity** in each copy. Exchange only the public `.friendpair` files. Use **Pair with Friend**, compare the complete SHA-256 fingerprint with the other copy, and confirm only that known identity.
4. In A, choose an owned capture source and click **Start Share**.
5. In B, set Host `127.0.0.1`, Remote Port `42801`, and click **Connect**. B can receive without selecting any local source.
6. To send B's picture back over the same connection, choose B's source and click **Start Share**. Both peers choose their encoder and decoder independently.

**Stop Share** stops only your outgoing picture; incoming video and the connection continue. **Disconnect** cancels listening, reconnect and outgoing sharing. Starting the program does not share automatically. **Preview** is a separate local choice, and incoming video has viewer priority.

Each portable folder owns a different persistent identity, public peer trust and settings. Private identity data is protected for the current Windows user and is never exported in the pairing file. Do not reuse the same data folder for both peers.

The application bounds media to two access units per direction, waits for the remote decoder before sending, and returns send capacity after the remote consumer releases a frame. Genuine dependency loss requests a fresh generation and IDR with parameter sets. Peer presentation uses sync interval zero so waiting for a display refresh does not block capture and reception. Existing local preview keeps its default presentation behavior.

Both application windows are excluded from capture where Windows supports it. Monitor capture also applies the checked black-mask fallback, and selecting an application's own viewer window is rejected. Physical feedback, recovery and sustained performance results must be read from the dated qualification report, not inferred from these implementation notes.

`FPS_CODEC_METRICS=1` enables bounded diagnostic traces and actual codec/transport counters. These development diagnostics are opt-in; normal video operation does not require them.
