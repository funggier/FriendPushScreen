# Current Development Task

## FPS-002 — Reference Baseline & Technical Spike

**Status:** ACTIVE — LOCAL PROBE COMPLETE / BLOCKED ON TWO-PEER QUALIFICATION

**Branch:** `fps-002-reference-baseline-spike`

**Started:** 2026-10-14

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

### Current blocker

The only visible Tailscale reference peer (`XQ-DC72`) was offline during the spike. Sunshine/Moonlight end-to-end latency and Direct-vs-DERP path qualification remain untested.

### Expected outputs

- `docs/development/reports/FPS-20261014-002-reference-baseline-technical-spike.md`
- architecture-gate recommendations, without premature irreversible choices
