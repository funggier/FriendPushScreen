# Current Development Task

## V1 Architecture Amendment — Audio + Connectivity

**Status:** GREEN / CLOSED

**Branch:** `fps-v1-audio-connectivity-amendment`

**Started:** 2026-10-15

**Closed:** 2026-10-15

**GitHub Issue:** #7 — V1 Architecture Amendment — Audio + Connectivity

### Outcome

FriendPushScreen V1 is no longer video-only.

Approved V1 architecture now includes:

- optional shared machine audio
- sender `Audio Source` dropdown
- `No Audio`
- Desktop / System Audio through WASAPI loopback
- explicit render/output-device loopback
- Windows capture/input audio endpoints, including capture-card audio when exposed by Windows
- capability-dependent Application Audio
- receiver-local mute/unmute
- receiver-local volume
- LAN connection mode
- Direct IP / DDNS connection mode
- Tailscale connection mode
- Auto connection mode
- one route-independent FriendPushScreen control/video/audio session architecture
- secure Direct Internet requirement
- one-public-UDP-port operational target when the selected transport permits it
- dual-instance localhost/LAN-stack development on one physical Windows machine

### Authoritative amendment

`docs/superpowers/specs/2026-10-15-v1-audio-connectivity-amendment.md`

Where the older V1 master specification conflicts with this amendment, the amendment is authoritative.

### Reference-machine compatibility ruling

Current reference machine:

```text
Windows 10 build 19045
```

Microsoft's documented native process-loopback API requires build 20348+.

Therefore on the current reference machine:

- Desktop/System Audio remains a V1 supported target
- render endpoint loopback remains a V1 supported target
- capture/input device audio remains a V1 supported target
- native Application Audio through the official process-loopback API must report Unsupported

FriendPushScreen must not fake Application Audio support or use undocumented process injection solely to bypass this runtime limit.

### Roadmap effect

- FPS-004 now owns the connection/audio UI state shell
- FPS-006 remains video local loopback
- FPS-006A is inserted for Audio Capture / Encode / Local Loopback
- FPS-008 expands control protocol to audio/connectivity state
- FPS-009 becomes secure media transport for control/video/audio
- FPS-010 qualifies LAN / Direct IP-DDNS / Tailscale
- FPS-011 through FPS-016 include audio/connectivity acceptance where applicable
- existing FPS-001 through FPS-003 GREEN qualification remains unchanged

### Next planned task

`FPS-004 — Viewer UI & Settings Shell`

No development task is ACTIVE after this architecture amendment closes.
