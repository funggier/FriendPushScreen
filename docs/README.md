# FriendPushScreen Documentation

This directory separates stable product intent from current execution state.

## Master specification

- `development-plan/ScreenLink_V1_Architecture_and_Development_Plan_TH.md` — V1 product specification, architecture blueprint, milestone sequence, risks, and acceptance criteria.

## Build and runtime baseline

- `BUILDING.md` — Windows x64 prerequisites and CMake build/test commands.
- `development/reports/FPS-20261014-003-windows-app-skeleton-hardware-diagnostics.md` — FPS-003 native application qualification evidence.

## Current development state

- `development/tasks/CURRENT.md` — exactly what is active now.
- `development/tasks/ROADMAP.md` — ordered FPS task sequence.
- `development/QUALIFICATION-MATRIX.md` — evidence required before a task can be called GREEN.
- `development/reports/` — dated evidence and handoff-style reports.

## Architecture governance

- `architecture/README.md` — architecture-document rules.
- `architecture/DECISION-GATES.md` — decisions intentionally left open until evidence is collected.

## Planning

- `superpowers/plans/` — implementation plans for scoped development tasks.

## Rule

Do not duplicate the full V1 specification into task files. Task files state current execution and evidence; the master specification remains the product/architecture authority unless an explicit later decision supersedes it.
