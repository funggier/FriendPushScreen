# Current Development Task

## FPS-001 — Engineering Baseline

**Status:** ACTIVE

**Branch:** `fps-001-engineering-baseline`

**Started:** 2026-10-14

### Goal

Convert the repository from specification-only state into a development-ready, traceable baseline without prematurely selecting implementation technologies that still require measurement.

### In scope

- repository README and documentation navigation
- task/roadmap structure
- architecture decision gates
- qualification matrix
- local workspace registration
- exact Git evidence and baseline report

### Out of scope

- capture implementation
- encoder/decoder implementation
- UI implementation
- network transport implementation
- installer
- V2 features

### Completion gate

FPS-001 becomes GREEN only when:

1. canonical local checkout exists at `T:\Space\Projects\ProjectsProgram\FriendPushScreen`
2. repository documentation points to the existing V1 master specification
3. roadmap and decision gates preserve V1 scope
4. `T:\Space\Projects\README.md` registers `ProjectsProgram\FriendPushScreen`
5. Git verification is clean after commit
6. the exact commit is pushed to GitHub on the FPS-001 branch
7. evidence report records the verification results

### Next planned task

`FPS-002 — Reference Baseline & Technical Spike`
