# C-MZ — Portable Fault-Injection Core for Embedded/RTOS Systems

[![ci](https://github.com/trinityman-hash/C-MZ/actions/workflows/ci.yml/badge.svg)](https://github.com/trinityman-hash/C-MZ/actions/workflows/ci.yml)

A small, deterministic fault-injection primitive — arm/disarm/hit-count,
compiles to nothing when disabled — generalized out of
[C-MSP](https://github.com/trinityman-hash/C-MSP)'s Zephyr-only
implementation into a portable core plus a small porting layer, with
real adapters for **two** independent RTOSes on top of it: **Zephyr**
and **RIOT-OS**.

## Why a portable core, and why two RTOSes specifically

A fault point lets a test force a specific call to fail, or return an
externally-controlled bad value, deterministically — so an
error-recovery path can be proven to actually work instead of just
assumed to. C-MSP proved that idea out for Zephyr. This repo generalizes
it: the registry logic (`src/fault_inject.c`) has zero RTOS-specific
code, and talks to whatever RTOS it's running under through a two-
function porting interface (`include/fi_port.h`) instead.

A "portable" core that's only ever been built against one RTOS hasn't
actually demonstrated portability — its porting-layer assumptions stay
untested. RIOT-OS is the deliberate second data point, chosen for a real
reason: while researching C-MSP's Zephyr issue outreach, RIOT-OS's own
issue history turned up a recurring, evidenced pattern of tests passing
while the device had actually hard-faulted, plus several separate
nondeterministic-failure/CI-reliability issues with no systematic
tooling behind them. Not picked arbitrarily.

## Relationship to C-MSP

C-MSP stays as-is — a complete, working, Zephyr-specific proof of the
underlying idea, plus its own CVE-2026-1679 reproduction. This repo
doesn't replace it; it generalizes the primitive C-MSP proved out, and
re-runs that same CVE-2026-1679 proof against the new, generalized core
as a regression check (see `docs/verification.md`). Where a design
choice here ever conflicted with a hard lesson from C-MSP's own
`docs/verification.md`, C-MSP won.

## Quick start

**Host-only, no RTOS needed** (fastest way to see the core itself work):
```sh
make test
```
Builds and runs two binaries under ASan+UBSan against the core's own
registry logic (`src/fault_inject.c`) via a no-op stub port — a fast
correctness check of the core in isolation, *not* proof of portability
(that requires the two real adapters below).

**Zephyr, via Twister** (real critical-section locking, `k_spin_lock`):
```sh
west twister -p native_sim -T tests/zephyr/eswifi_recv \
  -x=ZEPHYR_EXTRA_MODULES=/path/to/this/repo
```
Runs the CVE-2026-1679 regression proof (ported from C-MSP) as a genuine
Ztest suite on `native_sim`. Needs `cmake`, `ninja`,
`device-tree-compiler`, `gcc-multilib` (native_sim defaults to a 32-bit
build on x86_64), and `ZEPHYR_TOOLCHAIN_VARIANT=host` — no Zephyr SDK or
physical hardware.

**Zephyr, across real target architectures** (adapter smoke test,
`native_sim` + two QEMU-emulated targets, not just the host arch):
```sh
west twister -p native_sim -p qemu_cortex_m3 -p qemu_x86 \
  -T tests/zephyr/smoke \
  -x=ZEPHYR_EXTRA_MODULES=/path/to/this/repo
```
Same adapter (`adapters/zephyr/fi_port_zephyr.c`), same test file, run on
three genuinely different `k_spin_lock` implementations: `native_sim`
(POSIX-signal-based), `qemu_cortex_m3` (real ARMv7-M PRIMASK/BASEPRI
masking), `qemu_x86` (real x86 `cli`/`sti`). Needs the Zephyr SDK's
`arm-zephyr-eabi` and `x86_64-zephyr-elf` toolchains in addition to the
host toolchain `eswifi_recv` alone needs — see
`tests/zephyr/smoke/src/test_fault_inject_smoke.c`'s header for why
these two targets specifically. No physical hardware; QEMU only.

**RIOT-OS, via its own build system** (real critical-section locking,
`irq_disable`/`irq_restore`):
```sh
RIOTBASE=/path/to/RIOT BOARD=native make
```
run from `tests/riot/smoke/` (adapter smoke test) or
`tests/riot/nimble_scanlist_recv/` (the CVE-2024-32018 regression
proof — a real bug, found by actual research, not invented; see
`docs/verification.md`). No cross-compiler or physical hardware needed
for `BOARD=native`.

## Usage

```c
#include "fault_inject.h"

int eswifi_socket_recv(struct eswifi_socket *sock, struct eswifi_hw_mock *hw,
                        const uint8_t *hw_data)
{
    /* Normally: eswifi_hw_read_length(hw). A test can arm
     * FI_ESWIFI_RECV_LEN to substitute an attacker/hardware-controlled
     * value here instead, without touching eswifi_hw_read_length at all. */
    int len = FI_POINT(FI_ESWIFI_RECV_LEN, eswifi_hw_read_length(hw));

    if (len < 0 || (size_t)len > sizeof(sock->rx_buf)) {
        return ESWIFI_EMSGSIZE;
    }

    memcpy(sock->rx_buf, hw_data, (size_t)len);
    sock->rx_len = (size_t)len;
    return ESWIFI_OK;
}
```

From the test side (identical on Zephyr/Ztest, RIOT, or the host
harness — this is the whole point of the porting layer):
```c
fi_arm(FI_ESWIFI_RECV_LEN, 9999);      /* force an oversized length */
/* assert the call now safely rejects it, instead of overflowing */
fi_disarm(FI_ESWIFI_RECV_LEN);
```

Full API in `include/fault_inject.h`: `fi_arm`, `fi_disarm`,
`fi_reset_all`, `fi_hit_count`, `fi_should_fail`, and the `FI_POINT`
macro itself. Each RTOS adapter implements exactly two functions
(`include/fi_port.h`): `fi_port_lock()` / `fi_port_unlock()`.

## How this is verified

Every claim above — both adapters' locking assumptions checked against
real upstream source, both regression proofs' fixed variant passing and
buggy variant genuinely crashing under a sanitizer at the exact injected
fault, and two real build-system bugs found and fixed by actually
linking real applications rather than by re-reading the code — was
actually run, not asserted. Full commands and real output:
**`docs/verification.md`**.

## Status

All of steps 1–8 from the original roadmap are complete and verified —
portable core, host harness, both adapters, both regression proofs. See
`docs/verification.md` for the real evidence.

Beyond the original 8 steps: the Zephyr adapter is now additionally
verified on two real QEMU-emulated targets besides `native_sim` —
`qemu_cortex_m3` and `qemu_x86` — via `tests/zephyr/smoke/`, run through
the same `west twister` path CI uses, 21/21 test cases passing across
all three platforms. This exercises `k_spin_lock` under two genuinely
different real interrupt-masking mechanisms (ARMv7-M PRIMASK/BASEPRI,
x86 `cli`/`sti`), not just `native_sim`'s POSIX-signal stand-in or the
build host's own scheduler timing. RIOT-OS's board coverage is
unchanged (`BOARD=native` only) — this expansion is Zephyr-only so far.

CI (`.github/workflows/ci.yml`) now actually runs `host`, `zephyr`, and
`riot` on every push and pull request to `main`, on GitHub-hosted
runners — not just by hand. The badge above reflects the real state of
`main`, not an aspiration; see the workflow file's own comments for the
run history (first attempt failed both RTOS jobs for documented reasons,
fixed, now green) and what each job does and doesn't establish.

Deliberately not done, per the project's own v0 scope, not missing work:
- More than two RTOS adapters (no FreeRTOS, NuttX, etc.)
- Probabilistic/interval/budget-based failure, a Shell live-arming
  interface, additional fault kinds beyond "force a return value" — all
  already out of scope for C-MSP itself, carried forward here
- RIOT-OS board coverage beyond `BOARD=native`, and any physical
  hardware for either RTOS — Zephyr now also covers `qemu_cortex_m3` and
  `qemu_x86` (see above and `tests/zephyr/smoke/`), but that's still
  QEMU, not hardware, and still Zephyr-only
- Multi-core targets for either RTOS — both adapters' locking reasoning
  is explicitly single-core only (see each adapter's own file comments)
- Publishing or announcing this anywhere, including upstream against
  either project's issue tracker

## Layout

```
include/
  fault_inject.h            public API (fi_arm, fi_disarm, fi_hit_count,
                             fi_reset_all, fi_should_fail, FI_POINT)
  fi_port.h                 porting interface each adapter implements
src/
  fault_inject.c             portable registry -- zero RTOS-specific code,
                              calls fi_port_lock()/fi_port_unlock() only
Makefile                     host-only build (make test) -- no RTOS required
adapters/
  zephyr/                    fi_port_zephyr.c, Kconfig, CMakeLists.txt
  fault_inject/               RIOT-OS module: fi_port_riot.c, Makefile,
                              Makefile.include (directory name is the
                              module name -- required by RIOT's own
                              module discovery, see that Makefile)
zephyr/module.yml             west module declaration (points at adapters/zephyr)
tests/
  host/                       host-only verification harness for the core
                              (no-op stub port -- not one of the two real
                              adapters)
  zephyr/eswifi_recv/         CVE-2026-1679 regression proof, ported from
                              C-MSP, re-run against this repo's core+adapter
  zephyr/smoke/                Zephyr adapter smoke test, run on native_sim +
                              qemu_cortex_m3 + qemu_x86 -- three genuinely
                              different k_spin_lock implementations
  riot/smoke/                 RIOT-OS adapter smoke test
  riot/nimble_scanlist_recv/  CVE-2024-32018 regression proof (real bug,
                              found by research -- see docs/verification.md)
docs/
  verification.md             full real-command/real-output verification log
```

`docs/planning.md`, the working scratchpad this project was built from,
has been deleted per its own final step now that the work it was
tracking is done — its content lives on in this README and in
`docs/verification.md`'s real results.

## License

Apache License, Version 2.0 — see [`LICENSE`](LICENSE). No SPDX headers
are added to individual source files; the LICENSE file alone governs.
