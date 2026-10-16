# Dual-instance localhost baseline — 2026-10-16

**Result: BLOCKED_NOT_IMPLEMENTED for network video.**
**Source:** 23038b26675a991b34da309c2daee8dbbcaf3c05
**Machine:** CDQ-P, Windows 10 19045, Intel Core Ultra 5 245K / Intel Graphics.
**Time:** 08:25–08:37 Asia/Bangkok.

ผู้ใช้ขอทดสอบเปิดโปรแกรมจากสองโฟลเดอร์และใช้คนละพอร์ตบนคอมเครื่องเดียว และยืนยันให้ทำส่วนที่ยังไม่ครบต่อ ผลทดสอบจริงยืนยันว่ารุ่นปัจจุบันยังไม่มี network transport จึงยังส่งภาพหากันไม่ได้ แม้ H.264 local preview จะทำงาน

## Setup and identity

Two portable copies of the existing Release executable, not an in-memory peer simulation:

| Role | PID | Own Share Port | Host / Remote Port |
|---|---:|---:|---|
| A | 16396 | 42801 | 127.0.0.1 / 42802 |
| B | 1500 | 42802 | 127.0.0.1 / 42801 |

Executable SHA256 for both copies: 7F0FAC0675184092A0ACEAC69397ED8D8C40AEE25DC6C216757CC4007F40CA81. Each has its own portable.flag, data/settings.ini and log. No existing user settings were changed.

Test root:
T:\\Space\\Projects\\ProjectsProgram\\FriendPushScreen\\.worktrees\\fps-006-encode-decode-local-loopback\\.superpowers\\dual-instance-localhost-20261016

A/B executable and data copies remain there for reproducibility; the owned test applications were closed after the attempt.

## Actual procedure and results

1. Verified ports 42801 and 42802 were unused, then started two independent processes and showed their windows side by side. The managed launcher initially passed hidden show state; explicitly showing the owned HWNDs resolved this tooling condition without modifying product code.
2. Read the Host/Share Port/Remote Port controls using cross-process WM_GETTEXT and checked separate saved settings.
3. Invoked the real Start Share and Connect UI command handlers in both copies. Both changed to Requested, with Peer: Offline.
4. Checked TCP/UDP endpoints by port and PID. No application-owned endpoint existed at either configured port.
5. Selected an independent owned moving 1920×1080 WGC window in A and started Preview through the real source picker/Preview handlers. A reported H.264 decoded local loop, Intel Quick Sync GPU encoding and Microsoft H264 GPU decoding. B still had no local source and no received frame. Rechecked netstat and port ownership: still no endpoint.
6. The fixture later expired. Both application windows and test fixture processes were cleaned up normally; no user-owned application was terminated.

| Assertion | Result |
|---|---|
| Two separate executables/processes/data stores | PASS |
| Different settings/ports loaded in actual UI | PASS |
| A real local H.264 preview independent of Connect | PASS, functional observation only |
| Application listens/connects on configured network ports | NOT IMPLEMENTED |
| A → B decoded network video | BLOCKED_NOT_IMPLEMENTED |
| B → A / simultaneous network video | BLOCKED_NOT_IMPLEMENTED |
| Encrypted/pinned peer session, reconnect and WAN behavior | NOT IMPLEMENTED / NOT QUALIFIED |

The active-preview observation is retained in baseline-attempt.json at 08:34:26 and is not a sustained performance claim. The screenshot shows the actual Requested/offline connection UI and is not presented as active local preview or remote reception. The active-preview observation is a separate UI-state readback retained in the JSON.

Both logs contain:
- share requested; media pipeline not active
- connect requested; transport not active

## Source explanation

- src/app/application.cpp: ToggleConnectionRequest and ToggleShareRequest only change UI request state and write inactive-transport/pipeline log messages.
- src/ui/ui_state.cpp: ConnectRequested is never made Connected by a network implementation.
- src/ui/settings_panel.cpp: actual connection/share status strings explicitly say the transport/media pipeline is not implemented.
- Existing FPS-006 capture → H.264 encode → bounded queue → decode → render is wholly local to one process.

This failure does not invalidate FPS-006's already qualified local-codec scope. It identifies the absent FPS-008/FPS-009/FPS-010 functionality requested for continued work.

## Evidence and next concrete result

[Evidence directory](../evidence/dual-instance-localhost-20261016/) includes source/binary identity, UI-state and endpoint readbacks, separate settings/logs, screenshots and SHA256 manifest. Port ownership snapshots returned zero endpoints for both configured ports. The captured logs/settings contain no pairing secrets.

[Proposed secure dual-instance video design](../../superpowers/specs/2026-10-16-secure-dual-instance-video-design.md) defines the next architectural increment. It reuses the approved V1 connectivity/security requirements, evaluates Windows-10-compatible transport, and requires real encoded network video plus fault/reconnect tests before network GREEN.

No network implementation or permanent transport decision is claimed in this report. Main integration and release remain separate; existing feature/WIP worktrees and historical results are preserved.
