# Current Development Task

## FPS-002 — Reference Baseline & Technical Spike

**Status:** GREEN / CLOSED

**Branch:** `fps-002-reference-baseline-spike`

**Started:** 2026-10-14

**Closed:** 2026-10-14

**GitHub Issue:** #3 — FPS-002 — Reference Baseline & Technical Spike

### Goal

Collect real-machine evidence needed to choose FriendPushScreen implementation paths without turning assumptions into architecture.

### Probe scope

- Windows / CPU / GPU baseline
- Tailscale installation, peer state, and path diagnostics
- Sunshine / Moonlight installation and available version evidence
- Onten OTN-332 / generic video-capture device detection and runtime modes where possible
- WDA_EXCLUDEFROMCAPTURE feasibility on the target Windows baseline
- hardware encode/decode capability evidence available on the current machine
- determine which remaining measurements require a second peer or a later throwaway harness

### Rules

- No production application code.
- Any temporary probe code must be labeled throwaway and must not become product code automatically.
- Do not close a decision gate from documentation alone when the gate calls for runtime measurements.
- Record NOT TESTED / BLOCKED separately from FAIL.

### Completion gate

FPS-002 is GREEN only when the available reference-machine probes are recorded with reproducible evidence and every unresolved item is explicitly assigned to a later qualification step.

### Evidence

`docs/development/reports/FPS-20261014-002-reference-baseline-technical-spike.md`

### Qualification ruling

The project currently has one Windows development machine plus a smartphone, not a second physical Windows peer. FPS-002 therefore uses the local Windows machine as the reference qualification baseline. Two-peer Direct/DERP and end-to-end stream measurements are deferred to FPS-009/FPS-010 network qualification and are not required to close FPS-002.

This ruling changes the development qualification method only. It does not remove the eventual two-peer sharing requirement from FriendPushScreen V1.

### Expected outputs

- `docs/development/reports/FPS-20261014-002-reference-baseline-technical-spike.md`
- architecture-gate recommendations, without premature irreversible choices

### Next planned task

`FPS-003 — Windows App Skeleton & Hardware Diagnostics`

No development task is ACTIVE after FPS-002 closure.
