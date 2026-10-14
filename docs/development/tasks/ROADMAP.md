# FriendPushScreen Development Roadmap

This roadmap converts the V1 master specification into execution-sized FPS tasks. The specification remains authoritative for detailed requirements.

| Task | Name | Primary outcome | State |
|---|---|---|---|
| FPS-001 | Engineering Baseline | Repository/workspace/task/qualification structure | GREEN |
| FPS-002 | Reference Baseline & Technical Spike | Local reference-machine qualification: Tailscale baseline, capture-card modes, WDA behavior, simultaneous hardware encode/decode | GREEN |
| FPS-003 | Windows App Skeleton & Hardware Diagnostics | Launchable Windows app plus capability inventory | PLANNED |
| FPS-004 | Viewer UI & Settings Shell | Viewer-first UI, settings overlay, state visibility | PLANNED |
| FPS-005 | Unified Capture Sources | Monitor, Window, and generic Capture Card preview through one source abstraction | PLANNED |
| FPS-006 | Encode / Decode / Local Loopback | Hardware paths plus H.264 fallback and stable 1080p60 local loop | PLANNED |
| FPS-007 | Feedback Protection | Capture exclusion where supported plus black-mask fallback | PLANNED |
| FPS-008 | Control Protocol | Versioned handshake, peer/share state, capabilities, heartbeat, reconnect semantics | PLANNED |
| FPS-009 | Transport Decision & Network Video | Evidence-backed reuse/fork/native decision and bounded low-latency transport | PLANNED |
| FPS-010 | Internet / P2P Integration | Direct/Relay diagnostics, Tailscale-aware connectivity, reconnect | PLANNED |
| FPS-011 | One-Way Production Flow | Daily-usable A → B sharing across all V1 source types | PLANNED |
| FPS-012 | Two-Way Sharing | Stable simultaneous encode/decode A ↔ B with feedback protection | PLANNED |
| FPS-013 | Quality Controller | Conservative bitrate/FPS/resolution adaptation with hysteresis | PLANNED |
| FPS-014 | Recovery Qualification | Network, peer, source, device, sleep/wake and related fault recovery | PLANNED |
| FPS-015 | Installer & Distribution | Windows x64 install/uninstall, runtime packaging, notices/compliance | PLANNED |
| FPS-016 | V1 Acceptance & Release | Full compatibility, UX, safety, recovery and documentation acceptance | PLANNED |

## Roadmap rules

- Only one primary FPS task should be ACTIVE unless a later task explicitly documents safe parallel work.
- A task does not become GREEN from code completion alone; qualification evidence is required.
- Architecture gates remain unresolved until the task that owns the necessary evidence.
- V2 candidates stay out of this roadmap until V1 acceptance is complete.
