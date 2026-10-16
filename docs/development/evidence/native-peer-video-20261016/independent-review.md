# Independent whole-change review

Reviewer: fresh gpt-6-astra / high, read-only, no implementation delegation.
Range: 000c6a03164c1f1f15902eb3565ea357ab250589..ea2b68aa157b035a4dc5f0099be9787895d4ea11.
Read on the actual native Windows worktree; no edits, builds, tests, UI actions or workloads by the reviewer.

The reviewer found no Critical/Important security, lifetime or unbounded-queue defect. Three concrete findings were originally graded Minor:

1. Application::StopCapturePreview and the failed StartCapturePreview branch overwrite a retained incoming image with RenderPlaceholder. During a peer pause/recovery, the blank persists until a new peer frame or a redraw, including the old approximately one-second status-driven redraw.
2. Successful StartSender/StartReceiver clears the single shared error, hiding an unresolved error in the opposite direction. A saved unavailable decoder loses its explanation when the same peer starts a working outgoing share.
3. One-second UpdatePeerUi -> ApplyUiState -> LayoutChildren unconditionally invokes resize, calls ResizeBuffers at unchanged dimensions, then redraws the received frame with default Present(1). This is a deterministic additional scheduling cost; the reviewer did not attribute measured recoveries to it.

Parent re-grade: all three are Important for this user's reported flicker and independently controlled duplex usage. The first changes a valid incoming picture for an unrelated local action; the second hides the reason reception is broken; the third reintroduces blocking presentation and unnecessary swap-chain mutation on the shared video pump. They enter one grouped TDD fix pass. This does not change the reviewer's measured or unmeasured claims.

Review Focus outcomes:

- Identity/trust: fail-closed DPAPI persistence, public bundle signature/time/SAN/purpose policy, self rejection, explicit full-fingerprint replacement and normal TLS validation plus exact DER pin satisfy the scoped requirement. Copying an authorized private identity is cryptographically indistinguishable; clone detection beyond self checks/separate stores is not claimed.
- Simultaneous connection/callback/restart: deterministic selection, epoch/token/generation checks, weak consumer credits and shutdown ownership are coherent. Physical lifecycle is a separate parent gate.
- Bounds: logical transport/consumer-owned AU count and byte bounds include partial, queued, in-flight and retained work. They are not an absolute process-memory ceiling. DecodeVideo, native MFT samples and cleared metadata vector capacity add bounded working copies. The two-entry receiver tracking/recovery prevents unbounded retained metadata growth.
- Independence: core no-source receive and Stop Share are correct; the three UI/error findings above qualify the broader UX.
- Geometry/feedback: conservative signed clipping, selected-display lookup, non-mutating GPU masking, movement history and odd/portrait normalization are coherent. Physical non-primary/negative-origin monitor coverage remains unclaimed.

Declined-to-judge inputs retained as explicit parent rulings:

- Final Debug/Release/CTest/exact-commit CI: parent runs and verifies these before completion.
- Ongoing final duplex/lifecycle/native missing-decoder qualification: parent supplies actual results.
- Physical negative-origin/non-primary monitor, network capture card and second PC: no result is inferred.
- WAN/LAN/Tailscale/DNS, audio, permanent transport and distribution/legal/full V1 gates: remain open.
- Whether the extra periodic resize/present caused any measured recovery: no causal claim without runtime evidence.
- Absolute process-memory limit or cancellation of arbitrary native MFT calls: neither is promised.
- Larger previously possible local-preview resolutions: no supplied physical larger-than-network-budget success was available; prior 1080p60 baseline is retained. Separate high-resolution local regression remains unqualified.

Readiness: reviewer found no code-review blocker for the provisional No Audio loopback increment, subject to parent re-grading/fixes and remaining physical/build/CI gates. Strict uninterrupted-generation failures are reported independently of >=59 fps and transport overflow.
