# FPS-001 Engineering Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Convert FriendPushScreen from a specification-only repository into a clean, traceable development repository ready for implementation.

**Architecture:** Keep the existing V1 architecture document as the master product/technical specification. Add lightweight project navigation, task/roadmap state, architecture decision gates, and qualification evidence without prematurely creating product-code structure.

**Tech Stack:** Git, GitHub, Markdown; Windows implementation stack remains intentionally undecided until technical spikes resolve the architecture gates.

**Spec:** `docs/development-plan/ScreenLink_V1_Architecture_and_Development_Plan_TH.md`

## Global Constraints

- V1 is video-only; no audio subsystem is part of the V1 feature scope.
- Target Windows 10 Version 2004+ and Windows 11 x64.
- Two peers may send and view simultaneously.
- Monitor, Window, and generic Capture Card sources are V1 requirements.
- H.264 is the compatibility baseline; HEVC/AV1 are capability-dependent.
- No auto-share after application restart.
- Direct P2P is preferred when networking permits.
- Do not commit to a transport reuse/fork/native strategy until the technical spike provides evidence.

## Review Focus

- Documentation must not imply product implementation exists when the repository is still pre-code.
- Temporary name `ScreenLink` in the master specification must be clearly distinguished from repository/project name `FriendPushScreen`.
- Roadmap decomposition must preserve the V1 scope and not introduce V2 features.
- Architecture decision gates must stay unresolved where the specification explicitly requires measurement.
- Workspace README edits must preserve all existing project documentation.

---

### Task 1: Repository navigation and governance baseline

**Files:**
- Create: `README.md`
- Create: `docs/README.md`
- Create: `docs/development/tasks/CURRENT.md`
- Create: `docs/development/tasks/ROADMAP.md`
- Create: `docs/development/QUALIFICATION-MATRIX.md`
- Create: `docs/architecture/README.md`
- Create: `docs/architecture/DECISION-GATES.md`

**Interfaces:**
- Consumes: existing V1 master specification.
- Produces: stable navigation and task/decision tracking used by subsequent FPS tasks.

- [ ] Write repository baseline documents.
- [ ] Verify every referenced path exists and every roadmap item maps back to V1 scope.
- [ ] Verify no product implementation is claimed.

### Task 2: Workspace registration

**Files:**
- Modify: `T:\Space\Projects\README.md`

**Interfaces:**
- Consumes: current workspace organization rules.
- Produces: canonical local location for program projects and FriendPushScreen.

- [ ] Add `ProjectsProgram` to the workspace tree.
- [ ] Add a Program Projects section describing FriendPushScreen.
- [ ] Preserve existing AI project sections byte-for-byte except required surrounding insertion.

### Task 3: Qualification evidence and publishable branch

**Files:**
- Create: `docs/development/reports/FPS-20261014-001-engineering-baseline.md`
- Modify: `docs/development/tasks/CURRENT.md`
- Modify: `docs/development/QUALIFICATION-MATRIX.md`

**Interfaces:**
- Consumes: repository and workspace verification output.
- Produces: reproducible evidence for closing FPS-001 and activating FPS-002.

- [ ] Run structural and Git verification.
- [ ] Record exact baseline/head SHAs and working-tree state.
- [ ] Mark FPS-001 GREEN only if all acceptance checks pass.
- [ ] Commit on `fps-001-engineering-baseline`.
- [ ] Push the branch to GitHub for review/integration.
