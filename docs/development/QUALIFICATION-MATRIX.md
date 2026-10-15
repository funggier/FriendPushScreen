# Qualification Matrix

A task may be called **GREEN** only when its required evidence exists and can be reproduced.

| Task | Qualification evidence | State |
|---|---|---|
| FPS-001 | canonical checkout; documentation baseline; workspace README registration; clean exact-commit Git state; remote branch present; dated evidence report | GREEN |
| FPS-002 | local reference-machine evidence for Tailscale/network baseline; capture-card runtime modes; WDA/backend feasibility; simultaneous hardware encode/decode; unresolved two-peer evidence explicitly deferred to FPS-009/FPS-010 | GREEN |
| FPS-003 | Windows 10 physical launch; D3D11 Hardware placeholder; native OS/CPU/GPU/monitor/capture/codec/network diagnostics; async refresh/export/privacy tests; clean Debug+Release local builds; exact-commit Windows CI | GREEN |
| FPS-004 | UI state tests plus manual UX evidence for viewer/settings/share visibility | PENDING |
| FPS-005 | Monitor/Window/Capture Card source enumeration and preview evidence; hot-plug/source-loss behavior | PENDING |
| FPS-006 | local 1080p60 encode→decode→render loop; bounded queue; latency/resource metrics; H.264 fallback | PENDING |
| FPS-007 | no recursive mirror under supported capture paths; black-mask fallback verified | PENDING |
| FPS-008 | protocol-version/capability/state/reconnect tests without video dependency | PENDING |
| FPS-009 | written transport ADR backed by benchmark/security/licensing evidence; network video tests | PENDING |
| FPS-010 | cross-network connection evidence; Direct/Relay display; reconnect test | PENDING |
| FPS-011 | repeated one-way sharing across all V1 source types | PENDING |
| FPS-012 | sustained two-way sharing with resource and feedback evidence | PENDING |
| FPS-013 | controlled impairment tests proving stable adaptation and hysteresis | PENDING |
| FPS-014 | fault-injection matrix with expected/observed recovery behavior | PENDING |
| FPS-015 | clean install, over-install policy, uninstall, runtime packaging and notices verification | PENDING |
| FPS-016 | complete V1 acceptance matrix and release evidence | PENDING |

## Evidence rule

A statement such as “works”, “stable”, “low latency”, or “hardware accelerated” is not qualification evidence by itself. Reports must include the command/test/scenario, observed result, relevant environment, and exact commit being qualified.
