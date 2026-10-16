# FPS-006 follow-up — Independent video codec settings

Date: 2026-10-16 (Asia/Bangkok)  
Repository: funggier/FriendPushScreen  
Canonical branch: fps-006-encode-decode-local-loopback  
Qualified implementation: d116ab13ec9422174aa9ed7d761672d14f5b42a8  
Status: GREEN on the canonical feature branch; integration remains separate.

## Outcome and user operation

The user requested CPU codec support and independent, configurable encoder/decoder choices. CPU encode/decode already existed behind FPS_FORCE_SOFTWARE_CODEC=1, which forced both together. This change exposes the existing H.264 execution paths in Settings → VIDEO / H.264.

Each selector contains the runtime-discovered native registrations, with explicit [GPU] and [CPU] choices. The default, Prefer GPU (allow CPU fallback), preserves the previous behavior. A named registration is selected by a stable identity containing codec, direction, classification, CLSID, and losslessly encoded hardware URL. Friendly names are display text, so renaming a registration does not change its identity. A missing explicit registration produces an error rather than silently choosing another one.

The Microsoft H.264 decoder has a software MFT catalog classification but can execute D3D11 accelerated decoding. The UI presents GPU/CPU execution options for this registration and reports the path that actually produces frames. Catalog classification is unchanged.

Editable settings:
- Target bitrate: 1–200 Mbps.
- Keyframe interval: 1–600 frames.
- Low latency encoding: on/off.
- Independent encoder and decoder registration/execution policy.

Apply Video validates the whole draft, saves it, and resets only the codec loop after the native UI callback returns. Capture source selection remains intact. Unapplied numeric drafts survive unrelated UI state refreshes. Settings survive application restart. Explicit diagnostic overrides remain available and are identified in video status.

H.264 remains the implemented codec. HEVC/AV1 capability evidence does not imply an application backend exists for those codecs. CPU codec selection still uses D3D11 for conversion and rendering.

## Runtime evidence and identity

Reference machine: CDQ-P, Windows 10 build 19045, Intel Core Ultra 5 245K, Intel Graphics. No AMD/NVIDIA physical qualification is claimed.

The tested Release application and probe identities are recorded in [qualification-identity.json](../evidence/fps-006-codec-controls-20261016/qualification-identity.json). The copied portable qualification application matched the Release binary; restart-identity.json records its hash and restored UI values. Tests used an owned synthetic moving WGC window and isolated portable preferences.

### Native bitstream matrix

Each cell below is a separate real 1920×1080 H.264 encode/decode run with 120 input frames. Every run encoded and decoded 120/120, had zero quality failures, queue maximum 1, zero drops, and passed the exact frame-37 force-keyframe check.

| Encoder | Decoder | Encoding low latency ON, observed fps | Encoding low latency OFF, observed fps | GPU encoder/decoder samples |
|---|---|---:|---:|---:|
| GPU | GPU | 59.3918 | 58.5901 | 120 / 120 |
| CPU | GPU | 59.5988 | 58.3124 | 0 / 120 |
| GPU | CPU | 59.1177 | 58.7247 | 120 / 0 |
| CPU | CPU | 59.3690 | 56.1382 | 0 / 0 |

These are short functional/regression runs, including startup/drain, with other qualification activity present. They are not new sustained 1080p60 qualifications. The previous FPS-006 180-second WGC qualification remains preserved with its original source identity and results.

### Actual application UI

The final binary was restarted with saved exact-registration choices, 12 Mbps, GOP 90, and encoding low latency off. Each of GPU/GPU, CPU/GPU, GPU/CPU, and CPU/CPU was selected through the production native dropdown notifications and applied during active WGC capture. Every pair produced the requested live CPU/GPU status and preserved the source.

Additional PASS checks:
- invalid bitrate 0 is rejected without changing persisted preferences;
- editing bitrate followed by unrelated Always on Top state refresh preserves the draft;
- preferences and codec choices restore after restart;
- Stop/Start restores decoded preview;
- Settings open/hidden viewer stays 1264×691;
- F11 uses the complete 2560×1440 client; Esc restores 1264×691;
- a deliberately removed saved encoder ID displays “Selected H264 encoder unavailable” in the VIDEO section;
- selecting an available encoder and applying recovers GPU encode/CPU decode without changing the capture source.

Evidence: four-mode-ui.json, layout-and-restart.json, missing-codec-and-recovery.json, final-ui-session.json, fault-and-layout-session.json, and GPU/GPU / CPU/CPU screenshots in the linked evidence directory.

## Retained failures and fixes

1. RED persistence tests failed because the old settings serializer discarded all codec choices and advanced settings. GREEN verifies independent mixed modes and rejects malformed/out-of-range values. Pipeline tests independently verify the encoder and decoder receive their respective policies and configuration.

2. Disabling low latency for the synchronous Microsoft decoder exposed a native wait, reproduced outside the UI with a bounded probe and captured with noninvasive cdb stack inspection. Correcting the property type alone did not resolve that buffered-mode wait. This change therefore preserves the qualified real-time decoder invariant and labels the exposed option Low latency encoding. Buffered decoder scheduling is not implemented or claimed here. Raw trace evidence is retained; a general native-call timeout fix is not claimed.

3. Microsoft documents VT_UI4 for its H.264 decoder's CODECAPI_AVLowLatencyMode exception, while standard implementations use VT_BOOL. The helper now tries the standard property type, then the decoder-specific integer fallback, and records successful attribute/property application. [Microsoft property reference](https://learn.microsoft.com/en-us/windows/win32/medfound/codecapi-avlowlatencymode).

4. CPU encoding with encoding low latency off initially failed at frame 9 with NV12 pool exhausted. CPU input had already been copied into its own IMFMediaBuffer but unnecessarily retained one of the eight GPU conversion textures until encoded output. CPU textures are now reusable after synchronous readback/copy, while the owned CPU samples remain retained. A separate 32-pending-frame bound prevents unbounded buffering. Hardware keeps its original eight-texture leases. Both CPU/GPU and CPU/CPU then passed 120/120 with latency off; actual UI switching also passed. The failing UI session is retained as red-cpu-encoder-lease-session.json.

5. Independent review found that initialization failure before candidate assignment omitted the error from the VIDEO section. Error presentation now works even when no encoder candidate was activated; the deliberately missing-registration runtime test passes.

A full Release build once encountered a locked capture-probe executable because the owned fixture was running from the build output. The fixture was moved to an isolated copied executable; subsequent complete Debug and Release builds/tests passed. This was a build-output file lock, not a source compilation failure.

## Verification, review, and preservation

- Full local Debug build + CTest: PASS.
- Full local Release build + CTest: PASS.
- Native default/encoding-latency-off matrices: PASS, eight runs total.
- Actual application four-pair UI matrix, persistence/validation/drafts: PASS.
- Source loss substitution rules remain unchanged; missing codec and recovery explicitly tested.
- Settings/F11/Esc/Stop/Start regressions: PASS.
- Independent read-only review plus incremental resource-lifetime/property/error review: no remaining Critical or Important findings.
- [Exact implementation CI — Debug and Release](https://github.com/funggier/FriendPushScreen/actions/runs/38833566080): PASS for d116ab13ec9422174aa9ed7d761672d14f5b42a8.
- All 15 original WIP hashes match the prior preservation manifest; see preserved-wip-check.json.
- No reset, clean, WIP deletion, main integration, or release/tag mutation was performed.

The final documentation/evidence commit receives its own exact-commit CI before overall completion is announced. FPS-006A remains planned.

## Reproduction

Build with cmake --build build --config Debug or Release --parallel 8, then ctest --test-dir build -C Debug or Release --output-on-failure.

Run build/Release/FriendPushScreenCodecLoopProbe.exe for GPU/GPU. Add --software-encode, --software-decode, or --software for the other three pairs. Add --no-low-latency to test encoding latency off; the decoder remains in real-time mode.

For application checks, use a separately copied portable application with portable.flag, start the capture-loop probe with --fixture-only --seconds 900, select its moving window in Source, and start Preview. Choose the desired codec registrations and tuning values, then Apply Video. Test with isolated preferences when modifying saved registration IDs.
