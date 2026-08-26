# ScreenLink V1 — Architecture, Product Specification, and Full Development Plan

> **สถานะเอกสาร:** Draft Specification / Implementation Blueprint  
> **วันที่จัดทำ:** 2026-08-26  
> **ชื่อโครงการ:** `ScreenLink` เป็นชื่อชั่วคราวสำหรับใช้ในเอกสารนี้ สามารถเปลี่ยนชื่อภายหลังได้  
> **เป้าหมายหลัก:** โปรแกรมแชร์ภาพหน้าจอแบบ Real-time สำหรับ Windows จำนวน 2 คน เน้นภาพคม ลื่น latency ต่ำ ใช้งานง่าย และสามารถแชร์พร้อมกันสองฝั่งได้  
> **ขอบเขต V1:** **Video Only** — ไม่มีระบบเสียงในตัว  
> **ระบบสนทนา:** ใช้ Discord หรือระบบภายนอกต่อไป ไม่สร้าง Voice Chat ใหม่ใน V1

---

# 1. Executive Summary

ScreenLink V1 คือโปรแกรม Windows สำหรับคน 2 คนที่ต้องการดูหน้าจอของกันและกันแบบ real-time โดยไม่ต้องพึ่งคุณภาพ Screen Share ของ Discord

แนวคิดหลักคือแยกงานออกเป็นสองระบบ:

```text
Discord
└─ Voice / Chat

ScreenLink
└─ High-quality low-latency video sharing
```

ผู้ใช้แต่ละฝั่งสามารถเป็นทั้ง:

```text
Sender
+
Viewer
```

พร้อมกันได้

ดังนั้นรูปแบบการใช้งานมี 3 สถานะหลัก:

```text
1. ไม่มีใครแชร์

2. A แชร์ให้ B
   A ─────────► B

3. ทั้งคู่แชร์
   A ─────────► B
   A ◄───────── B
```

ScreenLink V1 จะถูกออกแบบให้ **สองเครื่องมีสถานะเท่าเทียมกัน (peer-to-peer)** ไม่มีแนวคิดว่าเครื่องหนึ่งเป็น Server ถาวรและอีกเครื่องเป็น Client ถาวร

---

# 2. Core Product Principle

## 2.1 สิ่งที่โปรแกรมควรรู้สึกเหมือน

เป้าหมายไม่ใช่สร้าง Discord ใหม่

เป้าหมายคือ:

> เปิดโปรแกรม → เห็นจอเพื่อน → เลือกสิ่งที่ต้องการแชร์ → กด Share

เท่านั้น

โปรแกรมควรให้ความรู้สึกคล้าย “จอเสริมของเพื่อน” มากกว่าเครื่องมือ Remote Desktop ที่เต็มไปด้วยเมนู

---

## 2.2 สิ่งที่สำคัญที่สุด

เรียงตามลำดับความสำคัญ:

1. **Latency ต่ำ**
2. **ภาพคม**
3. **60 FPS เสถียร**
4. **เชื่อมต่อ Direct P2P ให้ได้เมื่อเครือข่ายอนุญาต**
5. **Hardware encode/decode**
6. **ใช้ง่าย**
7. **ไม่เกิด feedback/infinite mirror**
8. **รองรับ Windows 10 และ Windows 11**
9. **ไม่ผูกกับ CPU/GPU ยี่ห้อใด**
10. **รองรับ Capture Card แบบ generic**
11. **Recover จาก network/device failure ได้**
12. **ผู้ใช้ต้องรู้ชัดเจนเสมอว่ากำลัง Share หรือไม่**

---

# 3. V1 Scope

## 3.1 V1 ต้องมี

- Windows 10 / Windows 11 x64
- ใช้งาน 2 คน
- แชร์ฝั่งเดียวได้
- แชร์พร้อมกันสองฝั่งได้
- Viewer แบบ real-time
- Monitor capture
- Window capture
- Capture Card capture
- Generic Windows video capture device support
- Onten OTN-332 เป็น reference capture card สำหรับทดสอบ
- Hardware encoding
- Hardware decoding
- Intel GPU
- AMD GPU
- NVIDIA GPU
- Intel CPU
- AMD CPU
- Software fallback เมื่อ hardware acceleration ใช้ไม่ได้
- H.264 compatibility baseline
- HEVC เมื่อทั้งสองฝั่งรองรับและเหมาะสม
- AV1 เป็น optional capability เมื่อ hardware ทั้งสองฝั่งพร้อม
- Source Auto Detection
- Capture Card hot-plug detection
- Share / Stop Share ปุ่มเดียว
- Always on Top
- Borderless Viewer
- Fullscreen
- Hide/Show Settings ด้วย keyboard shortcut
- Settings overlay เกือบเต็มหน้าต่าง
- Settings scroll ได้
- Outgoing live preview
- สถานะของเรา
- สถานะของเพื่อน
- สถานะ Direct / Relay
- Latency/RTT
- reconnect อัตโนมัติ
- feedback-loop protection อัตโนมัติ
- จำ settings ล่าสุด
- **ไม่ Auto Share หลังเปิดโปรแกรมใหม่**

---

## 3.2 สิ่งที่ตั้งใจไม่ทำใน V1

เพื่อป้องกัน scope creep สิ่งต่อไปนี้ **ห้ามดึงเข้ามาใน V1** เว้นแต่พบว่าเป็น prerequisite จริง:

- Audio streaming
- Microphone
- Voice chat
- Echo cancellation
- Audio from capture card
- System audio
- Remote keyboard control
- Remote mouse control
- Gamepad forwarding
- File transfer
- Clipboard synchronization
- Text chat
- User account/cloud account system
- Public matchmaking
- 3+ users
- Recording
- Replay buffer
- Streaming to Twitch/YouTube
- OBS-style scene composition
- Webcam composition
- HDR เป็น requirement บังคับ
- Mobile client
- macOS/Linux support
- Browser viewer
- Cloud video relay service ของเราเอง

สิ่งเหล่านี้สามารถพัฒนาใน V2+ ได้

---

# 4. Target Platform

## 4.1 Operating System

Baseline:

```text
Windows 10 Version 2004+
Windows 11
x64
```

เหตุผลที่ใช้ Windows 10 Version 2004 เป็น baseline:

- Windows Graphics Capture รองรับ Win32 HWND/HMONITOR ตั้งแต่ Windows 10 Version 1903
- `WDA_EXCLUDEFROMCAPTURE` มีอย่างเป็นทางการตั้งแต่ Windows 10 Version 2004
- ทำให้ feedback-protection strategy มี baseline ที่ชัดเจน

Windows 10 และ Windows 11 ต้องใช้ **binary/build เดียวกัน** ให้มากที่สุด

ไม่ควรมี:

```text
ScreenLink-Win10.exe
ScreenLink-Win11.exe
```

ควรเป็น:

```text
ScreenLink.exe
```

แล้วตรวจ feature ตอน runtime

---

# 5. Hardware Compatibility Requirement

## 5.1 CPU

รองรับ:

```text
Intel x64 CPU
AMD x64 CPU
```

CPU vendor ต้องไม่มีผลต่อ protocol หรือ compatibility ระหว่าง peer

ตัวอย่างทั้งหมดนี้ต้องใช้ได้:

```text
Intel CPU ↔ AMD CPU
AMD CPU   ↔ Intel CPU
Intel CPU ↔ Intel CPU
AMD CPU   ↔ AMD CPU
```

CPU feature เช่น SSE/AVX/AVX2 ต้องตรวจ runtime หากใช้ optimization

ห้ามบังคับ AVX2 หากไม่จำเป็น

---

## 5.2 GPU

ต้องเป็น vendor-neutral

รองรับ:

- Intel iGPU
- Intel Arc
- AMD iGPU/APU
- AMD Radeon dGPU
- NVIDIA GeForce
- ระบบ multi-GPU

ตัวอย่าง combination ที่ต้องใช้ได้:

| CPU | GPU | Expected Encoder |
|---|---|---|
| Intel | Intel iGPU | Intel hardware encoder |
| Intel | AMD Radeon | AMD hardware encoder |
| Intel | NVIDIA | NVENC |
| AMD | AMD Radeon | AMD hardware encoder |
| AMD | NVIDIA | NVENC |
| AMD | Intel Arc | Intel hardware encoder |

**CPU vendor และ GPU vendor ต้องแยก detection ออกจากกัน**

---

# 6. Video Codec Policy

## 6.1 Codec Negotiation

โปรแกรมต้องตรวจ capability ของทั้งสองฝั่งก่อนเริ่ม stream

ตัวอย่าง:

```text
Peer A:
H.264 Encode ✓
HEVC  Encode ✓
AV1   Encode ✓

Peer B:
H.264 Decode ✓
HEVC  Decode ✓
AV1   Decode ✕

Result:
HEVC
```

หลักการเลือก:

```text
1. หา codec ที่ Sender hardware encode ได้
2. ตรวจว่า Receiver hardware decode ได้
3. ตรวจ latency/performance
4. เลือก codec ที่เหมาะที่สุด
```

ไม่ใช่เลือก “codec ใหม่ที่สุด” เสมอ

---

## 6.2 Baseline

**H.264 ต้องเป็น compatibility baseline**

เหตุผล:

- hardware decode รองรับกว้าง
- เหมาะสำหรับ fallback
- เป็น codec ที่ต้องใช้เพื่อให้เครื่องหลากหลายรุ่นเชื่อมกันได้

---

## 6.3 HEVC

HEVC เป็น preferred option เมื่อ:

- Sender hardware encode ได้
- Receiver hardware decode ได้
- driver ทำงานเสถียร
- latency อยู่ในเกณฑ์ที่ดี

เหมาะกับ 1080p60 ที่ต้องการภาพคมโดยใช้ bitrate ไม่สูงเกินไป

---

## 6.4 AV1

AV1 ไม่ควรเป็น hard requirement ของ V1

ใช้เฉพาะเมื่อ:

```text
Sender hardware encode AV1 ✓
Receiver hardware decode AV1 ✓
Measured latency acceptable ✓
```

ห้าม fallback ไป software AV1 แบบหนัก CPU โดยอัตโนมัติสำหรับ real-time V1

---

# 7. Video Quality Target

ค่าเริ่มต้น V1:

```text
Resolution: 1920×1080
Frame Rate: 60 FPS
Codec: Auto
Bitrate: Auto / ประมาณ 15–30 Mbps ตาม codec และ network
Color: SDR
```

Preset ที่ควรมี:

```text
Auto
720p60
1080p30
1080p60
Custom
```

1440p และ 4K อาจเปิดได้ใน Advanced เมื่อ pipeline พร้อม แต่ไม่ใช่ Definition of Done ของ V1

---

# 8. Capture Sources

Source dropdown ต้องรวม source ทุกประเภทไว้ในเมนูเดียว

ตัวอย่าง:

```text
Screens
  Monitor 1
  Monitor 2

Windows
  Firefox — YouTube
  MetaTrader 5 — EURUSD
  Visual Studio Code — Project

Devices
  Onten OTN-332
  USB Video Capture
```

ผู้ใช้ไม่ควรต้องเลือก “ประเภท” ก่อนแล้วค่อยเลือก source

เลือก object ที่ต้องการแชร์ได้โดยตรง

---

# 9. Monitor Capture

## 9.1 Target

รองรับจอจริงทุกจอที่ Windows มองเห็น

ต้อง enumerate:

- HMONITOR
- display name
- resolution
- refresh rate
- DPI scaling
- desktop coordinates
- primary display state

---

## 9.2 Capture Backend

Windows มีสองแนวทางสำคัญ:

### DirectX Desktop Duplication (DXGI/DDX)

ข้อดี:

- mature
- latency ต่ำ
- ใช้กับ desktop capture ได้ดี
- Sunshine ใช้ backend `ddx` บน Windows

### Windows.Graphics.Capture (WGC)

ข้อดี:

- capture Window โดยตรงได้
- capture Monitor ได้
- Win32 interop รองรับ HWND/HMONITOR
- เหมาะกับ source abstraction ของโปรแกรมเรา

V1 ควรสร้าง abstraction:

```text
ICaptureSource
    │
    ├── MonitorCapture
    ├── WindowCapture
    └── CaptureCardSource
```

แล้วเลือก backend ภายใน

---

# 10. Window Capture

Windows Graphics Capture เหมาะสำหรับ window source

ต้องรองรับ:

- enumerate top-level windows
- icon
- process name
- window title
- thumbnail
- exclude invisible/cloaked/tool windows ตามความเหมาะสม
- track resize
- track minimize
- track close/reopen

กรณี window ปิด:

```text
Source lost
→ Pause outgoing video
→ แจ้งผู้ใช้
→ อย่าสลับ source เองโดยไม่บอก
```

---

# 11. Capture Card Support

## 11.1 Design Principle

ห้ามเขียน code ผูกกับ:

```text
if device == "Onten OTN-332"
```

ต้องสร้าง generic capture-device layer

เช่น:

```text
IVideoInputDevice
```

---

## 11.2 Onten OTN-332

อุปกรณ์ของผู้ใช้สำหรับ reference testing:

```text
Onten OTN-332
USB-C uplink
HDMI input
HDMI output
3.5 mm audio interfaces
```

V1 สนใจ **Video เท่านั้น**

ห้ามนำ audio ของ capture card เข้ามาใน V1 แม้ hardware จะรองรับ

---

## 11.3 Device Enumeration

โปรแกรมต้องถาม Windows ว่า device รองรับ mode อะไรจริง

ต้อง enumerate:

- device symbolic link / stable ID
- friendly name
- resolutions
- FPS
- pixel formats
- frame interval
- color format ถ้าหาได้

ตัวอย่าง media modes:

```text
1920×1080 @ 60
1920×1080 @ 30
1280×720  @ 60
...
```

**ห้าม hard-code specification จากเว็บ**

ข้อมูลจาก device runtime คือ source of truth

---

## 11.4 Capture API

สำหรับ Windows capture devices ให้ประเมิน:

- Media Foundation
- DirectShow fallback หากมี device compatibility issue

Preferred architecture:

```text
Capture Card
     ↓
Media Foundation
     ↓
D3D11 / video surface
     ↓
Video Pipeline
```

เป้าหมายคือหลีกเลี่ยง CPU copy เมื่อทำได้

---

## 11.5 Hot Plug

ต้อง detect:

```text
Device added
Device removed
Device reconnected
```

หาก OTN-332 ถูกถอดระหว่าง Share:

```text
Capture device disconnected
Share state = Paused
Connection ยังคงอยู่
```

เมื่อเสียบกลับและ stable device ID ตรงกัน:

```text
Resume capture
```

แต่ควรแจ้งผู้ใช้ก่อน/ระหว่างสถานะ

---

# 12. Unified Video Frame Pipeline

เป้าหมายคือทุก source ลงท้ายเป็น abstraction เดียว

```text
Monitor
Window
Capture Card
    │
    ▼
Unified Frame
    │
    ▼
Optional Processing
    │
    ▼
Encoder
```

Frame object ควรเก็บอย่างน้อย:

```text
timestamp
width
height
pixel format
GPU resource / texture
source type
frame sequence
color metadata
```

---

# 13. Zero/Low Copy Principle

หนึ่งในจุดที่ต้องใส่ใจมากที่สุดคือการหลีกเลี่ยง:

```text
GPU
↓
CPU RAM
↓
GPU
```

ทุก frame

1080p60 raw frame มีข้อมูลจำนวนมาก

ควรพยายาม:

```text
Capture GPU Texture
        ↓
GPU Processing
        ↓
Hardware Encoder
```

หาก backend รองรับ

ต้อง profile จริง ไม่สมมติว่า zero-copy สำเร็จเพียงเพราะใช้ D3D11

---

# 14. Feedback / Infinite Mirror Protection

## 14.1 Problem

เมื่อแชร์ Desktop และดูจอเพื่อนในหน้าต่างเดียวกัน:

```text
A เห็น B
↓
A แชร์ desktop
↓
B เห็น A ที่กำลังเห็น B
↓
recursive mirror
```

ถ้าทั้งสองแชร์พร้อมกันปัญหาจะรุนแรงขึ้น

---

## 14.2 Desired Behavior

จอผู้ใช้จริง:

```text
┌───────────────────────────────┐
│ Desktop                       │
│            ┌───────────────┐  │
│            │ Friend Viewer │  │
│            └───────────────┘  │
└───────────────────────────────┘
```

Outgoing stream:

```text
┌───────────────────────────────┐
│ Desktop                       │
│            ┌───────────────┐  │
│            │     BLACK     │  │
│            └───────────────┘  │
└───────────────────────────────┘
```

---

## 14.3 Strategy A — Window Display Affinity

Windows มี:

```text
WDA_MONITOR
WDA_EXCLUDEFROMCAPTURE
```

`WDA_EXCLUDEFROMCAPTURE` มีตั้งแต่ Windows 10 Version 2004

ข้อจำกัดสำคัญ:

- Window ต้องเป็น top-level window
- HWND ต้อง belong to current process ที่เรียก API
- Microsoft ระบุว่า behavior ขึ้นกับ public capture APIs ที่รองรับ
- ไม่ควรถือเป็น security guarantee

ข้อดี:

ถ้า Viewer เป็น window ของ ScreenLink เอง เราสามารถตั้ง affinity ให้ window เราได้โดยตรง

---

## 14.4 Strategy B — Outgoing Black Mask

เพื่อให้ behavior predictable กว่า ควรมี fallback ที่เรา control เอง:

```text
Captured Frame
    ↓
Get Viewer Visible Rect
    ↓
Mask region to black
    ↓
Encode
```

นี่ควรถือเป็น **core V1 feedback fallback**

---

## 14.5 Strategy C — Exclude Viewer Before Capture

หาก WDA_EXCLUDEFROMCAPTURE ทำงานกับ backend ที่ใช้ได้ดี อาจใช้เป็น optimized path

แต่ต้องมี automated integration test

ห้าม assume

---

## 14.6 Mask Behavior

V1 default:

```text
Viewer Feedback Mask = BLACK
```

ไม่ต้องมี Blur/Freeze/Placeholder ใน V1

Black มีข้อดี:

- ง่าย
- deterministic
- encode ได้ง่าย
- ลด bitrate บริเวณที่เคยเป็น moving stream

---

## 14.7 Z-Order Complexity

กรณี window อื่นทับ viewer:

```text
Viewer
   + Notepad on top
```

simple rectangular mask อาจทำให้ส่วนของ Notepad ที่ทับ Viewer กลายเป็นดำด้วย

V1 สามารถยอมรับ behavior นี้ได้หาก documented

หลัง V1 ค่อยทำ visible-region calculation

**อย่าเพิ่มความซับซ้อน z-order clipping ก่อนพิสูจน์ pipeline หลัก**

---

# 15. Capture Card and Feedback

ถ้า Source เป็น Capture Card:

```text
HDMI
↓
Capture Card
↓
Outgoing Video
```

ภาพ Viewer บน Windows Desktop ไม่ได้อยู่ใน source

ดังนั้น:

```text
Feedback Mask = Not Required
```

โปรแกรมควรทราบจาก source type และไม่เสีย processing โดยไม่จำเป็น

---

# 16. Viewer

Viewer คือพื้นที่หลักของโปรแกรม

Default mode:

```text
┌─────────────────────────────────────┐
│                                     │
│                                     │
│         FRIEND VIDEO                │
│                                     │
│                                     │
└─────────────────────────────────────┘
```

เป้าหมายคือให้ video ใช้พื้นที่เกือบทั้งหมด

---

# 17. Viewer Scaling

ต้องมีอย่างน้อย:

```text
Fit
Fill
1:1
```

Default:

```text
Fit
```

รักษา aspect ratio

พื้นที่ที่เหลือเป็นสีดำ

ไม่ stretch ภาพจนผิดสัดส่วน

---

# 18. Viewer Modes

## 18.1 Normal Viewer

หน้าต่าง Windows ปกติ

## 18.2 Borderless Viewer

ไม่มี toolbar/settings

เหมาะกับ Always on Top

## 18.3 Fullscreen

เต็ม monitor

เข้าด้วย:

```text
F11
Double Click
```

ออกด้วย:

```text
Esc
```

---

# 19. Always on Top

ต้องเป็น core viewer feature

Behavior:

```text
OFF
→ normal z-order

ON
→ topmost window
```

ต้องจำ state ข้ามการเปิดโปรแกรม

แต่ Share state **ห้ามจำเป็น active**

---

# 20. Keyboard Shortcuts

ค่า default ที่เสนอ:

| Action | Shortcut |
|---|---|
| Hide/Show Control & Settings | `Tab` |
| Fullscreen | `F11` |
| Exit Fullscreen | `Esc` |
| Always on Top | `Ctrl+Shift+T` |
| Share / Stop Share | `Ctrl+Shift+S` |

Hotkey ต้องเปลี่ยนได้ใน Settings

Global hotkey สำหรับ Share/Stop ต้องพิจารณา conflict กับโปรแกรมอื่น และไม่ควร register ถ้า user ปิด feature

---

# 21. Main UI State

## 21.1 Viewer Mode

ปกติจะเห็นเฉพาะ video

ไม่มี bottom bar ถาวร

---

## 21.2 Settings / Control Overlay

กด `Tab`

Settings overlay เกือบเต็มพื้นที่

stream ที่รับจากเพื่อน:

- connection ยังทำงาน
- decode ต่อ
- state ไม่หาย
- ไม่ reconnect

เพียงแต่ Settings overlay ปิดทับภาพ

---

# 22. Settings Layout

Settings ควร scroll ได้ทั้งหน้า

โครง:

```text
Share
Video
Viewer
Network
Hotkeys
Advanced
```

แต่ละ section collapse/expand ได้

ตัวอย่าง:

```text
▼ Share
▼ Video
▶ Viewer
▶ Network
▶ Hotkeys
▶ Advanced
```

---

# 23. Settings Top Section

บนสุดต้องเป็นสิ่งที่ใช้บ่อยที่สุด:

```text
Source
Live Preview
Share / Stop Share
Peer Status
Connection Status
```

---

# 24. Outgoing Live Preview

Preview ต้องแสดง:

> “สิ่งที่อีกฝ่ายจะได้รับจากเรา”

ไม่ใช่ raw capture ก่อน processing

ดังนั้น:

```text
Capture
↓
Feedback mask
↓
Scale/convert
↓
Outgoing Preview
↓
Encode
```

Preview ควรสะท้อนภาพ **หลัง feedback protection**

ถ้า Viewer ถูก black mask:

Preview ต้องเห็น black mask ด้วย

---

# 25. Source Dropdown

ต้องแบ่ง group:

```text
Screens
Windows
Devices
```

Window item ควรมี:

- application icon
- title
- optional thumbnail

Capture Card item ควรมี device name

---

# 26. Share Button

ปุ่มเดียว สลับ state

```text
Not Sharing
[ Share ]
```

เมื่อ active:

```text
Sharing
[ Stop Sharing ]
```

ห้ามมี Share และ Stop เป็นคนละปุ่ม

---

# 27. Share Consent / Safety

เมื่อเปิดโปรแกรมใหม่:

```text
Share = OFF
```

เสมอ

แม้ปิดโปรแกรมครั้งก่อนตอนกำลังแชร์

โปรแกรมสามารถจำ:

- source ล่าสุด
- quality
- codec preference
- bitrate
- always on top

แต่ห้ามจำ:

```text
sharing = true
```

เป็น startup state

---

# 28. Status Indicators

ต้องมีสถานะของเราและเพื่อน

ตัวอย่าง:

```text
You
○ Not sharing
● Sharing
! Source lost

Friend
○ Offline
● Online
● Sharing
! Connection problem
```

---

# 29. Network Status

ต้องแสดงอย่างน้อย:

```text
Direct
Relay
Connecting
Offline
```

และ latency เช่น:

```text
Direct · 12 ms
```

รายละเอียดเพิ่มใน Network section:

- RTT
- connection path
- bitrate
- packet loss
- reconnect count
- peer address (Advanced)
- transport state

---

# 30. Networking Strategy for V1

## 30.1 Recommended Initial Transport Environment

ใช้ Tailscale เป็น overlay network ใน V1 prototype/product integration ระยะแรก

เหตุผล:

- WireGuard encryption
- NAT traversal
- Direct P2P เมื่อทำได้
- ไม่ต้องเปิด Sunshine/service port สู่ public Internet โดยตรง
- รับมือ CGNAT ได้ดีกว่าการพึ่ง port forwarding อย่างเดียว
- มี diagnostics สำหรับ Direct/Relay

---

# 31. Tailscale Connection Types

ต้องแยกให้ชัด:

```text
Direct
Peer Relay
DERP Relay
```

ลำดับ preference:

```text
Direct
  ↓
Peer Relay
  ↓
DERP
```

สำหรับ video streaming:

```text
Direct = Preferred
Peer Relay = Acceptable fallback หาก throughput/latency ดี
DERP = Connective fallback แต่ไม่ควรถือว่าคุณภาพพร้อมสำหรับ high-bitrate stream
```

---

# 32. Direct P2P Requirement

เพียง “ping ถึง” ไม่พอ

ก่อนเริ่ม quality สูง โปรแกรมควรตรวจ connection path

diagnostic equivalent:

```text
tailscale status
tailscale ping <peer>
tailscale netcheck
```

ถ้าเป็น Relay:

UI ต้องแสดงให้ผู้ใช้รู้

ห้ามขึ้นสีเขียวเหมือน Direct

---

# 33. Network Auto Quality

Auto Quality ควรดู:

- available throughput
- RTT
- packet loss
- jitter
- decode queue
- encoder output
- dropped frames

อย่าปรับ bitrate จาก speed test เพียงอย่างเดียว

V1 อาจเริ่มจาก simple adaptive rule ก่อน

---

# 34. Example Network Budget

1080p60 HEVC ที่ 20 Mbps:

กรณีแชร์ฝั่งเดียว:

```text
Sender:
Upload ~20 Mbps

Receiver:
Download ~20 Mbps
```

กรณีแชร์สองทาง:

```text
Peer A:
Upload ~20
Download ~20

Peer B:
Upload ~20
Download ~20
```

ต้องมี headroom

ไม่ควรตั้ง bitrate เท่ากับ maximum measured upload

---

# 35. Network Failure Behavior

กรณี network หลุด:

```text
Streaming
↓
Connection lost
↓
Viewer displays Reconnecting
↓
Attempt reconnect
```

**ห้ามปิดโปรแกรม**

ถ้า reconnect สำเร็จ:

```text
control connection
↓
capability check if needed
↓
resume stream
```

หาก sender ยังคง Share อยู่

---

# 36. Peer Presence

V1 ต้องรู้สถานะอย่างน้อย:

```text
Peer Offline
Peer Online
Peer Sharing
```

วิธี implement อาจอาศัย:

- overlay-network device presence
- control channel heartbeat
- stream session state

ควรเชื่อ control state มากกว่า UI assumption

---

# 37. Control Channel vs Video Channel

อย่าผูกทุกอย่างไว้ stream socket เดียวแบบไม่มี state separation

ควร conceptualize:

```text
Control Channel
├─ presence
├─ share state
├─ source metadata
├─ codec negotiation
├─ capability negotiation
├─ reconnect
└─ errors

Video Channel
└─ encoded frames
```

ถ้า video pipeline restart:

control channel ควรยังอยู่

---

# 38. Capability Handshake

เมื่อ peer เชื่อม:

```text
OS
App version
Protocol version
GPU encoder capabilities
GPU decoder capabilities
Codec profiles
Max resolution
Max FPS
Feature flags
```

ตัวอย่าง:

```json
{
  "protocol": 1,
  "video": {
    "decode": ["h264", "hevc"],
    "max_width": 3840,
    "max_height": 2160,
    "max_fps": 120
  }
}
```

ข้อมูลจริงค่อยกำหนด schema ตอน implementation

---

# 39. Protocol Versioning

ตั้งแต่ V1 ต้องมี:

```text
protocol_version
```

อย่าอาศัย app version อย่างเดียว

อนาคต:

```text
App 1.2
Protocol 1
```

อาจยังคุยกับ:

```text
App 1.5
Protocol 1
```

ได้

---

# 40. Hardware Detection

ตอน startup:

```text
System Probe
│
├─ Windows version/build
├─ CPU
├─ GPU adapters
├─ hardware encoders
├─ hardware decoders
├─ displays
├─ capture devices
└─ network state
```

ผลลัพธ์เก็บใน Capability Registry

---

# 41. Encoder Abstraction

ควรมี interface ลักษณะ:

```text
IVideoEncoder
    ├── Intel
    ├── AMD
    ├── NVIDIA
    └── Software
```

application layer ห้ามรู้ implementation detail มากเกินไป

---

# 42. Decoder Abstraction

เช่น:

```text
IVideoDecoder
    ├── D3D11/DXVA hardware
    └── Software fallback
```

prefer hardware

---

# 43. Encoder Selection

Auto selection:

```text
1. enumerate GPU
2. test encoder availability
3. test requested codec
4. initialize short probe
5. select stable hardware path
6. fallback if init fails
```

อย่าถือว่า driver advertise = ใช้งานจริงได้

---

# 44. Software Encoding Fallback

ใช้เมื่อ:

- hardware encoder ไม่มี
- driver failure
- unsupported codec
- temporary init failure

หาก fallback:

UI ต้องแสดง:

```text
Hardware encoding unavailable
Using CPU encoding
```

และอาจลด:

```text
FPS
resolution
```

เพื่อรักษา real-time behavior

---

# 45. Frame Timing

ทุก frame ต้องมี monotonic timestamp

หลีกเลี่ยง wall-clock สำหรับ internal timing

ต้องวัด:

```text
capture time
processing time
encode queue time
network send time
receive time
decode time
render time
```

เพื่อนำไปหาจุด latency

---

# 46. Backpressure

ห้ามปล่อย queue โตไม่จำกัด

สำหรับ real-time video:

> frame ใหม่มักมีค่ามากกว่า frame เก่าที่ค้างอยู่

หาก encoder/network ตามไม่ทัน:

ควร drop stale frames ตาม policy

ไม่ควรสร้าง latency 2–3 วินาทีด้วยการสะสม queue

---

# 47. Latency Engineering Rule

Priority:

```text
Low latency
> no frame drop at all
```

สำหรับ screen sharing real-time

ตัวอย่าง:

```text
Dropped 1 frame
```

ดีกว่า:

```text
display delayed 500 ms
```

---

# 48. Renderer

Windows viewer ควรใช้ GPU rendering

เช่น D3D11 texture path

ต้องรองรับ:

- resizing window
- DPI scaling
- fullscreen
- aspect ratio
- vsync/present strategy
- device lost
- monitor change

---

# 49. UI Framework Recommendation

### C++ + Qt 6

เหมาะเพราะ:

- Moonlight PC ใช้ Qt
- Win32 interop ได้
- Scroll Settings ง่าย
- Always on Top ง่าย
- Borderless/fullscreen ง่าย
- cross-component state management ทำได้ดี
- C++ เข้ากับ D3D11/Media Foundation/native video stack

อย่างไรก็ตาม UI framework ต้องไม่ครอบ video core จน core test ไม่ได้

โครง:

```text
Qt UI
  ↓
Application State
  ↓
Core Services
```

---

# 50. Proposed Module Architecture

```text
screenlink/
│
├─ app/
│  ├─ main
│  ├─ lifecycle
│  └─ application_state
│
├─ ui/
│  ├─ viewer
│  ├─ settings_overlay
│  ├─ source_picker
│  ├─ status
│  └─ diagnostics
│
├─ platform/windows/
│  ├─ window_enum
│  ├─ monitor_enum
│  ├─ device_notifications
│  ├─ display_affinity
│  └─ hotkeys
│
├─ capture/
│  ├─ capture_source
│  ├─ monitor_capture
│  ├─ window_capture
│  └─ capture_card
│
├─ video/
│  ├─ frame
│  ├─ compositor
│  ├─ feedback_mask
│  ├─ encoder
│  ├─ decoder
│  └─ renderer
│
├─ network/
│  ├─ peer
│  ├─ control_channel
│  ├─ transport
│  ├─ capability
│  └─ quality_controller
│
├─ integration/
│  ├─ tailscale
│  ├─ sunshine
│  └─ moonlight
│
├─ config/
│  ├─ settings
│  └─ migration
│
├─ diagnostics/
│  ├─ metrics
│  ├─ logging
│  └─ crash_report
│
└─ tests/
```

โครงจริงสามารถเปลี่ยนตาม repository ที่เลือก

---

# 51. Sunshine / Moonlight Role

Sunshine/Moonlight เป็น reference stack ที่มีคุณค่ามากเพราะแก้ปัญหา:

- low-latency streaming
- hardware encoding
- hardware decoding
- GameStream transport/session behavior
- Windows host/client experience

แต่ต้องตัดสินใจให้ชัดว่าเราจะใช้แบบใด

---

# 52. Integration Options

## Option A — External Reference Stack

```text
ScreenLink UI
├─ launches/manages Sunshine
└─ launches/manages Moonlight
```

ข้อดี:

- prototype เร็ว
- validate UX/network ได้เร็ว
- upstream update ง่ายกว่า fork

ข้อเสีย:

- UI integration จำกัด
- custom capture source เช่น Capture Card ยาก
- black-mask integration จำกัด
- หลาย process
- user experience ไม่ใช่ single native app เต็มรูปแบบ

เหมาะสำหรับ **prototype / feasibility phase**

---

## Option B — Fork Sunshine + Moonlight

ข้อดี:

- ใช้ engine ที่ mature
- ปรับ sender/receiver ได้
- ทำ feedback masking ได้ลึกขึ้น
- ปรับ UI/client behavior ได้

ข้อเสีย:

- maintenance burden
- upstream merges
- GPL-3.0 obligations
- integration complexity
- build system ใหญ่

เหมาะเมื่อพิสูจน์แล้วว่า reuse code คุ้มกว่าสร้าง core เอง

---

## Option C — Native ScreenLink Streaming Core

สร้าง:

```text
Capture
Encoder
Transport
Decoder
Renderer
```

เองโดยใช้ Windows/video SDK

ข้อดี:

- architecture ตรง requirement
- Capture Card เป็น first-class source
- UI/control เต็มที่
- feedback protection ง่ายกว่า
- ไม่ผูก GameStream

ข้อเสีย:

- network transport/congestion/jitter/NAT ยากมาก
- ใช้เวลาพัฒนาสูง
- ต้องทดสอบ hardware หลายรุ่น
- risk สูงกว่า

**ห้ามเลือก Option C เพียงเพราะ “เขียนเองสะอาดกว่า” ก่อนทำ prototype benchmark**

---

# 53. Recommended Development Strategy

ใช้ **Two-Track Strategy**

## Track 1 — Reference Validation

Sunshine + Moonlight + Tailscale

เป้าหมาย:

พิสูจน์ experience จริงระหว่างเครื่อง 2 เครื่อง

## Track 2 — Productization

หลังได้ตัวเลขจริง ตัดสินใจ:

```text
External integration
vs
GPL fork
vs
Native core
```

ด้วยข้อมูล ไม่ใช่การคาดเดา

---

# 54. Licensing Gate — สำคัญมาก

Sunshine และ Moonlight PC ปัจจุบันเผยแพร่ภายใต้ GPL-3.0

ดังนั้นก่อน:

- copy source
- link code
- modify/fork
- redistribute binaries

ต้องทำ license review

เอกสารนี้ **ไม่ใช่คำปรึกษากฎหมาย**

Decision Gate ต้องเกิดก่อน Productization Phase

คำถามที่ต้องตอบ:

```text
1. เรา distribute Sunshine/Moonlight หรือให้ user ติดตั้งเอง?
2. เรา modify source หรือไม่?
3. เรา link GPL code เข้ากับ executable หรือแค่ IPC?
4. ScreenLink จะ open source หรือไม่?
5. codec patent/licensing มีผลอย่างไรถ้าจำหน่าย?
```

อย่ารอจนโปรแกรมเสร็จแล้วค่อยคิดเรื่องนี้

---

# 55. Configuration Storage

ควรเก็บ user config ใน:

```text
%LOCALAPPDATA%\ScreenLink\
```

หรือ path ตาม Windows conventions

config ต้องมี version:

```json
{
  "config_version": 1
}
```

เพื่อ migrate ในอนาคต

---

# 56. What Settings Persist

จำ:

- last source
- quality
- bitrate preference
- codec preference
- always on top
- viewer mode
- hotkeys
- window size/position

ไม่จำเป็น active:

- Share ON
- temporary error
- current session ID

---

# 57. Logging

ต้องมี structured log

ระดับ:

```text
INFO
WARN
ERROR
DEBUG
```

ห้าม log:

- raw video frames
- sensitive screen contents
- passwords/tokens
- private network credentials

ควร log:

```text
capture backend
device ID (safe form)
encoder selected
codec
resolution/FPS
connection type
RTT
error code
reconnect events
```

---

# 58. Diagnostics View

Advanced diagnostics:

```text
OS
CPU
GPU
Encoder
Decoder
Capture backend
Source
Codec
Resolution
FPS
Encode FPS
Dropped frames
Bitrate
RTT
Packet loss
Direct/Relay
```

มีปุ่ม:

```text
Copy Diagnostics
```

โดย redact sensitive data

---

# 59. Error Handling Philosophy

ทุก subsystem ต้องมี state machine

ห้ามใช้ logic แบบ:

```text
ถ้า error → exit
```

สำหรับ recoverable errors

ตัวอย่าง:

```text
Capture:
Idle
Starting
Running
SourceLost
Recovering
Stopped
Failed
```

Network:

```text
Offline
Connecting
Connected
Degraded
Reconnecting
Failed
```

---

# 60. Device Lost / GPU Reset

ต้องรองรับ:

- D3D device removed
- monitor topology change
- driver reset
- capture device removed

หลัก:

```text
detect
↓
tear down affected pipeline only
↓
recreate
↓
resume if possible
```

ไม่ restart ทั้ง application หากไม่จำเป็น

---

# 61. Threading Model

อย่าให้ UI thread ทำ:

- capture
- encode
- decode
- network I/O

conceptual threads/tasks:

```text
UI
Control
Capture
Encode
Network send
Network receive
Decode
Render
Diagnostics
```

ไม่จำเป็นต้อง 1 OS thread ต่อ module แต่ ownership ต้องชัด

---

# 62. Queue Design

ต้อง bounded

ตัวอย่าง:

```text
Capture → Encoder queue: 1–3 frames max
Receive → Decoder: bounded
Decoder → Renderer: latest frame oriented
```

เป้าหมายป้องกัน latency accumulation

---

# 63. Security Baseline

V1 ต้องมี:

- encrypted transport
- peer authentication
- explicit pairing/trust
- no auto-share
- no public unauthenticated listener
- safe config storage
- no embedded static secret
- logs ไม่เก็บ screen contents

Tailscale ช่วย network identity/encryption แต่ application control channel ก็ควรมี peer identity/state ของตัวเอง

---

# 64. Pairing UX

V1 อาจเริ่มจากการให้สองเครื่องอยู่ใน Tailscale network เดียวกัน

ScreenLink แสดง peer ที่อนุญาต

ภายหลังค่อยทำ invite code

อย่าออกแบบ cloud account ก่อนจำเป็น

---

# 65. Privacy Indicator

เมื่อ Share:

ต้องมี visual state ชัดเสมอ

แม้ Settings ถูกซ่อน อาจใช้:

- tray icon state
- window border indicator
- taskbar overlay
- small nonintrusive indicator

แต่ต้องไม่บดบัง Viewer มาก

---

# 66. System Tray

ควรพิจารณา:

```text
ScreenLink running
Sharing
Not sharing
```

หาก user ปิด main window:

Decision V1 ที่แนะนำ:

- Close = exit application
- หรือถามเฉพาะครั้งแรกว่าจะ minimize to tray หรือ exit

**อย่าปล่อย Share ทำงานใน background โดยผู้ใช้คิดว่าปิดแล้ว**

---

# 67. Performance Metrics

เก็บ:

```text
capture_fps
capture_ms
process_ms
encode_ms
encode_queue
send_bitrate
packet_loss
rtt
receive_bitrate
decode_ms
render_fps
dropped_frames
```

ใช้ rolling window

---

# 68. Latency Target

อย่ารับประกันตัวเลขที่ยังไม่ได้วัด

Engineering target สำหรับ direct P2P V1:

- 60 FPS stable เมื่อ hardware/network เหมาะสม
- latency ไม่สะสม
- queue ต้องไม่โต
- interaction ต้องรู้สึก real-time
- benchmark end-to-end และราย stage

หลัง prototype ค่อยกำหนด p50/p95 target จากเครื่องจริง

---

# 69. Reference Test Hardware

## Machine A — User Reference

```text
Windows 10
Intel CPU
Intel iGPU
Onten OTN-332
```

ต้องทดสอบ:

- monitor
- window
- OTN-332
- hardware encode
- hardware decode
- simultaneous send + receive

## Machine B — Friend Reference

```text
Windows 11
AMD CPU
AMD discrete GPU
```

ต้องทดสอบ:

- monitor
- window
- AMD encode
- AMD decode
- simultaneous send + receive
```

---

# 70. Critical Cross-Vendor Test

ต้องทดสอบ:

```text
Intel encode → AMD decode
AMD encode   → Intel decode
```

สำหรับ:

```text
H.264
HEVC
AV1 ถ้าทั้งคู่รองรับ
```

นี่เป็น first-class test ไม่ใช่ edge case

---

# 71. Development Milestone 0 — Repository and Decisions

### Goal

สร้างฐานงานที่ reproducible

### Tasks

- [ ] สร้าง repository
- [ ] เลือก license ของ ScreenLink
- [ ] เพิ่ม `README.md`
- [ ] เพิ่ม `ARCHITECTURE.md`
- [ ] เพิ่ม `BUILDING.md`
- [ ] เพิ่ม `.gitignore`
- [ ] กำหนด Visual Studio 2022
- [ ] กำหนด CMake
- [ ] กำหนด Qt 6 version
- [ ] เปิด warnings ระดับสูง
- [ ] CI build Windows x64
- [ ] version metadata
- [ ] semantic version scheme
- [ ] logging skeleton
- [ ] config versioning skeleton

### Exit Criteria

Clean checkout สามารถ build executable เปล่าได้ด้วย documented commands

---

# 72. Milestone 1 — Reference Stack Validation

**ห้ามข้าม milestone นี้**

### Setup

สองเครื่อง:

```text
Tailscale
Sunshine
Moonlight
```

### Tests

- [ ] A → B 1080p60
- [ ] B → A 1080p60
- [ ] A ↔ B พร้อมกัน
- [ ] ตรวจ Direct/Relay
- [ ] จด RTT
- [ ] จด bitrate
- [ ] จด encode/decode latency ถ้าเครื่องมือให้
- [ ] Intel host → AMD client
- [ ] AMD host → Intel client
- [ ] H.264
- [ ] HEVC
- [ ] AV1 หากรองรับ
- [ ] Wi-Fi vs Ethernet ถ้าต้องเทียบ
- [ ] Discord Voice เปิดพร้อมกันเพื่อดูผลกระทบจริง

### Deliverable

`reference-benchmark.md`

### Exit Criteria

รู้แล้วว่า baseline experience ที่ต้อง “เทียบให้ได้หรือดีกว่า” คืออะไร

---

# 73. Milestone 2 — System Probe / Diagnostics

ก่อนทำ streaming ของเราเอง ให้โปรแกรมตรวจ hardware ได้ก่อน

### Tasks

- [ ] Windows version/build
- [ ] CPU vendor/model
- [ ] GPU enumerate DXGI adapters
- [ ] monitor enumerate
- [ ] window enumerate
- [ ] capture device enumerate
- [ ] encoder capability probe
- [ ] decoder capability probe
- [ ] network/Tailscale state
- [ ] diagnostics UI
- [ ] export diagnostics text

### Exit Criteria

เปิด ScreenLink บนทั้ง 2 เครื่องและเห็น capability ถูกต้อง

---

# 74. Milestone 3 — Viewer UI Shell

ยังไม่ต้องมี network video

### Tasks

- [ ] main window
- [ ] D3D/placeholder render surface
- [ ] Tab settings overlay
- [ ] scroll settings
- [ ] fullscreen
- [ ] borderless
- [ ] always on top
- [ ] hotkeys
- [ ] responsive layout
- [ ] state indicators
- [ ] settings persistence

### Exit Criteria

UX หลักใช้งานได้ครบด้วย test pattern

---

# 75. Milestone 4 — Source Enumeration and Preview

### Monitor

- [ ] list monitors
- [ ] preview monitor

### Window

- [ ] list top-level windows
- [ ] title/icon
- [ ] preview window

### Capture Card

- [ ] enumerate video devices
- [ ] detect OTN-332
- [ ] enumerate media modes
- [ ] preview 1080p60 ถ้า device offer
- [ ] hot plug

### Exit Criteria

ผู้ใช้เลือก source จาก dropdown และเห็น local preview ได้ โดยยังไม่ส่ง network

---

# 76. Milestone 5 — Capture Pipeline Stability

### Tests

แต่ละ source ต้อง run อย่างน้อยหลายชั่วโมงใน stress test

ต้องทดสอบ:

- [ ] resize window
- [ ] minimize
- [ ] source close
- [ ] monitor resolution change
- [ ] monitor disconnect
- [ ] capture card disconnect/reconnect
- [ ] sleep/wake
- [ ] lock/unlock Windows
- [ ] GPU device reset scenario เท่าที่สร้างได้

### Exit Criteria

capture ไม่ leak memory และ recovery state ถูกต้อง

---

# 77. Milestone 6 — Hardware Encoder Layer

### Tasks

- [ ] abstraction
- [ ] Intel path
- [ ] AMD path
- [ ] NVIDIA path
- [ ] H.264
- [ ] HEVC
- [ ] AV1 capability probe
- [ ] software fallback
- [ ] low-latency settings
- [ ] bitrate control
- [ ] FPS control
- [ ] keyframe control
- [ ] statistics

### Test

encode local stream → file/test sink เพื่อ validate bitstream

### Exit Criteria

ทั้ง Intel reference และ AMD reference encode 1080p60 stable

---

# 78. Milestone 7 — Decoder + Renderer

### Tasks

- [ ] receive test bitstream from local source
- [ ] hardware decode
- [ ] D3D render
- [ ] Fit/Fill/1:1
- [ ] resize
- [ ] fullscreen
- [ ] frame timing
- [ ] drop stale frames
- [ ] stats

### Exit Criteria

local encode → decode → render loop stable 1080p60

---

# 79. Milestone 8 — Localhost End-to-End

ก่อน Internet:

```text
Capture
→ Encode
→ Local transport
→ Decode
→ Render
```

### Goal

พิสูจน์ whole video path โดยตัด network complexity ออก

### Measure

- capture latency
- encode latency
- decode latency
- render latency
- CPU
- GPU engine use
- memory
- dropped frames

### Exit Criteria

1080p60 stable ไม่มี queue growth

---

# 80. Milestone 9 — Feedback Protection

### Tasks

- [ ] identify viewer HWND
- [ ] test WDA_EXCLUDEFROMCAPTURE
- [ ] test WDA_MONITOR
- [ ] test monitor capture backend
- [ ] test window capture backend
- [ ] implement black mask fallback
- [ ] outgoing preview shows masked result
- [ ] measure mask GPU cost
- [ ] simultaneous viewer + sender test

### Critical Test

A และ B แชร์พร้อมกัน

ต้องไม่มี recursive mirror

### Exit Criteria

เปิดสองทาง 30+ นาทีโดยไม่มี feedback recursion

---

# 81. Milestone 10 — Control Protocol

### Build

- [ ] peer state
- [ ] handshake
- [ ] protocol version
- [ ] capability exchange
- [ ] sharing state
- [ ] start/stop message
- [ ] codec negotiation
- [ ] resolution negotiation
- [ ] error messages
- [ ] heartbeat
- [ ] reconnect

### Exit Criteria

สอง app instances communicate state โดยไม่มี video

---

# 82. Milestone 11 — Network Video Transport

นี่คือ milestone ที่มีความเสี่ยงสูงที่สุดหากไม่ reuse Sunshine/Moonlight transport

ก่อนเริ่มต้องผ่าน Architecture Decision Gate:

```text
Reuse Sunshine/Moonlight
vs
Fork
vs
Native transport
```

### Requirements ไม่ว่าเลือกทางไหน

- low latency
- bounded buffering
- packet-loss handling
- reconnect
- bitrate feedback
- secure transport
- session identity
- Direct P2P preference

---

# 83. Milestone 12 — Internet / Tailscale Integration

### Tasks

- [ ] peer discovery/address
- [ ] detect Tailscale installed
- [ ] detect peer online
- [ ] detect direct vs relay
- [ ] RTT
- [ ] connection diagnostics
- [ ] reconnect
- [ ] CGNAT test ถ้าทำได้
- [ ] hard NAT behavior documentation

### Exit Criteria

A/B เชื่อมข้าม Internet และ UI บอก connection path จริง

---

# 84. Milestone 13 — One-Way Production Flow

### Scenario

```text
A shares
B views
```

ต้องทดสอบ:

- monitor
- window
- OTN-332
- start/stop repeatedly
- source switching
- quality changes
- peer disconnect
- peer reconnect

### Exit Criteria

one-way share ถือว่าใช้งานประจำวันได้

---

# 85. Milestone 14 — Two-Way Sharing

### Scenario

```text
A → B
A ← B
```

ต้องตรวจ:

- simultaneous encode/decode
- GPU contention
- CPU
- network upload/download
- feedback mask
- UI responsiveness
- Discord running simultaneously
- source combinations

เช่น:

```text
A Monitor ↔ B Monitor
A OTN-332 ↔ B Monitor
A Window ↔ B Window
```

### Exit Criteria

สองทางเสถียรหลายชั่วโมง

---

# 86. Milestone 15 — Quality Controller

V1 Auto Quality แบบง่ายก่อน

Inputs:

```text
RTT
packet loss
send queue
encode FPS
decode drops
```

Actions:

```text
bitrate down/up
optional FPS downgrade
optional resolution downgrade
```

ต้องมี hysteresis ป้องกันค่ากระโดดขึ้นลงตลอด

---

# 87. Milestone 16 — Recovery Tests

ต้องทดสอบ fault injection

- [ ] unplug Ethernet
- [ ] disable Wi-Fi
- [ ] restart router ถ้าทำได้
- [ ] kill peer
- [ ] restart peer
- [ ] unplug OTN-332
- [ ] close shared window
- [ ] change resolution
- [ ] sleep/wake
- [ ] GPU driver reset/restart scenario
- [ ] Tailscale relay/direct transition

ทุก failure ต้องมี expected behavior

---

# 88. Milestone 17 — Installer / Distribution

### Requirements

- [ ] Windows x64 installer
- [ ] required runtime
- [ ] Qt deployment
- [ ] code signing plan
- [ ] uninstall clean
- [ ] config preservation policy
- [ ] firewall rule management
- [ ] Tailscale dependency instruction
- [ ] third-party notices
- [ ] GPL/license compliance decision
- [ ] version shown in UI

---

# 89. Milestone 18 — V1 Acceptance Test

V1 ไม่ถือว่าสมบูรณ์เพียงเพราะ “เปิดแล้วเห็นภาพ”

ต้องผ่านทุกกลุ่ม:

### Compatibility

- [ ] Windows 10
- [ ] Windows 11
- [ ] Intel CPU
- [ ] AMD CPU
- [ ] Intel iGPU
- [ ] AMD dGPU
- [ ] NVIDIA path อย่างน้อย test machine/CI lab เมื่อหาได้

### Sources

- [ ] Monitor
- [ ] Window
- [ ] OTN-332
- [ ] Generic second capture device ถ้าหาได้

### Streaming

- [ ] A → B
- [ ] B → A
- [ ] A ↔ B

### UX

- [ ] Tab
- [ ] settings scroll
- [ ] preview
- [ ] Share/Stop
- [ ] fullscreen
- [ ] always on top
- [ ] borderless

### Recovery

- [ ] network reconnect
- [ ] peer reconnect
- [ ] source lost
- [ ] capture card reconnect

### Safety

- [ ] no auto-share
- [ ] sharing state visible
- [ ] no infinite mirror

---

# 90. Definition of Done — V1 Complete

V1 Complete เมื่อ:

1. สองเครื่อง Windows 10/11 เชื่อมกันได้
2. Intel ↔ AMD ใช้ร่วมกันได้
3. Monitor/Window/Capture Card ใช้ได้
4. Onten OTN-332 ใช้ได้จริง
5. 1080p60 ทำงานเสถียรบน reference hardware/network ที่เหมาะสม
6. Hardware encode/decode ถูกใช้โดยอัตโนมัติ
7. H.264 fallback ใช้งานได้
8. สองฝั่ง Share พร้อมกันได้
9. ไม่มี infinite mirror
10. Settings UX ครบตาม specification
11. Always on Top/Fullscreen/Borderless ทำงาน
12. Network status แยก Direct/Relay ได้
13. reconnect จาก network interruption ได้
14. device/source failure ไม่ทำให้ application crash
15. Share ไม่เปิดเองหลัง restart
16. ไม่มี audio subsystem ใน binary path ที่ถือเป็น V1 feature
17. installer/uninstaller ทำงาน
18. diagnostics เพียงพอสำหรับแก้ปัญหา
19. documentation สำหรับ install/connect/share ครบ
20. license/compliance review เสร็จก่อน distribute

---

# 91. Things That Must Not Be Optimized Too Early

อย่าเสียเวลาใน V1 กับ:

- UI animation
- theme system ซับซ้อน
- HDR
- 4K tuning
- 120/144 FPS
- custom blur mask
- multi-user
- cloud account
- NAT protocol ใหม่
- custom audio
- elaborate tray UX
- custom virtual display
- fancy preview effects

ต้องพิสูจน์:

```text
Capture
Encode
Network
Decode
Render
Recovery
```

ก่อน

---

# 92. Things That Must Be Designed Correctly From Day One

ตรงกันข้าม สิ่งเหล่านี้ห้าม “ค่อยแก้ทีหลัง”:

- protocol version
- bounded queues
- timestamps
- hardware abstraction
- source abstraction
- encoder abstraction
- state machines
- no auto-share
- logs ไม่เก็บภาพ
- configuration versioning
- diagnostics
- Direct/Relay distinction
- licensing decision gate
- source lost/reconnect semantics

---

# 93. Highest-Risk Areas

## Risk 1 — Network transport

ยากที่สุดหากเขียนเอง

Mitigation:

ใช้ Sunshine/Moonlight เป็น reference และตัดสิน reuse หลัง benchmark

## Risk 2 — Cross-vendor hardware encoding

driver behavior ต่างกัน

Mitigation:

capability probe + runtime test + fallback

## Risk 3 — Feedback exclusion

capture backend behavior อาจต่างกัน

Mitigation:

WDA test + own black-mask fallback

## Risk 4 — Capture Card formats

USB capture devices expose formatsไม่เหมือนกัน

Mitigation:

runtime enumeration + Media Foundation + fallback path

## Risk 5 — GPL integration

architecture อาจได้รับผลกระทบจากวิธี reuse

Mitigation:

license gate ก่อน fork/link/distribution

## Risk 6 — Latency accumulation

เกิดจาก queue มากกว่าความเร็ว codec อย่างเดียว

Mitigation:

bounded queues + drop stale frames + metrics

---

# 94. First Prototype Sequence — Recommended Exact Order

ถ้าจะเริ่มทำงานจริง ให้ทำตามลำดับนี้:

```text
1. ทดสอบ Sunshine/Moonlight/Tailscale ระหว่างสองเครื่อง
2. จด baseline performance
3. สร้าง ScreenLink empty Windows app
4. สร้าง diagnostics hardware probe
5. ทำ Viewer UI + Settings Overlay
6. ทำ Monitor preview
7. ทำ Window preview
8. ทำ OTN-332 preview
9. ทำ unified capture frame
10. ทำ hardware encoder
11. ทำ hardware decoder
12. ทำ local loopback stream
13. ทำ feedback mask
14. ทำ control protocol
15. ตัดสินใจ transport integration
16. ทำ network stream
17. ทำ one-way Internet test
18. ทำ two-way Internet test
19. ทำ recovery
20. ทำ installer
21. ทำ V1 acceptance
```

**อย่าสลับไปเริ่มจาก network protocol ก่อน capture/local loopback จะนิ่ง**

---

# 95. Recommended First Technical Spike

Spike แรกควรตอบ 5 คำถาม:

1. Sunshine/Moonlight บน Intel ↔ AMD ให้ latency/quality เท่าไร?
2. Tailscale connection เป็น Direct หรือ Relay?
3. OTN-332 expose 1080p60 format อะไรจริง?
4. WDA_EXCLUDEFROMCAPTURE ทำงานกับ capture backend ที่เราจะใช้หรือไม่?
5. Hardware encode+decode พร้อมกันสองทางใช้ resource เท่าไร?

ถ้าตอบ 5 ข้อนี้ได้ Architecture Decision จะมีข้อมูลมากขึ้นมาก

---

# 96. V1 UX Walkthrough

## Start

```text
Open ScreenLink
↓
Friend status: Online/Offline
↓
Viewer waiting
```

## Friend shares

```text
Friend Sharing
↓
Video appears
```

## User wants to share

```text
Tab
↓
Settings Overlay
↓
Select Source
↓
Preview
↓
Share
```

## Both sharing

```text
Friend video remains viewer
Your outgoing pipeline runs simultaneously
Feedback protection active if Desktop/Monitor source
```

## User wants viewer only

```text
Tab
↓
Settings hidden
```

## Floating viewer

```text
Always on Top
+
Borderless
```

## Fullscreen

```text
F11
```

---

# 97. Suggested Status Language

ใช้คำสั้นและไม่คลุมเครือ:

```text
Offline
Connecting
Online
Sharing
Not sharing
Direct
Relay
Reconnecting
Source lost
No signal
Hardware encoding
Software encoding
```

หลีกเลี่ยง technical jargon ใน main UI

---

# 98. No Signal Behavior — Capture Card

หาก device อยู่แต่ HDMI ไม่มี signal:

```text
Device: Onten OTN-332
Status: No Signal
```

Preview:

```text
NO SIGNAL
```

ห้ามตีความเป็น device disconnected

สองสถานะต้องแยก:

```text
Device Disconnected
No Signal
```

---

# 99. Future V2 Candidates

หลัง V1 เท่านั้น:

- source/system audio
- capture card HDMI audio
- remote control
- clipboard
- invite code
- custom relay
- multi-user
- AV1 tuning
- HDR
- 1440p/4K profiles
- 120 FPS
- PiP compositor
- recording
- native virtual display
- multi-monitor simultaneous sharing
- browser client

---

# 100. Reference Sources / Current Technical Facts

> แหล่งเหล่านี้ใช้ยืนยันข้อเท็จจริงของ platform/reference stack ณ วันที่จัดทำเอกสาร ควรตรวจ version ล่าสุดอีกครั้งก่อน implementation ระยะยาว

## Sunshine

- Sunshine — self-hosted game stream host for Moonlight, รองรับ hardware encoding จาก AMD/Intel/NVIDIA และ software encoding  
  https://github.com/LizardByte/Sunshine

- Sunshine Configuration — Windows capture backends `ddx` และ `wgc`; encoders `nvenc`, `quicksync`, `amdvce`, `software`  
  https://docs.lizardbyte.dev/projects/sunshine/master/md_docs_2configuration.html

- Sunshine Legal — GPL-3.0 และข้อมูล commercial-use/licensing caveats  
  https://docs.lizardbyte.dev/projects/sunshine/latest/md_docs_2legal.html

## Moonlight

- Moonlight PC repository — GameStream client, Qt/MSVC build information, GPL-3.0  
  https://github.com/moonlight-stream/moonlight-qt

- Moonlight hardware decoding notes — modern AMD/NVIDIA/Intel GPUs and H.264 hardware decoding on Windows  
  https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems

## Tailscale

- Connection types — Direct, Peer Relay, DERP  
  https://tailscale.com/docs/reference/connection-types

- Device connectivity / NAT behavior  
  https://tailscale.com/docs/reference/device-connectivity

- Peer Relay  
  https://tailscale.com/docs/features/peer-relay

## Microsoft Windows Capture

- Windows Graphics Capture — display/application window frame capture  
  https://learn.microsoft.com/windows/apps/develop/media-authoring-processing/screen-capture

- `IGraphicsCaptureItemInterop::CreateForWindow` — Win32 HWND capture, Windows 10 1903+  
  https://learn.microsoft.com/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow

- `IGraphicsCaptureItemInterop::CreateForMonitor` — HMONITOR capture, Windows 10 1903+  
  https://learn.microsoft.com/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createformonitor

- `SetWindowDisplayAffinity` — `WDA_MONITOR` / `WDA_EXCLUDEFROMCAPTURE`; latter introduced in Windows 10 Version 2004  
  https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity

## Onten OTN-332

- Onten official product page  
  https://www.onten.com.cn/enp-531.html

---

# 101. Final Architecture Principle

ScreenLink ต้องรักษาหลักนี้ตลอดการพัฒนา:

```text
User Intent
   ↓
Simple UI
   ↓
Explicit Share State
   ↓
Source Abstraction
   ↓
Low-copy Capture
   ↓
Feedback Protection
   ↓
Hardware Encode
   ↓
Direct P2P When Possible
   ↓
Hardware Decode
   ↓
Low-latency Viewer
```

ทุก feature ใหม่ควรถามว่า:

> ทำให้ผู้ใช้เห็นจอเพื่อนได้ดีขึ้น ง่ายขึ้น เสถียรขึ้น หรือไม่?

ถ้าไม่ — ไม่ควรดึงเข้ามาใน V1

---

# 102. Final V1 Statement

**ScreenLink V1 คือโปรแกรมแชร์ภาพแบบ real-time สำหรับ Windows 10/11 จำนวนสองคน ที่ทั้งสองฝ่ายสามารถแชร์และดูพร้อมกันได้ โดยรองรับ Monitor, Window และ Capture Card, ทำ hardware video acceleration แบบ vendor-neutral, เชื่อม P2P เป็นหลัก, ป้องกัน feedback loop อัตโนมัติ และมี Viewer UI ที่เรียบที่สุดเท่าที่ทำได้**

V1 จะถือว่าสำเร็จเมื่อมันสามารถแทนการใช้ Discord Screen Share สำหรับกรณี “เพื่อนสองคนต้องการดูจอกันคุณภาพสูงแบบ real-time” ได้อย่างสม่ำเสมอ โดย Discord ยังคงทำหน้าที่ Voice/Chat แยกต่างหาก

---

## Development Rule

> **พิสูจน์ทีละชั้น และอย่าซ่อน failure ด้วย abstraction**
>
> Capture ต้องพิสูจน์ก่อน Encode  
> Encode/Decode ต้องพิสูจน์ก่อน Internet  
> Direct P2P ต้องพิสูจน์ก่อน Quality tuning  
> One-way ต้องเสถียรก่อน Two-way  
> Recovery ต้องผ่านก่อนเรียก V1 ว่า Complete
