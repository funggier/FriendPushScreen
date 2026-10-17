# Qualification Matrix

A task may be called **GREEN** only when its required evidence exists and can be reproduced.

| Task | Qualification evidence | State |
|---|---|---|
| FPS-001 | canonical checkout; documentation baseline; workspace README registration; clean exact-commit Git state; remote branch present; dated evidence report | GREEN |
| FPS-002 | local reference-machine evidence for Tailscale/network baseline; capture-card runtime modes; WDA/backend feasibility; simultaneous hardware encode/decode; unresolved two-peer evidence explicitly deferred to FPS-009/FPS-010 | GREEN |
| FPS-003 | Windows 10 physical launch; D3D11 Hardware placeholder; native OS/CPU/GPU/monitor/capture/codec/network diagnostics; async refresh/export/privacy tests; clean Debug+Release local builds; exact-commit Windows CI | GREEN |
| FPS-004 | UI state tests plus manual UX evidence for viewer/settings/share visibility, Connection Mode/Host/Port shell, sender Audio Source dropdown, receiver mute/volume shell | GREEN |
| FPS-005 | Monitor/Window/Capture Card source enumeration and preview evidence; hot-plug/source-loss behavior | GREEN |
| FPS-006 | local 1080p60 video encode→decode→render loop; bounded queue; latency/resource metrics; H.264 fallback; dated FPS-006 and codec-controls reports | GREEN on recorded sources |
| FPS-006A | native-audio report/JSON: real Opus/WASAPI conversion; selectable persisted output/mute/volume;13/13 physical cases;5min duplex50audio frames/s,0drops/PLC/underflow;29947output10ms blocks without silence; Debug/Release and exact-sourceCI | SCOPED PASS — localhost; endpoint-loss/external-signal/process gates open |
| FPS-007 | native-peer-video report: physical owned DDA/WGC exclusion and fallback, movement/history, CPU/GPU masks; physical negative-origin/card network paths remain open | SCOPED PASS |
| FPS-008 | native-peer-video report: authenticated No Audio capabilities, independent share/receive, generations, heartbeat/reconnect and simultaneous Connect; local audio capability/source/format/ready and independent generations qualified in audio report; remote/recovery state remains open | SCOPED PASS |
| FPS-009 | actual secure No Audio localhost video, codec matrix, >=59fps throughput aim and bounded recovery evidence; strict generation-continuity failures retained; localhost audio/output subgate qualified separately; TCP/remote/permanent decision inputs open | ACTIVE / provisional |
| FPS-010 | actual dual-instance pinned localhost subgate demonstrated; LAN, Direct IP/DDNS, Tailscale, Direct Internet and physical second-PC paths remain open | LOCALHOST SUBGATE ONLY |
| FPS-011 | repeated one-way video sharing plus optional shared audio across all V1 source types | PENDING |
| FPS-012 | sustained two-way video/audio sharing with independent audio state, receiver mute/volume, resource and feedback evidence | PENDING |
| FPS-013 | controlled impairment tests proving stable video adaptation/hysteresis while preserving control/audio continuity and bounded audio QoS | PENDING |
| FPS-014 | fault-injection matrix including audio source/endpoint loss and restart, with expected/observed recovery behavior | PENDING |
| FPS-015 | clean install, over-install policy, uninstall, runtime packaging and notices verification | PENDING |
| FPS-016 | complete V1 video/audio/connectivity acceptance matrix and release evidence | PENDING |

## Evidence rule

A statement such as “works”, “stable”, “low latency”, or “hardware accelerated” is not qualification evidence by itself. Reports must include the command/test/scenario, observed result, relevant environment, and exact commit being qualified.
