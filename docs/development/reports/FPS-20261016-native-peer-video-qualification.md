# Native two-instance H.264 qualification — 2026-10-16

Status: scoped native No Audio loopback functionality demonstrated. User confirms stable menus, no visible picture flicker after the atomic test-source correction, and corrected Diagnostics scrolling. Strict uninterrupted-generation performance remains separately reported; FPS-009 and the full transport decision are not fully GREEN.

## Scope and identities

This increment exchanges actual No Audio H.264 video between two portable Win32 applications on CDQ-P, Windows 10 19045 x64, Core Ultra 5 245K, 14 logical processors and 32 GB RAM. It is a loopback evaluation, not full V1, LAN, DNS/DDNS, Internet, Tailscale or a physical second-PC result.

Final implementation source: 8f33876f52c9e08e20c7aef44d5ba650a89a0b11. Final physical application SHA256: F117743C53205EE1494CEF4A303417FF01B7671BD0F62564997CEFB3A295D01A. The previous final-UI matrix and three five-minute phases used a21b651 / F206D45438F18A59AFF78CA756C7E3D0124FADEF087D89027AB0DE9CAE8228AF. The earlier three-phase measurements used ea2b68a / E336E6357AD38C2CD386200CFAB701A625DD7D635495090C393BED0453E82A58, before the final UI/review corrections. The UI codec matrix used 9925646C6BA503C26AECDEB5BA29F998CF29D47C437052AADBFD529A5E56F1E8, which contains the same codec recovery implementation before the monitor-name lookup fix.

The two folders have separate portable settings, DPAPI-protected private identities and explicit public peer trust. Each run records its own PIDs. The final Diagnostics pixel test used A PID 31956 on Share Port 42801 and B PID 34372 on 42802 through 127.0.0.1. Public fingerprints and exact runtime hashes are in the adjacent qualification manifest. MsQuic internally exposes wildcard UDP socket ownership; the listener API is configured for IPv4 loopback, incoming endpoints are checked, and non-loopback client endpoints are explicitly rejected. Socket ownership alone is not proof of an unrestricted listener.

Pinned MsQuic/OpenSSL 2.6.3 runtime SHA256: 290B6C8716AC09EEA043913F86164E0B2F5096C666C9F1CA81C0C5C1864E1CB0. No firewall/router/registry or machine certificate trust change was made.

## Flicker and recovery changes

The original application could send more compressed units while the receiving UI still owned both allowed units. That filled the receive budget and caused repeated decoder/generation resets and black output. The fix waits for native decoder readiness, paces outgoing units with consumer acknowledgments, and keeps timed-out credits blocked until a coordinated fresh-generation/VideoReady barrier. The two-unit/16 MiB transport limits were not increased.

Brief same-format recovery keeps the last valid viewer picture while status explicitly waits for a fresh IDR and SPS/PPS. Stop Share, Disconnect, source loss and genuine format/source changes still invalidate the old picture. Incoming picture ownership remains independent of the local Preview option.

Two controlled 700 ms receiver file-dialog stalls recovered with no transport overflow. During the second stall, 80 screen samples at 25 ms spacing found zero unexpected/black pixels in the known red source stripe after the dialog closed; cumulative frame counters continued. The source was the separate owned moving WGC fixture, not the receiver's preview. Before/after images and samples are preserved.

Native UI/codec scheduling pauses can still trigger automatic recovery. The strict diagnostic requires an unchanged generation throughout five minutes; its failures are retained separately from throughput and transport-overflow results. A recovery is not silently described as uninterrupted delivery.

Meaningful compiled RED→GREEN evidence:

| Behavior | RED | GREEN |
|---|---|---|
| Expired credits cannot be reused before consumer recovery; matching recovery format; retain image; cumulative counters | 260–262 | 263, Release CTest 2/2 |
| Missing saved decoder reports an error to both peers and Apply with a valid codec resumes without remote Stop/Start | 270 | 271, Release CTest 2/2 |
| Production monitor descriptors resolve their selected display name; an unrelated/stale handle cannot choose a different monitor | 277 | 278, Release CTest 2/2 |

The decoder-retry orchestration test uses explicit codec test doubles over a real mutually authenticated connection. Physical codec evidence below uses actual native MFTs.


### Final viewer and Settings corrections

The fresh independent review identified three concrete effects, originally graded Minor. The parent graded them Important for the reported flicker and duplex usage and corrected them in one regression-verified pass:

- Stopping local Preview or failing to start local capture must not clear the received picture. Actual old-binary paused-peer tests found 38/50 and 33/50 unexpected pixels respectively; final physical tests each found 0/50.
- Incoming/outgoing codec errors now have separate ownership. An unrelated successful outgoing/receiving stream no longer erases an unresolved error in the other direction. A peer error retains its generation. Authenticated duplex orchestration RED291 -> GREEN298 covers this.
- Status-only updates do not resize the viewer; an identical-size D3D resize preserves the existing swap-chain resources. Genuine peer redraws use Present(0), including resize. Actual MainWindow callback and held-backbuffer RED tests pass after the correction. No cause is inferred for earlier recovery incidents.

The user additionally reported a white Hide Settings flash and Settings controls looking as though they reload. Real Win32 tests reproduced the default button erase color 240,240,240, three redundant captions and two codec-list resets on one status update. The fix paints owner-drawn button erase surfaces with the dark brush, updates unchanged captions/checks only when needed, preserves codec/connection/audio dropdown contents and avoids unchanged panel geometry/show/layout work. GREEN302/303 reports color 38,46,57 with captions0, codec resets0 and dirty buttons0.

Actual application stress309 sampled the Hide Settings background380 times, including eight overlay toggles, with zero white-background samples. The encoder menu and a deliberately uncommitted highlight remained unchanged across five seconds of live status updates. Final actual UI matrix313 again passed all four CPU/GPU pairs and buffered encoding after these changes.
### Remaining burst flicker traced to the owned test source

After the menu correction, the user still saw repeated flicker bursts. A 180-second visible-stripe test on the unchanged F206 application found 110 unexpected A samples and 13 B samples among 10,790 samples. Several pixels matched the synthetic source background instead of its constant colored stripe. Inspection showed the GDI fixture painted its background directly into the window before drawing the other content.

The fixture now composes the complete image offscreen and submits one final BitBlt. Its SHA256 changed from 620093C8F0112661AE464891C95DAA2DAE0889E26D0923967326AE8032FE8A47 to 275042FEF8FBDA8E8B8BCF88653C1320BB00DBFC7A74CC4407A2D3FD79301181. The application binary and media pipeline did not change for this comparison.

The owned-window observer then recorded 10,783 samples over 180 seconds with zero unexpected A/B pixels and zero occluded samples. It checked the sampled point's owning PID on each frame. The requested 8 ms sampling interval was paced by actual screen copies to roughly 60 samples/second. The user independently confirmed no flicker at 20:43:59 Asia/Bangkok.

One intermediate run was rejected because its initial reference stripe was absent; it is not a passing test. The earlier sampler lacked PID ownership checks, so its long grayscale interval is retained without assigning every anomalous sample to the fixture. The controlled change, later ownership-checked result and user observation support the source-paint diagnosis; they do not prove universal absence of flicker on arbitrary sources.

### Diagnostics mouse-wheel ghosting

The user's screenshot showed accumulated glyphs when scrolling the read-only Diagnostics EDIT. A physical reproduction matched the screenshot: after internal scrolling, forcing a clean repaint changed 11,247 / 28,396 / 33,336 / 12,578 pixels across four rounds. The control was incorrectly given a transparent label background through WM_CTLCOLORSTATIC, which read-only EDIT controls also use.

The targeted correction selects an opaque surface background for Diagnostics. Native compiled RED321 failed its mode/color/brush checks; Release CTest324 passed after the correction. The new F117 app then passed physical GREEN326: four rounds of 12 line-scroll steps each matched a clean repaint exactly, with zero changed pixels in every round. The user confirmed the result at 20:56:44 Asia/Bangkok.

The RED metadata's text_length25 came from an interprocess initial caption, not the actual diagnostic text; that field does not describe report length. GREEN uses WM_GETTEXT and recorded 2,668 characters. Pixel comparisons and screenshots remain independent of that metadata correction. A helper's inherited LASTEXITCODE briefly produced a false wrapper failure after both updated apps had launched; direct verification on the checked F117 hashes passed.

Microsoft's message contract: https://learn.microsoft.com/windows/win32/controls/wm-ctlcolorstatic

## Native codec and capture results

Eight physical media probe scenarios passed: CPU→CPU, CPU→GPU, GPU→CPU and GPU→GPU at 1920×1080; GPU odd 1919×1079; CPU and GPU portrait 641×1153; and buffered hardware encoding. Every scenario reported zero decoded-content, geometry and drop failures. Decoder real-time mode remained enabled. CPU encoders delivered all 120/90 frames; hardware encoders retained one undrained tail frame, reported explicitly rather than counted as delivered.

Actual application UI matrix 279 passed all four independent codec pairs plus buffered GPU encoding. B had no selected local source. Encoder/decoder frame counters confirmed the selected CPU/GPU paths; changing bitrate to 12 Mbps, GOP 60 and low latency preserved the selected outgoing source. The test restored 20 Mbps, GOP 120 and low latency for hardware measurements. CPU selection still uses D3D11 conversion/display.

Physical feedback qualification:

- DDA fallback 280: both actual application windows masked, zero nonblack masked pixels, 5,440 reference-color points outside masks preserved, original GPU texture unchanged. Old/new moving-window coverage completed in 47 ms.
- Normal affinity 285: both applications reported WDA_EXCLUDEFROMCAPTURE (0x11); the checked monitor fallback still masked both windows and their movement history. Reference points and original texture remained unchanged.
- WGC is the production window backend, not a monitor backend. The normal owned reference window retained 3,686,336 known-color pixels without accidental monitor-coordinate masking, before and after moving an application window. Both application viewer sources were explicitly rejected.
- Initial probe attempts are retained as diagnosis: a NOACTIVATE/tool fixture was not WGC-capturable; normal popup qualification passed. DDA's first exclusion-transition frame lacked the known reference scene, so the probe now establishes that bounded scene and reports its one warmup frame before assertions.
- Negative-origin, clipping, portrait/odd and overlapping-mask coordinate cases also pass unit tests. Physical non-primary/negative-origin monitor sharing and network capture-card sharing are separate unclaimed scenarios.

## Five-minute measurements

The a21b651 / F206 source ran three actual five-minute phases with corrected GPU counter paths. All phases met the >=59 fps throughput aim and had zero transport drops. All failed the stricter unchanged-generation diagnostic because automatic recovery occurred:

| Phase | A sent / rendered fps | B sent / rendered fps | Outgoing recoveries A / B | Strict result |
|---|---|---|---|---|
| A to no-source B | 59.527 / 0 | 0 / 59.527 | 2 / 0 | FAIL |
| B to A | 0 / 59.623 | 59.623 / 0 | 0 / 1 | FAIL |
| Simultaneous | 59.142 / 59.149 | 59.329 / 59.322 | 3 / 1 | FAIL |

The earlier ea2b68a / E336 three-phase results were approximately 59.12-59.38 fps, zero drops, and strict FAIL/PASS/FAIL for A-to-B/B-to-A/duplex. Their summaries preserve the GPU counter-path error; they are not GPU utilization evidence. Corrected a21 sampling has gpu_error=null and retains per-engine PID-filtered values.

Final 8f33876 / F117 duplex run328 with the atomic fixtures lasted 300.003 seconds. It uses the capture-affinity diagnostic fallback for the same owned sources. Results:

| Receiver | Sent / rendered fps | CPU mean % | Peak working / private MiB | Outgoing recoveries | Transport drops |
|---|---|---|---|---|---|
| A | 59.793 / 59.800 | 1.264 | 284.1 / 271.4 | 0 | 0 |
| B | 59.800 / 59.793 | 1.259 | 312.0 / 298.4 | 1 | 0 |

Both directions met the >=59fps throughput aim. The strict unchanged-generation result is FAIL and is retained as such. Automatic recovery is still a continuity limitation; the source-paint and Diagnostics fixes do not claim to eliminate native scheduling pauses. gpu_error=null; per-engine values remain in the raw samples and must not be summed into a whole-GPU percentage.

| Receiver | Encode p50 / p95 / p99 ms | Decode p50 / p95 / p99 ms | End-to-end p50 / p95 / p99 ms |
|---|---|---|---|
| A | 5.554 / 8.607 / 9.315 | 1.845 / 3.014 / 4.614 | 9.978 / 12.727 / 16.035 |
| B | 5.465 / 8.570 / 9.283 | 1.788 / 2.923 / 4.620 | 10.034 / 12.696 / 16.055 |

Latency entries are the last observed bounded rolling distributions (up to 18,000 frames), not an isolated network-latency benchmark. Counters retain recovery history. The final native runtime was then restored to normal capture exclusion (0x11) on both apps.

Measurements use actual applications, WGC 1920×1080 moving patterns, independent native GPU encoding/decoding, 20 Mbps target and GOP 120. The resource sampler records per-process CPU (normalized to all logical processors), working/private memory, and PID-filtered per-engine GPU counters. QPC-based stage latency is same-host evidence, not remote clock alignment. Native successful presentation counts submitted frames, not an independent physical-refresh counter; DXGI occlusion success is not treated as display measurement.

Raw summaries/samples preserve both positive and negative runs. Earlier results include:

| Source/run | Sent/rendered FPS | Transport drops | Strict unchanged-generation result |
|---|---|---|---|
| Before peer-only Present(0), duplex 249 | approximately 57.6–58.1 | 5 each | FAIL |
| Before coordinated expiry recovery, A→B 258 | counters reset after one late failure | receive-budget incident | FAIL |
| d65a95f, A→no-source B 268 | 59.607 / 59.610 | 0 | PASS |
| d65a95f, B→A 269 | 59.390 / 59.390 | 0 | FAIL: one automatic recovery |

The first application run was also retained: repeated generation resets within seconds, before the consumer-pacing fix.

## Security, bounds and lifecycle

Native security tests reject unknown/wrong pins, missing client credentials, self identity, expired credentials, wrong SAN and wrong purpose before media delivery. Signed malformed-purpose/time/name fixtures use actual test identity keys; no TLS success is mocked. Normal TLS certificate checks remain enabled together with the exact full-DER peer pin. Identity corruption is refused rather than silently generating a replacement. Pairing decline writes no trust, and acceptance verifies the full known fingerprint.

The protocol parser validates fixed-width network-order lengths, versions, reserved bits, geometry and cursor limits before payload allocation. Transport tests include fragmented controls, partial/consumer-held media credit, abort, bounded deadlines, duplicate port, unavailable endpoint and repeated connection/shutdown. The controller/session tests cover simultaneous Connect, one selected authenticated session, old tokens/generations, independent share/receive, heartbeat and bounded reconnect.

Actual lifecycle287 passed F11/Esc viewport restoration, Settings toggles, Stop Share A preserving B video and authentication, live source B->A->B fresh generations, and B source loss stopping only B outgoing while B still received A. Actual native saved-decoder test292 restarted B with an unavailable decoder: the full public fingerprint was unchanged, no source/share was auto-started, both peers showed the real native error, and selecting GPU through Apply resumed A->B without A Stop/Start (201 native received frames observed).

Actual lifecycle320 repeated all five scenarios successfully on F206 immediately before the Diagnostics correction. Final F117 lifecycle329 repeated all five assertions successfully, including live source switch and source loss. Both apps were then reopened with the unchanged public trust, normal 0x11 capture exclusion, distinct ports and active bidirectional GPU video; the current PIDs/ports/hashes are in final-running-apps.json and final-port-owners.json. Persistent public identities and trust were preserved across the controlled application restarts.

## Decision gates and limits

DG-001 remains provisional: the pinned native QUIC candidate enables this No Audio loopback increment. Same-bitstream encrypted TCP comparison, scheduled synthetic audio multiplexing, full V1 audio, non-loopback path/one-public-port qualification and WAN loss behavior are not supplied by these results. DG-006 distribution/licensing and DG-008 Direct Internet security remain open for their full decision inputs. Existing package notices and archive/runtime hashes are retained.

There is no claim that a timeout can cancel an arbitrary blocked synchronous MFT call. Decoder real-time low latency remains mandatory for the qualified native path. Network/capture-card, physical negative-origin monitor and physical remote-machine results must be qualified separately.

## Final verification and review

Fresh gpt-6-astra/high read-only whole-change review covered 000c6a0..ea2b68a; findings, re-grading, Review Focus outcomes and declined scopes are preserved in independent-review.md. No implementer delegation or second review was used. All three re-graded findings and the user's additional Settings cases have compiled or physical RED->GREEN evidence.

All native targets built in Debug and Release; CTest303 passed 2/2 in each. After the final Diagnostics and fixture changes, regression327 again built every target and passed CTest2/2 in both configurations. A concurrent 10-second prior local-loop regression delivered 561/561 frames with zero drops/resets and final queue0; its 55.9254 fps while the two network apps remained active is a functional regression result, not a new standalone 1080p60 performance qualification. Historical implementation commit a21b6518a8e15db374dae128bb319ed90f41c6e6 passed both Windows x64 Debug and Release CI jobs in run 38906825060:
https://github.com/funggier/FriendPushScreen/actions/runs/38906825060

Final implementation commit 8f33876f52c9e08e20c7aef44d5ba650a89a0b11 passed both Windows x64 Debug and Release jobs in run 38912126386: https://github.com/funggier/FriendPushScreen/actions/runs/38912126386 . The compact exact-commit/job evidence is source-ci-8f33876.json.

The final documentation commit receives a separate exact-head CI check before delivery; its final run/SHA is retained in the native ledger and final handoff rather than recursively embedding a commit's own SHA into its tracked content.


## Workflow rulings and verification cost

- Native implementation stayed with the parent. The single fresh whole-change review was used once; no second review or implementer delegation was added. Its three Minor findings were re-graded Important and all corrected. No review Minor was deferred.
- The user's continuous-execution approval overrides routine skill confirmation/handoff prompts. The existing isolated worktree and ignored portable identities/fixtures are retained for repeatable testing; cleanup must not delete the user's working copies. Main, prior worktrees, tags and releases remain unchanged.
- Supplementary skill helper/template resources were unavailable through the advertised resource mechanism. The native ledger, review package and exact command/session evidence supplied the equivalent workflow records.
- The 16 MiB limit describes two logical transport access units per direction, including partial/in-flight/consumer-held work. It is not a total-process-memory cap; codec/conversion/decoder working copies explain the separately reported process memory.
- A fully copied already-authorized private identity is cryptographically the same identity; the project does not claim clone detection. Self, malformed, changed-pin, untrusted, expired, wrong-name and wrong-purpose credentials are separately tested.
- Physical monitor/card/second-PC/WAN results, synchronized remote clocks, full audio and cancellation of arbitrary native codec calls are not inferred from this localhost work. No causal claim ties every earlier UI gap/recovery to resizing.
- The seven retained later five-minute measurement phases alone account for about 35 minutes of observation. Earlier failed runs, physical fault/painting probes, build/test/CI and review add time. Exact model token and monetary totals are not exposed and are not invented.

Evidence directory: ../evidence/native-peer-video-20261016/. Portable data/private identities stay outside tracked evidence. Main, older worktrees, releases and tags are unchanged.
