# FriendPushScreen

FriendPushScreen is a Windows real-time screen-sharing project for two peers who want higher-quality, lower-latency video plus optional shared machine audio while continuing to use an external service such as Discord for voice and chat.

> **Current state:** native Windows application baseline qualified through FPS-003. The repository now builds a launchable Win32/D3D11 diagnostics application. Screen sharing itself is not implemented yet; capture, audio/video codec execution, and secure network-media milestones remain ahead.

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

## Current task

See:

- `docs/development/tasks/CURRENT.md`
- `docs/development/tasks/ROADMAP.md`
- `docs/development/QUALIFICATION-MATRIX.md`
- `docs/architecture/DECISION-GATES.md`

Build instructions are available in `docs/BUILDING.md`. The current executable provides the native Win32/D3D11 application shell and hardware diagnostics foundation; real screen-sharing/audio/network-media work starts in the later capture/codec/network tasks.
