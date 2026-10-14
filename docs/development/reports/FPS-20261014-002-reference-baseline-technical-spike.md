# FPS-20261014-002 — Reference Baseline & Technical Spike

## Status

**Local reference-machine probe: PASS / COMPLETE**

**FPS-002 overall: GREEN / CLOSED — LOCAL REFERENCE QUALIFICATION**

Date: 2026-10-14

Repository: `funggier/FriendPushScreen`

Branch: `fps-002-reference-baseline-spike`

GitHub Issue: #3 — FPS-002 — Reference Baseline & Technical Spike

This report records runtime evidence from the current Windows reference machine. On 2026-10-14 the project qualification scope was explicitly adjusted to accept this local machine as the FPS-002 reference baseline because no second physical Windows PC is available. Missing two-peer evidence is deferred to the later network tasks rather than treated as a failure.

---

## 1. Reference machine

Observed live through LConnect and Windows runtime queries:

| Item | Evidence |
|---|---|
| Host | `CDQ-P` |
| OS | Windows 10 Pro 64-bit |
| Version / build | `10.0.19045` / `19045` |
| CPU | Intel Core Ultra 5 245K |
| Cores / logical processors | 14 / 14 |
| RAM | 33,783,828,480 bytes (~32 GiB) |
| Mainboard | ASRock Z890 Pro RS |
| GPU | Intel(R) Graphics |
| GPU driver | `32.0.101.8826` |
| Active desktop | 2560×1440 @ 59 Hz |
| Physical Ethernet adapter | Realtek Gaming 2.5GbE Family Controller |
| Current physical link | **1 Gbps** |
| Power plan | High performance |

### Implication

This is a valid Intel iGPU reference machine for the V1 Intel path. It does **not** qualify AMD or NVIDIA behavior.

---

## 2. Tailscale baseline

Installed Tailscale:

```text
1.102.4
tailscale commit: 3caf7d9e7dcaba589cfc58beda596929733e4fea
```

Local state:

```text
BackendState: Running
Host: CDQ-P
IPv4: 100.73.92.109
Tailnet: funggier.github
```

`tailscale netcheck` at 2026-10-14 23:13 +07:00:

```text
UDP: true
IPv4: yes
IPv6: no, but OS has support
MappingVariesByDestIP: false
CaptivePortal: false
Nearest DERP: Singapore
Singapore DERP latency: 44.3 ms
```

One peer was visible:

```text
XQ-DC72
online: false
active: false
relay field: sin
last seen: 2026-09-19T14:24:36.1Z
```

### Result

- Local Tailscale networking baseline: **PASS**
- UDP availability: **PASS**
- Stable mapping indication: **PASS**
- Current real Direct-vs-DERP path to a peer: **NOT TESTED / DEFERRED TO FPS-009/FPS-010**
- Two-peer RTT and throughput: **NOT TESTED / DEFERRED TO FPS-009/FPS-010**

Reason: the only visible reference peer was offline during qualification.

---

## 3. Sunshine / Moonlight reference stack

No Sunshine or Moonlight installation was found in:

- command PATH
- common Windows installation paths
- installed-app registry match
- `winget list`

Current `winget search` availability:

```text
Sunshine
  Id: LizardByte.Sunshine
  Version: 2026.914.233613

Moonlight Game Streaming Client
  Id: MoonlightGameStreamingProject.Moonlight
  Version: 6.2.0.0
```

The tools were **not installed during this probe** because the reference peer was offline; installation alone would not produce the required two-machine latency/path benchmark.

Current upstream licensing verified during the spike:

- Sunshine: GPL-3.0-only
- Moonlight Qt project metadata: GPL-3.0+

### Result

Reference-stack end-to-end benchmark: **NOT TESTED / DEFERRED TO FPS-009/FPS-010**

DG-001 and DG-006 remain open.

---

## 4. Reference capture device

Windows currently exposes a USB capture device as:

```text
Friendly name: USB3 Video
Device description: USB Video Device
Hardware ID:
  USB\VID_345F&PID_2131&REV_3100&MI_00
  USB\VID_345F&PID_2131&MI_00
Sibling interfaces:
  USB3 Video
  USB3 Digital Audio
  USB Input Device
  USB Composite Device
```

Windows does not expose an Onten model/manufacturer string for this device. Therefore this report identifies it as:

> **reference USB3 capture device (user identifies the physical device as Onten OTN-332)**

The VID/PID alone is not treated as proof of model identity.

### V1 audio boundary

Video and digital audio are exposed as separate interfaces. V1 can enumerate/use the video interface while leaving the audio interface outside the product feature path.

---

## 5. Capture-device runtime modes

FFmpeg 8.1.2 DirectShow enumeration found `USB3 Video`.

Important advertised modes include both YUYV422 and MJPEG:

| Resolution | Advertised maximum |
|---|---:|
| 1920×1080 | ~60.0002 fps |
| 2048×1080 | ~60.0002 fps |
| 2048×1152 | ~60.0002 fps |
| 1920×1200 | ~60.0002 fps |
| 2560×1440 | 30 fps |
| 2560×1600 | 30 fps |
| 3840×2160 | 18 fps |
| 1280×720 | ~60.0002 fps |

### 5.1 YUYV422 1080p60 read probe

Command shape:

```text
ffmpeg -f dshow -video_size 1920x1080 -framerate 60
       -pixel_format yuyv422 -i video="USB3 Video"
       -t 3 -an -f null NUL
```

Observed:

- process exit: 0
- 163 frames in ~3.01 s
- approximately 54–55 fps during most of the run
- DirectShow realtime buffer warnings and dropped frames

This is a valid capture path, but the default FFmpeg DirectShow buffer is not a production-quality baseline for uncompressed 1080p60.

### 5.2 MJPEG 1080p60 read probe

Command shape:

```text
ffmpeg -f dshow -video_size 1920x1080 -framerate 60
       -vcodec mjpeg -i video="USB3 Video"
       -t 3 -an -f null NUL
```

Observed:

- process exit: 0
- 181 frames in ~3.01 s
- approximately 59–60 fps
- no realtime-buffer overflow in the captured tail

### Result

- Device enumeration: **PASS**
- 1080p60 advertised: **PASS**
- 1080p60 MJPEG capture: **PASS**
- 1080p60 YUYV capture availability: **PASS WITH BUFFER WARNING**
- Native Media Foundation capture path: **NOT TESTED**
- Hot-plug / reconnect: **NOT TESTED**

### Architecture implication

The evidence does not justify making DirectShow the production capture-card backend. The existing preference for Media Foundation remains appropriate and should be tested natively in FPS-005.

---

## 6. FFmpeg / Intel hardware acceleration

FFmpeg runtime:

```text
8.1.2 full build
```

Relevant hardware acceleration methods present:

```text
dxva2
qsv
d3d11va
d3d12va
```

QSV encoders present:

- `h264_qsv`
- `hevc_qsv`
- `av1_qsv`

QSV decoders present:

- `h264_qsv`
- `hevc_qsv`
- `av1_qsv`

### 6.1 Actual 1080p60 hardware encode

Synthetic 1920×1080 @ 60 fps, two-second probes:

| Codec | Result | Frames | Approx processing speed |
|---|---|---:|---:|
| H.264 QSV | PASS | 120 | 2.39× realtime |
| HEVC QSV | PASS | 120 | 2.39× realtime |
| AV1 QSV | PASS | 120 | 2.66× realtime |

These are short synthetic capability probes, not latency benchmarks.

### 6.2 Actual hardware decode

Generated 1080p60 files were decoded with QSV.

- HEVC QSV: PASS
- AV1 QSV: PASS
- H.264 QSV: one transient access-violation occurred during a chained multi-codec probe
- Immediate isolated rerun:
  - software H.264 decode: PASS
  - `h264_qsv` decode: PASS
  - QSV hwaccel decode: PASS
  - D3D11VA decode: PASS

### Warning

The single transient H.264 QSV crash was not reproduced in the immediate isolated rerun. It is recorded as a **stability warning**, not a hardware capability failure. Longer repeated stress qualification is required before the production decoder path is declared stable.

---

## 7. Simultaneous H.264 hardware encode + decode

A five-second simultaneous probe ran:

- 1080p60 H.264 QSV encoding
- 1080p60 H.264 QSV decoding

Observed tails:

```text
encode: 300 frames in ~4.95 s media time, ~1.01× realtime
decode: 300 frames in 5.00 s media time, ~1.01× realtime
```

Windows GPU Engine counters reported process-associated activity while both were running.

Short sampled summed engine utilization:

```text
encode: ~15.6% sampled
decode: ~11.5% sampled
```

These utilization values are diagnostic observations only. They are not performance ceilings because:

- the workload is synthetic
- the run is short
- capture/render/network were not in the same pipeline
- summed Windows engine counters are not equivalent to a single normalized GPU utilization metric

### Result

Simultaneous Intel hardware encode/decode feasibility: **PASS**

---

## 8. Live capture → H.264 QSV probe

Pipeline:

```text
USB3 Video
  → DirectShow MJPEG 1920×1080 @ 60
  → NV12 conversion
  → Intel H.264 QSV encode
  → null sink
```

Five-second observation:

- exit: 0
- 287 frames
- approximately 57 fps final observed rate
- non-monotonic DTS warnings appeared

### Architecture implication

The device + hardware encoder pipeline is feasible, but source timestamp behavior must not be trusted blindly.

FriendPushScreen should own:

- monotonic frame sequence
- normalized timestamps
- bounded queue behavior
- stale-frame dropping

This directly reinforces the master specification's timestamp/bounded-queue requirements.

---

## 9. Windows desktop/window capture primitives

### Windows Graphics Capture

Runtime probe:

```text
GraphicsCaptureSession.IsSupported=True
```

Result: **WGC platform support PASS**

### Desktop Duplication

FFmpeg exposes:

```text
ddagrab — Grab Windows Desktop images using Desktop Duplication API
```

A 60 fps three-second DDA probe opened successfully and exited 0.

Observed rate was roughly 54–56 fps in the short FFmpeg readback probe, with some duplicate/non-monotonic timestamp warnings.

Result: **DDA availability PASS**

This does not establish DDA as better or worse than WGC.

---

## 10. WDA_EXCLUDEFROMCAPTURE API probe

A throwaway WinForms top-level window was created in-process.

Runtime calls:

```text
SetWindowDisplayAffinity(hwnd, 0x11)
GetWindowDisplayAffinity(hwnd)
```

Observed:

```text
SetExcludeFromCapture: True
SetLastError: 0
GetAffinity: True
GetLastError: 0
Affinity: 0x11
```

### Result

`WDA_EXCLUDEFROMCAPTURE` is accepted and readable on this Windows build: **PASS at API level**

### Important limitation

This does **not** prove that the eventual FriendPushScreen WGC or DDA backend will exclude the viewer exactly as required.

A native backend comparison must still verify:

1. viewer visible on local desktop
2. viewer excluded/black in outgoing capture
3. behavior under resize/move/fullscreen
4. behavior while both peers send simultaneously

DG-002 remains OPEN.

---

## 11. Decision-gate status after local spike

### DG-001 — Network video transport

**OPEN / TWO-PEER TEST DEFERRED TO FPS-009/FPS-010**

Evidence gained:

- Tailscale operational
- UDP available
- stable mapping indication
- Sunshine/Moonlight currently not installed
- reference peer offline

No transport integration choice is justified yet.

### DG-002 — Monitor capture backend

**OPEN**

Evidence gained:

- WGC supported by OS/runtime
- DDA available and functional
- WDA_EXCLUDEFROMCAPTURE API accepts 0x11

Missing:

- native WGC vs DDA latency comparison
- actual capture-exclusion comparison
- low-copy/texture-transfer measurements

### DG-003 — Capture-card backend fallback

**OPEN, with evidence supporting the existing Media Foundation preference**

Evidence gained:

- generic UVC-style device enumeration works
- 1080p60 modes are real
- MJPEG DirectShow probe is stable at ~60 fps
- uncompressed DirectShow probe shows buffer pressure
- DirectShow/live encode path shows timestamp anomalies

Native Media Foundation still requires a later implementation spike.

### DG-004 — Hardware codec abstraction

**INTEL SIDE QUALIFIED; CROSS-VENDOR GATE OPEN**

Evidence gained:

- actual H.264/HEVC/AV1 QSV encode
- actual QSV decode
- simultaneous 1080p60 H.264 QSV encode/decode
- live capture → H.264 QSV feasibility

AMD/NVIDIA paths are not tested on this machine.

### DG-005 — UI/application framework

**UNCHANGED / OPEN**

FPS-002 did not gather enough evidence to select the application framework.

### DG-006 — Licensing/compliance boundary

**OPEN**

Evidence reconfirmed:

- Sunshine uses GPL-3.0-only
- Moonlight Qt project metadata reports GPL-3.0+

Any reuse/fork/link decision needs an explicit distribution architecture review before integration.

---

## 12. Deferred two-peer qualification

The local reference-machine phase is complete and is sufficient to close FPS-002 under the project qualification ruling. Real two-peer evidence remains required before network behavior is called qualified, but that evidence now belongs to FPS-009/FPS-010 rather than blocking FPS-002.

Required remaining test:

```text
Sunshine host ↔ Moonlight client
over Tailscale
between two physical machines
```

Must record:

- host/client versions
- Direct vs DERP path
- Tailscale RTT
- stream resolution / FPS / codec
- observed host encode latency if exposed
- observed client decode/render latency if exposed
- bitrate
- packet-loss/recovery behavior
- one-way perceived/measurement baseline
- simultaneous encode/decode only if both sides can be exercised
- CPU/GPU utilization on both machines
- Discord running simultaneously if that is the intended normal use case

Current environment limitation:

> No second physical Windows reference machine is available. The only visible Tailscale reference peer `XQ-DC72` was offline during the spike.

This is no longer a blocker for FPS-002 closure. It is a deferred dependency for the network qualification tasks.

---

## 13. Recommendation

Do **not** install or configure Sunshine/Moonlight solely to satisfy a checkbox while the second peer is offline.

When the second physical machine is online:

1. install/pin the exact Sunshine and Moonlight versions used for the benchmark
2. verify Tailscale peer path before streaming
3. run one-way 1080p60 first
4. collect latency/resource evidence
5. repeat across Direct and DERP if both paths can be induced or observed
6. only then close FPS-002 and use the evidence to constrain FPS-003/FPS-009 decisions

Final FPS-002 state:

```text
FPS-002 = GREEN / CLOSED
Local reference qualification = COMPLETE
Two-peer network qualification = DEFERRED TO FPS-009 / FPS-010
```

This does not change the FriendPushScreen V1 product requirement for real peer-to-peer sharing.
