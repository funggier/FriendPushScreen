# Qualification Matrix

A task may be called **GREEN** only when its required evidence exists and can be reproduced.

| Task | Qualification evidence | State |
|---|---|---|
| FPS-001 | canonical checkout; documentation baseline; workspace README registration; clean exact-commit Git state; remote branch present; dated evidence report | GREEN |
| FPS-002 | local reference-machine evidence for Tailscale/network baseline; capture-card runtime modes; WDA/backend feasibility; simultaneous hardware encode/decode; unresolved two-peer evidence explicitly deferred to FPS-009/FPS-010 | GREEN |
| FPS-003 | Windows 10 physical launch; D3D11 Hardware placeholder; native OS/CPU/GPU/monitor/capture/codec/network diagnostics; async refresh/export/privacy tests; clean Debug+Release local builds; exact-commit Windows CI | GREEN |
| FPS-004 | UI state tests plus manual UX evidence for viewer/settings/share visibility, Connection Mode/Host/Port shell, sender Audio Source dropdown, receiver mute/volume shell | GREEN |
| FPS-005 | Monitor/Window/Capture Card source enumeration and preview evidence; hot-plug/source-loss behavior | GREEN |
| FPS-006 | local 1080p60 video encode→decode→render loop; bounded queue; latency/resource metrics; H.264 fallback | PENDING |
| FPS-006A | WASAPI System/Device capture; process-loopback capability result; local audio encode→decode→render; source switch; receiver mute/volume; latency/drift/underflow evidence | PENDING |
| FPS-007 | no recursive mirror under supported capture paths; black-mask fallback verified | PENDING |
| FPS-008 | protocol-version/capability/state/reconnect tests covering connection mode plus audio/video capability and audio stream/config state without media dependency | PENDING |
| FPS-009 | written media-transport ADR backed by benchmark/security/licensing evidence; control/video/audio multiplexing; secure handshake; one-port feasibility; network media tests | PENDING |
| FPS-010 | dual-instance localhost plus LAN, Direct IP/DDNS and Tailscale connection evidence; secure Direct Internet; path display; reconnect test | PENDING |
| FPS-011 | repeated one-way video sharing plus optional shared audio across all V1 source types | PENDING |
| FPS-012 | sustained two-way video/audio sharing with independent audio state, receiver mute/volume, resource and feedback evidence | PENDING |
| FPS-013 | controlled impairment tests proving stable video adaptation/hysteresis while preserving control/audio continuity and bounded audio QoS | PENDING |
| FPS-014 | fault-injection matrix including audio source/endpoint loss and restart, with expected/observed recovery behavior | PENDING |
| FPS-015 | clean install, over-install policy, uninstall, runtime packaging and notices verification | PENDING |
| FPS-016 | complete V1 video/audio/connectivity acceptance matrix and release evidence | PENDING |

## Evidence rule

A statement such as “works”, “stable”, “low latency”, or “hardware accelerated” is not qualification evidence by itself. Reports must include the command/test/scenario, observed result, relevant environment, and exact commit being qualified.
