# FriendPushScreen Development Roadmap

This roadmap converts the V1 master specification into execution-sized FPS tasks. The specification remains authoritative for detailed requirements.

| Task | Name | Primary outcome | State |
|---|---|---|---|
| FPS-001 | Engineering Baseline | Repository/workspace/task/qualification structure | GREEN |
| FPS-002 | Reference Baseline & Technical Spike | Local reference-machine qualification: Tailscale baseline, capture-card modes, WDA behavior, simultaneous hardware encode/decode | GREEN |
| FPS-003 | Windows App Skeleton & Hardware Diagnostics | Launchable native Win32/D3D11 app plus capability inventory | GREEN |
| FPS-004 | Viewer UI & Settings Shell | Viewer/settings shell, Share Port, Connection Host/Remote Port, program-owned dark controls, canonical Windows app icon, sender Audio Source, receiver mute/volume state | GREEN |
| FPS-004A | Clean Viewer | Persist whole status-strip visibility; preserve Tab access and full-client rendering | SCOPED PASS â€” native Debug/Release and real UI/relaunch |
| FPS-005 | Unified Capture Sources | Monitor, Window, and generic Capture Card preview through one source abstraction | GREEN |
| FPS-006 | Encode / Decode / Local Loopback | Hardware video paths plus H.264 fallback and stable 1080p60 local loop | GREEN |
| FPS-006A | Audio Capture / Encode / Local Loopback | WASAPI System/Device sources, process-loopback capability, local audio codec loop, receiver playback/mute/volume and selectable output | SCOPED PASS — native/local and secure localhost; broader recovery gates open |
| FPS-007 | Feedback Protection | Capture exclusion where supported plus black-mask fallback | SCOPED PASS in FPS-009; full V1 inputs open |
| FPS-008 | Control Protocol | Versioned handshake, connection/audio/video capabilities, peer/share/audio state, heartbeat, reconnect semantics | SCOPED PASS in FPS-009; full V1 inputs open |
| FPS-009 | Media Transport Decision & Network Streaming | Evidence-backed secure low-latency control/video/audio transport; one-port feasibility; timing/multiplexing | PROVISIONAL — localhost video+audio subgate recorded; TCP comparison/remote inputs and decision open |
| FPS-009A | Peer Status Metrics | Correlated application Ping and actual Send/View FPS, stale resets and existing heartbeat | SCOPED PASS â€” localhost8996/8997, performance comparison retained |
| FPS-010 | Connectivity Modes | Dual-instance localhost, LAN, Direct IP/DDNS, Tailscale, secure Direct Internet, reconnect/path diagnostics | PLANNED |
| FPS-011 | One-Way Production Flow | Daily-usable A → B video plus optional shared audio across all V1 source types | PLANNED |
| FPS-012 | Two-Way Sharing | Stable A ↔ B video/audio streams with independent audio state, receiver mute/volume, and feedback protection | PLANNED |
| FPS-013 | Quality Controller | Conservative video adaptation with hysteresis while preserving control/audio continuity and bounded audio QoS | PLANNED |
| FPS-014 | Recovery Qualification | Network, peer, video/audio source, endpoint/device, sleep/wake and related fault recovery | PLANNED |
| FPS-015 | Installer & Distribution | Windows x64 install/uninstall, runtime/audio codec packaging, firewall guidance, notices/compliance | PLANNED |
| FPS-016 | V1 Acceptance & Release | Full video/audio/connectivity compatibility, UX, safety, recovery and documentation acceptance | PLANNED |

## Roadmap rules

- Only one primary FPS task should be ACTIVE unless a later task explicitly documents safe parallel work.
- A task does not become GREEN from code completion alone; qualification evidence is required.
- Architecture gates remain unresolved until the task that owns the necessary evidence.
- V2 candidates stay out of this roadmap until V1 acceptance is complete.
