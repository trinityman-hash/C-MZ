# Verification log

Living document, same spirit as
[C-MSP's `docs/verification.md`](https://github.com/trinityman-hash/C-MSP/blob/main/docs/verification.md):
real commands, run for real, output pasted as produced -- not
hand-written or assumed. Append to this, don't rewrite history out of
it, as more steps from `docs/planning.md` get verified.

## Step 1 + 2: portable core + host stub port

Scope of what's checked here: the portable registry logic in
`src/fault_inject.c` and the `FI_POINT` contract in
`include/fault_inject.h`, built against the host-only stub port
(`tests/host/fi_port_host.c`). **This does NOT verify portability** --
that requires a real second locking implementation (the Zephyr and/or
RIOT-OS adapters), which don't exist yet. What this does verify: the
core's own logic is correct in isolation, under sanitizers, before
anything gets built on top of it.

Toolchain used: host `gcc` (Ubuntu 24.04 container, gcc 13.3.0), no
cross-compiler, no RTOS toolchain -- none was available in the sandbox
this was run in. `clang` was not available in that environment, so the
Clang half of the "GCC/Clang statement-expression" portability claim in
`fault_inject.h` is NOT verified here; only GCC is.

```
$ make test
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4 -Iinclude src/fault_inject.c tests/host/fi_port_host.c tests/host/test_fault_inject_core.c -o build/test_core
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g -Iinclude tests/host/test_disabled_compiles_out.c -o build/test_disabled
=== core test (expected: all checks pass) ===
./build/test_core
test_disarmed_passes_through:
  ok: returns the real call's result when disarmed
  ok: real call was evaluated exactly once
test_armed_injects_and_skips_real_call:
  ok: returns the armed injected value
  ok: real call was NOT evaluated -- the fault genuinely replaced it
test_disarm_restores_passthrough:
  ok: passes through again after disarm
  ok: real call evaluated again after disarm
test_hit_count_increments_armed_or_not:
  ok: hit count counts a disarmed evaluation
  ok: hit count also counts an armed evaluation
test_points_are_independent:
  ok: point A is armed
  ok: point B is unaffected by arming A
  ok: point A hit count only reflects point A
  ok: point B hit count only reflects point B
test_reset_all_clears_everything:
  ok: point A hit count cleared
  ok: point B hit count cleared
  ok: point A no longer armed after reset
test_table_full_is_handled_safely:
  ok: an entry created while the table had room works normally
  ok: a new id past table capacity is treated as not-armed, not a crash
  ok: an id that couldn't be allocated has no hit count (it was never stored)
  ok: existing entries survive a failed allocation attempt

all checks passed

=== disabled build (expected: all checks pass, no FI symbols linked) ===
./build/test_disabled
  ok: FI_POINT is a pass-through with fault injection compiled out
all checks passed
```

Both binaries built clean under `-Wall -Wextra -Werror` plus
`-fsanitize=address,undefined` -- zero warnings, zero sanitizer
findings, on both the fault-injection-enabled and fault-injection-
disabled builds.

The "no FI symbols in the disabled build" claim (`fault_inject.h`'s
whole reason for existing) was checked, not assumed:

```
$ nm build/test_disabled | grep -i "fi_"
NO fault-injection symbols found in test_disabled (confirmed)
```

## What this establishes

- The registry logic ported cleanly from C-MSP's Zephyr-specific
  version to the port-abstracted version, with identical behavior
  (arm/disarm/hit-count/reset, table-full handled without crashing,
  independent fault points don't interfere with each other).
- `FI_POINT` genuinely skips evaluating the real call when armed (not
  just overriding the result after the fact) -- checked with a
  call-count side effect, not inferred from the return value alone.
- `FI_POINT` compiles to exactly the original call, with zero
  fault-injection machinery linked in, when `CONFIG_FAULT_INJECTION` is
  undefined.
- All of the above is clean under AddressSanitizer and
  UndefinedBehaviorSanitizer.

## What wasn't (yet) verified here -- do not read more into this log than this

- **Portability itself.** Only one port implementation
  (`fi_port_host.c`, a single-threaded no-op) has been built against
  the core. `fi_port.h`'s core design bet -- that a Zephyr-shaped and a
  RIOT-OS-shaped critical section both fit through the same two-function,
  one-word-token interface -- is unverified until adapters/zephyr/ and
  adapters/riot/ both exist and build for real (docs/planning.md steps
  3 and 5).
- **Real concurrency / real IRQ context.** `fi_port_host.c` is a no-op;
  nothing here proves the locking discipline is correct under an actual
  scheduler or actual interrupts, because there isn't one in this build.
- **`k_spinlock_key_t` actually fitting in `fi_port_key_t`'s one
  `unsigned long`.** Flagged as an open assumption in `fi_port.h`; not
  checked against a real Zephyr checkout yet (no Zephyr toolchain
  available in the sandbox this was written in).
- **Clang.** Not installed in the sandbox this was verified in; only
  GCC 13.3.0 was exercised.
- **The Zephyr regression check** (docs/planning.md step 4: re-running
  the CVE-2026-1679 proof against the new core+adapter) -- can't happen
  before the Zephyr adapter itself exists.
- Anything RIOT-OS-related at all -- no RIOT-OS toolchain has been set
  up anywhere in this project yet.
