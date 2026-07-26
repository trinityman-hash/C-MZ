# C-MZ — Portable Fault-Injection Core for Embedded/RTOS Systems

**Status: planning/scaffolding stage.** Nothing is built yet — see
`docs/planning.md` for the live status, design decisions, and exact next
steps. This file will be rewritten properly once there's real, verified
work to describe (see [C-MSP](https://github.com/trinityman-hash/C-MSP)
for what that looks like when it's done).

## What this is

A generalization of the deterministic fault-injection primitive built in
[C-MSP](https://github.com/trinityman-hash/C-MSP) — arm/disarm/hit-count,
compiles to nothing when disabled — pulled out of its Zephyr-only form
into a portable core with a small, explicit porting layer, plus adapters
for individual RTOSes on top of it.

Planned v0 scope: the portable core, plus exactly two adapters —
**Zephyr** (porting the already-proven C-MSP implementation onto the new
core) and **RIOT-OS** (a second real target, chosen because of evidenced,
recurring test-reliability pain in that project's own issue history, not
picked arbitrarily).

## Why a second RTOS matters here

A "portable" library that's only ever run against one RTOS hasn't
actually proven it's portable — the porting layer's assumptions stay
untested. RIOT-OS is the deliberate stress test: different locking
primitives, different build system, different conventions. If the same
core works cleanly against both without Zephyr-specific assumptions
leaking through, that's real evidence, not a claim.

## Relationship to C-MSP

C-MSP stays as-is — a complete, working, Zephyr-specific proof of the
underlying idea, plus the CVE-2026-1679 reproduction that demonstrates
why it matters. This repo doesn't replace it; it generalizes the
primitive C-MSP proved out. Where a design choice here contradicts a
lesson learned there, C-MSP's `docs/verification.md` is the tie-breaker.

## Contributing / picking this back up

Full context, current status, and exact next steps live in
`docs/planning.md`. Read that before doing anything else — it's written
so a fresh session (human or Claude) can resume without re-deriving
context. It will be deleted once the project is complete and this
README has been rewritten to describe a finished thing instead of a
plan.
