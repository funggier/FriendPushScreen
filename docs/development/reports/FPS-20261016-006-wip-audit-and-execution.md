# FPS-006 — WIP audit and native execution qualification
Date: 2026-10-16 (Asia/Bangkok)
Status: ACTIVE — no GREEN claim until all gates pass.
Authority: canonical branch `fps-006-encode-decode-local-loopback`; resumed at `ed6a07f2a008b3fafa717981c45d7510aa7d1b46`.
Machine: CDQ-P, Windows 10 build 19045, Intel Core Ultra 5 245K.
Task: #14. No network, control, or audio changes.

## WIP audit before new conversion/encoder work
Both older worktrees were read without reset, clean, build, or writes. Their existing binaries predate their latest source edits, so old passing logs do not qualify the WIP.

| Source | Decision | Reason |
|---|---|---|
| fps-006-video-codec-local-loopback, graphics/d3d11_video_converter.* | Selectively port | Existing bidirectional VideoProcessor implementation fits CapturedFrame; caller owns destinations and decoder subresource is explicit |
| fps-006-codec-local-loopback, media/d3d11_video_converter.* | Reference | Useful pitch-aware readback and CPU uploads; public decoder conversion ignores array slice; reusable NV12 can be overwritten while async MFT consumes it |
| fps-006-codec-local-loopback, media/video_frame_converter.* | Do not wholesale port | CPU conversion/reference useful, but BT.601 conflicts with chosen SDR BT.709; GPU readback cannot be normal monitor/window path; malformed odd NV12 validation unsafe |
| fps-006-codec-local-loopback, media/mf_h264_codec.* | Adapt sample/event concepts | Old model differs; timestamp mapping is FIFO, pending decoder sample lease lost, async backpressure can stall, drain not resumable |
| fps-006-video-codec-local-loopback, media/mf_video_encoder.* | Adapt negotiation/CodecAPI patterns | Single reused NV12 surface unsafe for async input; boolean input event credit loses multiplicity; waiting for every output can deadlock buffering |
| Canonical clock/catalog/activation/media types/queue/metrics | Retain | Current task contracts and prior model tests already committed |

Implementation direction: shared sync/async MFT session, timestamp keyed metadata, bounded eight-surface encoder pool retained through encoded output, tracked decoder sample retained until GPU copy/conversion fence completes, explicit software encode/decode forcing, actual runtime-selected transforms.

## Conversion checks
Physical test target: `FriendPushScreenConversionTests`, manually invoked; never silently skipped/passed on generic CI.
RED on ported WIP: 2 failures — short CPU pitch accepted, padded decoder texture rejected.
GREEN after corrections: 0 failures, native BGRA→NV12→BGRA executes, BT.709 red luma 63, decoder slice index and padded extent validation.
SDR contract: BT.709 primaries/matrix/transfer, full-range RGB, limited-range NV12. Codec media types carry matching attributes.
Decoder input is copied from the requested validated subresource and visible rectangle. Output CapturedFrame owns BGRA texture.
No conversion success is inferred solely from resource creation.

## Evidence rules
- MFT catalog classification is preserved; Microsoft decoder remains a software MFT even when D3D11 acceleration executes.
- Device inventory, codec discovery/activation, accelerator creation, real compressed-bitstream execution, and sustained capture/render are separate evidence levels.
- HEVC/AV1 absence through MF does not imply silicon lacks them.
- No AMD/NVIDIA physical qualification is claimed.
- Exact source revision, command exits, performance/resource data and closure decisions will be added after runtime qualification.
