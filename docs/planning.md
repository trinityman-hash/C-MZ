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

## Architecture plan (proposed, NOT yet fully implemented or verified)

Most of this section is still a design proposal to validate as
adapters get built, not a claim about finished, verified code — flagging
that explicitly so nobody mistakes a plan for a result, same as before.
The core + host harness pieces below are now implemented and verified
(see `docs/verification.md`); the adapters are not.

**The core problem to solve**: C-MSP's registry used `k_spinlock`
directly — a Zephyr-specific, IRQ-safe locking primitive, needed because
a fault point can be armed/checked from an ISR. A portable core can't
call Zephyr APIs directly, so locking needs to go through a porting
layer.

**Porting interface** (`include/fi_port.h`, implemented once per RTOS
adapter, plus once more by the host-only test harness):

```c
typedef struct { unsigned long state; } fi_port_key_t;

fi_port_key_t fi_port_lock(void);              /* enter critical section */
void          fi_port_unlock(fi_port_key_t key); /* leave critical section */
```

Justification: Zephyr's `k_spin_lock`/`k_spin_unlock` and RIOT-OS's
`irq_disable`/`irq_restore` both already follow this exact shape — enter
a critical section, get back an opaque token representing prior state,
restore it on exit. That's not a coincidence; it's the standard pattern
for an IRQ-safe critical section on a single core. `fi_port_key_t` is a
concrete one-word struct (not left abstract) so that `src/fault_inject.c`
can be compiled with zero RTOS headers in its include path; each adapter
is responsible for making its native key fit inside that one word.
RIOT-OS's fit is asserted with reasonable confidence (`irq_disable()`'s
return type is directly known: a plain `unsigned`). Zephyr's fit is
**not yet verified** — see "Deviations from the original plan" below.
If either assumption turns out wrong once the corresponding adapter is
actually written, this section gets corrected, not defended.

**Current layout** (adapters not yet started):

```
include/
  fault_inject.h       portable core public API (fi_arm, fi_disarm,
                        fi_hit_count, fi_reset_all, FI_POINT macro) — DONE
  fi_port.h             porting interface each adapter must implement — DONE
src/
  fault_inject.c        portable core (registry logic only, calls
                         fi_port_lock/unlock, zero RTOS-specific code) — DONE
tests/
  host/                  host-only verification harness for the core —
                          DONE, but NOT one of the v0 adapters, and NOT
                          in the originally proposed layout — see
                          "Deviations from the original plan"
  zephyr/                 the CVE-2026-1679 proof, re-run against the
                           new core+adapter, must still pass — NOT STARTED
  riot/                   a real proof case for RIOT-OS (see step 6
                           below — not chosen yet) — NOT STARTED
adapters/
  zephyr/                 fi_port_zephyr.c, zephyr/module.yml, Kconfig,
                          CMakeLists.txt — ports C-MSP's proven module
                          onto the new core — NOT STARTED
  riot/                   fi_port_riot.c, RIOT module boilerplate —
                          NOT STARTED
docs/
  planning.md             this file
  verification.md         real commands/output — DONE for the core;
                           adapters not yet covered
```

## What's actually done so far

- `include/fault_inject.h` + `include/fi_port.h` — portable core public
  API and porting interface. Zero RTOS dependency: `fault_inject.h` only
  includes `<stdint.h>`, and `fi_port.h` includes nothing.
- `src/fault_inject.c` — portable registry, generalized from C-MSP's
  version. Locking now goes through `fi_port_lock()`/`fi_port_unlock()`
  unconditionally; there is no more `#ifdef __ZEPHYR__` inside this
  file, and no RTOS header is included by it, directly or indirectly.
- `tests/host/fi_port_host.c` + `tests/host/test_fault_inject_core.c` +
  `tests/host/test_disabled_compiles_out.c` + a root `Makefile` — a
  host-only verification harness for the core. **Not part of the
  originally proposed layout** — see "Deviations from the original
  plan" below for why it was added and exactly what it does and doesn't
  prove.
- Real, run-for-real results for all of the above, including sanitizer
  output and a checked (not assumed) "no symbols linked" claim for the
  disabled build: `docs/verification.md`.

Still nothing under `adapters/`, and nothing under `tests/zephyr/` or
`tests/riot/` — no RTOS adapter exists yet. Don't let a future session
assume otherwise — check the repo tree, not this sentence, since this
file could be stale by the time it's read.

## Deviations from the original plan

- **Added a host-only test harness** (`tests/host/`, root `Makefile`)
  that wasn't in the originally proposed layout. Reason: steps 1 and 2
  couldn't honestly be called "done" under this project's own ground
  rule ("every claim must be actually run and verified, not asserted")
  without compiling and exercising the code somehow, and no RTOS
  toolchain was available in the sandbox this work was done in. This
  harness plays the same role C-MSP's own host-only build played there:
  proof the core logic itself is correct, in isolation, before any RTOS
  adapter exists to build it against. It is explicitly **not** a proof
  of portability (only one port implementation — a single-threaded
  no-op — has ever been built against the core so far) and it is not
  one of the two v0 adapters. Full detail on what it does and doesn't
  establish is in `docs/verification.md`.
- **`fi_port_key_t`'s Zephyr fit is an unverified carried-forward
  assumption.** RIOT-OS's fit in one `unsigned long` is asserted with
  reasonable confidence — `irq_disable()`'s return type is directly
  known. Zephyr's `k_spinlock_key_t` fitting in one word is based on
  general familiarity with the spinlock API's shape, not on checking an
  actual Zephyr checkout (none was reachable in this sandbox). This
  must be verified for real during step 3 below, before it's trusted —
  see the comment block in `include/fi_port.h`.
- **No SPDX license headers were added** to any new file, unlike
  C-MSP's convention, and no `LICENSE` file exists in this repo yet.
  C-MSP's own handoff history records a prior session claiming a
  LICENSE file existed when it didn't — deliberately not repeating that
  pattern here by asserting an SPDX tag with no LICENSE file backing
  it. This is an open item for the user to decide (add a LICENSE file
  and headers can follow C-MSP's Apache-2.0 convention, or choose
  otherwise), not an oversight.

## Next concrete steps, in order

1. ~~Scaffold `include/fault_inject.h` + `include/fi_port.h`~~ — done,
   see "What's actually done so far" above.
2. ~~Implement `src/fault_inject.c` against the port interface only~~ —
   done, see above.
3. Port the Zephyr adapter (`adapters/zephyr/`) from C-MSP's already-
   proven implementation onto the new core. This is also the point at
   which the "`k_spinlock_key_t` fits in one `unsigned long`" assumption
   above needs real verification against an actual Zephyr checkout, not
   another carried-forward assumption.
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
8. Append real verification results for steps 3–7 to
   `docs/verification.md` — append, don't overwrite the log already
   there for the core.
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
  — the code this was generalized from. Now largely superseded within
  this repo by `include/fault_inject.h` / `src/fault_inject.c` here, but
  still the proven reference for any conflict.
- This repo's own `docs/verification.md` — real results for what's been
  built so far.
