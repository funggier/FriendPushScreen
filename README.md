# FriendPushScreen

FriendPushScreen is a Windows real-time screen-sharing project for two peers who want higher-quality, lower-latency video sharing while continuing to use an external service such as Discord for voice and chat.

> **Current state:** engineering baseline / pre-implementation. The repository currently contains the V1 specification and project-development structure; it does **not** yet contain a working screen-sharing application.

## V1 product boundary

FriendPushScreen V1 is intentionally **video only**.

V1 targets:

- Windows 10 Version 2004+ and Windows 11 x64
- two equal peers; either side may send and view
- one-way or simultaneous two-way sharing
- Monitor, Window, and generic Capture Card sources
- hardware encode/decode when available with software fallback
- H.264 compatibility baseline
- capability-dependent HEVC and AV1
- low-latency Direct P2P preference
- reconnect and source/device recovery
- explicit Share / Stop Share state
- feedback-loop protection
- no auto-share after restart

V1 intentionally excludes audio streaming, remote keyboard/mouse control, clipboard synchronization, file transfer, public accounts/matchmaking, recording, multi-user sessions, and mobile/browser clients.

## Naming

The repository and project name is **FriendPushScreen**.

The master V1 document currently uses **ScreenLink** as a temporary product name. Until an explicit naming decision is made, references to ScreenLink in that document mean FriendPushScreen V1 and do not represent a separate repository.

## Authoritative project sources

When sources disagree, use this order:

1. live Git / GitHub / runtime evidence
2. current task and qualification documents
3. the V1 master specification
4. older reports or historical notes

Master specification:

`docs/development-plan/ScreenLink_V1_Architecture_and_Development_Plan_TH.md`

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
Encode / decode
        ↓
Local loopback
        ↓
Feedback protection
        ↓
Control protocol
        ↓
Transport decision + network path
        ↓
One-way production flow
        ↓
Two-way sharing
        ↓
Recovery / installer / V1 acceptance
```

The project must prove each layer before depending on the next one. In particular, network transport is not selected permanently until benchmark and licensing evidence are available.

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

No build instructions are published yet because product implementation has not started. The first implementation-oriented work begins only after the engineering baseline is qualified.
