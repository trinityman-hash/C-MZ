# C-MZ project — planning & handoff

Living document. Update this as work happens; don't let it drift out of
sync with reality. Delete it once the project is complete and `README.md`
has been rewritten to describe a finished thing instead of a plan —
that was the explicit agreement when this file was created.

## What this project is

A portable, RTOS-agnostic fault-injection core, generalized out of the
Zephyr-only `fault_inject` module built in
[C-MSP](https://github.com/trinityman-hash/C-MSP). Same idea — force a
specific call to fail/return a bad value during a test, deterministically,
so an error-recovery path can be proven to actually work — but split into
a portable core plus a small porting layer, with adapters for individual
RTOSes built on top.

v0 target: exactly **two** adapters — **Zephyr** and **RIOT-OS** — not
more. A "portable" core that's only ever exercised against one RTOS
hasn't actually proven portability; RIOT-OS is the deliberate second data
point, not a nice-to-have.

## Why RIOT-OS specifically, not another RTOS

Not arbitrary. While working on C-MSP's Zephyr issue outreach, a search
across RIOT-OS's real issue history turned up a recurring, evidenced
pattern: tests reporting PASS while the device had actually hard-faulted
right after (RIOT fixed one instance of this themselves in 2021 —
"Testrunner: Detect hard fault after the test is finished"), plus
multiple separate nondeterministic-failure and CI-reliability issues over
several years with no systematic tooling behind them. That's real
evidence the testing-infrastructure gap in that ecosystem is genuine, not
a guess.

## Ground rules, carried forward from C-MSP — these still apply

- **Quality over speed, no compromises.** Same as before.
- **Every claim must be actually run and verified**, not asserted. This
  matters even more here than in C-MSP: a "portable" claim is exactly the
  kind of thing that's easy to assert and hard to actually prove without
  a second real target.
- Apply the **Andrej Karpathy coding guidelines** skill
  (`/mnt/skills/plugins/andrej-karpathy-skills:karpathy-guidelines/SKILL.md`)
  — simplicity, surgical changes, surface assumptions, verifiable success
  criteria. Load it fresh in any new session before code work.
- "Any better tech" is fine as long as it's better in most aspects, not a
  narrow trade-off.
- If a design decision here ever conflicts with a hard lesson from
  C-MSP's `docs/verification.md`, C-MSP wins — it's the proven reference.

## Architecture plan (proposed, NOT yet implemented or verified)

Everything in this section is a design proposal to validate once code
actually exists, not a claim about working code. Flagging that explicitly
so nobody mistakes a plan for a result — that mistake already happened
once in C-MSP's own handoff history (a prior session claimed a LICENSE
file and SPDX headers existed when they didn't) and it's not repeating
here.

**The core problem to solve**: C-MSP's registry used `k_spinlock`
directly — a Zephyr-specific, IRQ-safe locking primitive, needed because
a fault point can be armed/checked from an ISR. A portable core can't
call Zephyr APIs directly, so locking needs to go through a porting
layer.

**Proposed porting interface** (`include/fi_port.h`, implemented once per
RTOS adapter):

```c
typedef /* RTOS-defined, opaque to the core */ fi_port_key_t;

fi_port_key_t fi_port_lock(void);              /* enter critical section */
void          fi_port_unlock(fi_port_key_t key); /* leave critical section */
```

Justification: Zephyr's `k_spin_lock`/`k_spin_unlock` and RIOT-OS's
`irq_disable`/`irq_restore` both already follow this exact shape — enter
a critical section, get back an opaque token representing prior state,
restore it on exit. That's not a coincidence; it's the standard pattern
for an IRQ-safe critical section on a single core. If this assumption
turns out to be wrong once the RIOT-OS adapter is actually written, this
section gets corrected, not defended.

**Proposed layout:**

```
include/
  fault_inject.h       portable core public API (fi_arm, fi_disarm,
                        fi_hit_count, fi_reset_all, FI_POINT macro)
  fi_port.h             porting interface each adapter must implement
src/
  fault_inject.c        portable core (registry logic only, calls
                         fi_port_lock/unlock, zero RTOS-specific code)
adapters/
  zephyr/                fi_port_zephyr.c, zephyr/module.yml, Kconfig,
                          CMakeLists.txt — ports C-MSP's proven module
                          onto the new core
  riot/                   fi_port_riot.c, RIOT module boilerplate
tests/
  zephyr/                 the CVE-2026-1679 proof, re-run against the
                           new core+adapter, must still pass
  riot/                   a real proof case for RIOT-OS (see step 6
                           below — not chosen yet)
docs/
  planning.md             this file
  verification.md         real commands/output, created once there's
                           something to verify
```

## What's actually done so far

Nothing but this file and `README.md`. No code exists yet. Don't let a
future session assume otherwise — check the repo tree, not this
sentence, since this file could be stale by the time it's read.

## Next concrete steps, in order

1. Scaffold `include/fault_inject.h` + `include/fi_port.h` — pure
   portable design, zero RTOS dependency, nothing to build yet.
2. Implement `src/fault_inject.c` against the port interface only.
3. Port the Zephyr adapter (`adapters/zephyr/`) from C-MSP's already-
   proven implementation onto the new core.
4. **Critical regression check**: re-run the exact CVE-2026-1679 proof
   from C-MSP against the new core+Zephyr-adapter combination. If
   generalizing the core broke or weakened that proof, that's a stop-
   and-fix, not a footnote.
5. Implement the RIOT-OS adapter (`adapters/riot/`).
6. Find (or, if nothing suitable exists, build) a real RIOT-OS bug
   reproduction for the proof case — same discipline as C-MSP's
   CVE-2026-1679 choice: a real bug pattern, not an invented one. Needs
   actual research before picking one, not an assumption.
7. Verify both adapters for real, in their real respective build/test
   systems (west/Twister for Zephyr — environment setup already known
   from C-MSP; RIOT-OS's own build system — not yet set up in any
   sandbox, needs doing from scratch).
8. Write `docs/verification.md` with real commands and real output,
   matching C-MSP's standard.
9. Rewrite `README.md` to describe the finished thing.
10. Delete this file.

## Deliberately out of scope for v0

- More than two RTOS adapters (no FreeRTOS, NuttX, etc. yet).
- Anything C-MSP itself already scoped out: probabilistic/interval-based
  failure, a Shell live-arming interface, additional fault kinds beyond
  "force a return value," non-`native_sim` boards.
- Publishing or announcing this anywhere. C-MSP's approach was: build it,
  verify it, then it's the user's call whether/where to post it — same
  here.

## Key files to read first, in a new session

- This file.
- [C-MSP's `docs/verification.md`](https://github.com/trinityman-hash/C-MSP/blob/main/docs/verification.md)
  — what "done and verified" actually looks like in this style of
  project, and the standard this repo is held to.
- [C-MSP's `include/fault_inject.h`](https://github.com/trinityman-hash/C-MSP/blob/main/include/fault_inject.h)
  and [`src/fault_inject.c`](https://github.com/trinityman-hash/C-MSP/blob/main/src/fault_inject.c)
  — the code being generalized. Read these before designing anything
  here from scratch.
