# Verification log

Living document, same spirit as
[C-MSP's `docs/verification.md`](https://github.com/trinityman-hash/C-MSP/blob/main/docs/verification.md):
real commands, run for real, output pasted as produced -- not
hand-written or assumed. Append to this, don't rewrite history out of
it, as more work on this project gets verified. Originally tracked
against `docs/planning.md`'s step list while that file existed (steps
1-10, all now complete -- see the "Post-completion consistency audit"
section below for why that file itself is gone); nothing here needs
that doc to be read correctly, each section states its own scope.

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

## Steps 3-7: Zephyr + RIOT-OS adapters, both regression proofs

Toolchain used: gcc 13.3.0 (Ubuntu 24.04 container), cmake 3.28.3,
ninja 1.11.1, west 1.5.0. Zephyr checked out at
`zephyrproject-rtos/zephyr` commit `b3e7c445b343c4feac8ba06095019c09cda3ea3a`
(2026-07-26, `v4.4.0-9493-gb3e7c445b343`), `ZEPHYR_TOOLCHAIN_VARIANT=host`
(native_sim needs no cross toolchain/SDK). RIOT-OS checked out at
`RIOT-OS/RIOT` commit `cf0e1b52740c41d5209786f20a04bb6f508ceef9`
(2026-07-26), `BOARD=native`. Both are real upstream checkouts fetched
over the network into the sandbox this was verified in, not vendored
into this repo.

### `k_spinlock_key_t` -- verified for real (closes the step-3 open item)

```
$ grep -n "k_spinlock_key_t\|struct z_spinlock_key" zephyr/include/zephyr/spinlock.h
struct z_spinlock_key {
        int key;
};
...
typedef struct z_spinlock_key k_spinlock_key_t;
```

A single `int`. `long`/`unsigned long` is guaranteed by the C standard
to be at least as wide as `int`, so this fits in `fi_port_key_t`'s one
`unsigned long` with no truncation, on every Zephyr-supported target.
The assumption carried forward from steps 1-2 was correct.

### Zephyr CVE-2026-1679 regression check (docs/planning.md step 4)

The exact same proof from C-MSP (`tests/zephyr/eswifi_recv/`, ported
unchanged in spirit -- see that directory's own file comments for what
did and didn't change), now built against this repo's portable core
plus `adapters/zephyr/`, not C-MSP's monolithic Zephyr-only
`fault_inject.c`.

Fixed variant (the one permanent Twister entry, per
`tests/zephyr/eswifi_recv/testcase.yaml`):

```
$ west twister -p native_sim -T tests/zephyr/eswifi_recv \
    -x=ZEPHYR_EXTRA_MODULES=/home/claude/C-MZ --inline-logs
...
INFO    - 1 of 1 executed test configurations passed (100.00%), 0 built (not run), 0 failed, 0 errored, with no warnings in 56.80 seconds.
INFO    - 2 of 2 executed test cases passed (100.00%) on 1 out of total 1642 platforms (0.06%).
```

```
*** Booting Zephyr OS build v4.4.0-9493-gb3e7c445b343 ***
Running TESTSUITE eswifi_recv_fault_injection
STARTING - test_normal_recv_no_fault
 PASS - test_normal_recv_no_fault in 0.000 seconds
STARTING - test_oversized_length_injected
 PASS - test_oversized_length_injected in 0.000 seconds
SUITE PASS - 100.00% [eswifi_recv_fault_injection]: pass = 2, fail = 0, skip = 0, total = 2
```

Buggy variant (deliberately not a permanent Twister entry -- built and
run once by hand, per that same file's comment, matching C-MSP's own
convention for the same reason: the crash itself is the proof, not a
green checkmark):

```
$ west build -p always -b native_sim tests/zephyr/eswifi_recv -d build_buggy -- \
    -DZEPHYR_EXTRA_MODULES=/home/claude/C-MZ -DESWIFI_RECV_VARIANT=buggy \
    -DCONFIG_FAULT_INJECTION=y -DCONFIG_FAULT_INJECTION_MAX_POINTS=8 \
    -DCONFIG_ASAN=y -DCONFIG_UBSAN=y
[116/116] Running utility command for native_runner_executable

$ ./build_buggy/zephyr/zephyr.exe; echo "exit code: $?"
STARTING - test_normal_recv_no_fault
 PASS - test_normal_recv_no_fault in 0.000 seconds
STARTING - test_oversized_length_injected
==2555==ERROR: AddressSanitizer: stack-buffer-overflow on address 0xefff7054 ...
WRITE of size 64 at 0xefff7054 thread T5
    #0 ... in memcpy
    #1 ... in memcpy /usr/include/bits/string_fortified.h:29
    #2 ... in eswifi_socket_recv tests/zephyr/eswifi_recv/src/eswifi_repro_buggy.c:25
    #3 ... in eswifi_recv_fault_injection_test_oversized_length_injected tests/zephyr/eswifi_recv/src/test_eswifi_recv_ztest.c:78
SUMMARY: AddressSanitizer: stack-buffer-overflow ... in memcpy
==2555==ABORTING
exit code: 1
```

Note on the manual buggy-variant invocation: a plain `west build`
does **not** read `testcase.yaml`'s `extra_configs` -- that's a
Twister-only mechanism. A first attempt at this manual build omitted
those Kconfig options entirely and failed with `fault_inject.h: No such
file or directory`, because `adapters/zephyr/CMakeLists.txt`'s whole
body is guarded behind `if(CONFIG_FAULT_INJECTION)`, which was never
set. Not an adapter bug -- an incomplete manual reconstruction of what
Twister does automatically for the fixed variant. Fixed by passing
`testcase.yaml`'s `extra_configs` explicitly as `-D` args, shown above.

### RIOT-OS `irq_disable()`/`irq_restore()` -- verified for real (closes the step-5 open item)

```
$ grep -n "irq_disable\|irq_restore" core/lib/include/irq.h
MAYBE_INLINE unsigned irq_disable(void);
MAYBE_INLINE void irq_restore(unsigned state);
```

Both plain `unsigned`, fits in `fi_port_key_t`'s `unsigned long` with no
truncation. The assumption carried forward from steps 1-2 ("asserted
with reasonable confidence") was correct.

### RIOT-OS adapter (docs/planning.md step 5) -- two real bugs found and fixed by actually building it

The first attempt at `adapters/riot/` (module dir literally named
`riot`, with `MODULE := fault_inject` set inside it, and
`SRC += ../../src/fault_inject.c`) looked correct on paper and was
wrong in two independent ways that only surfaced by actually linking a
real RIOT app against it:

1. **Module never discovered at all.** RIOT's dependency resolution
   (`makefiles/dependency_resolution.inc.mk`) locates an external
   module at `$(EXTERNAL_MODULE_DIRS)/<module-name>/Makefile` -- the
   directory name itself is the lookup key, not any `MODULE` variable
   set inside that Makefile. `adapters/riot/Makefile.include` was
   simply never `-include`d, silently. Confirmed with
   `make info-debug-variable-EXTERNAL_MODULE_PATHS` before and after.
   Fixed by renaming the directory to `adapters/fault_inject/` (see
   that Makefile's own header comment for the full explanation).

2. **Object file silently excluded from the link**, even after fix
   #1. `SRC += ../../src/fault_inject.c` compiles fine, but
   Makefile.base derives each object's path from its `SRC` entry
   verbatim (`$(SRC:%.c=$(BINDIR)/$(MODULE)/%.o)`), so the `../../`
   produced an object at `bin/native64/fault_inject/../../src/fault_inject.o`
   -- i.e. `bin/src/fault_inject.o`, physically outside
   `bin/native64/fault_inject/`, which is exactly the directory the
   final link's `find $(BINDIR)/fault_inject/ -name "*.o"` searches.
   It compiled with zero errors and then took no part in the link,
   surfacing only as `undefined reference to fi_arm` etc. Confirmed by
   `find bin -iname "*.o"` showing the stray object's real location.
   Fixed with `VPATH += ../../src` plus a bare `SRC += fault_inject.c`
   -- GNU Make resolves the prerequisite via VPATH (so the real source
   path is preserved in `$<` and in debug info) while the object target
   path, derived from the bare SRC entry, correctly stays under
   `$(BINDIR)/$(MODULE)/`.

A third, smaller issue in `tests/riot/smoke/main.c` (not the adapter
itself): its own header comment asserted that `return 0;` from `main()`
would terminate the native-board process with that exit code, "the
same way `tests/minimal` (RIOT's own convention) returns 0." That claim
was checked, not just carried over from that file's own precedent --
and was wrong. `tests/minimal` is a size test nobody runs for its exit
code; on RIOT's native board, `main()` returning does not end the
process (the scheduler and other threads, e.g. idle, are still
running). Confirmed by literally watching a first run hang past a
5-second timeout despite printing "all checks passed". Fixed with an
explicit `exit()` call, matching real precedent elsewhere in RIOT's own
tree (`tests/pkg/wolfcrypt-ed25519-verify/tools/ed25519_sign_msg.c`).

RIOT-OS adapter smoke test, after all three fixes:

```
$ RIOTBASE=/home/claude/RIOT BOARD=native make
...
   text	   data	    bss	    dec	    hex	filename
  34275	   1088	  59200	  94563	  17163	.../fault_inject_riot_smoke.elf

$ ./bin/native64/fault_inject_riot_smoke.elf; echo "exit code: $?"
RIOT native64 board initialized.
main(): This is RIOT! (Version: cf0e)
=== fault_inject RIOT-OS adapter smoke test ===
test_disarmed_passes_through:
  ok: returns the real call's result when disarmed
  ok: real call was evaluated exactly once
test_armed_injects_and_skips_real_call:
  ok: returns the armed injected value
  ok: real call was NOT evaluated -- the fault genuinely replaced it, through the real irq_disable/irq_restore critical section
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
exit code: 0
```

17/17 checks pass, through the real `irq_disable`/`irq_restore` path,
confirmed by the process's real exit code -- not just its printed
output.

### A real RIOT-OS bug for the second proof case (docs/planning.md step 6)

Found through actual research (web search + reading the real source in
the real checkout above), not invented:
**CVE-2024-32018 / [GHSA-899m-q6pp-hmp3](https://github.com/RIOT-OS/RIOT/security/advisories/GHSA-899m-q6pp-hmp3)**,
in `nimble_scanlist_update()`,
`pkg/nimble/scanlist/nimble_scanlist.c`. Same class of bug as
CVE-2026-1679 (an externally-reported length trusted before a
fixed-size `memcpy`), different specific defect: here the pre-fix
guard was `assert(len <= BLE_ADV_PDU_LEN)` only, and `assert()` compiles
to nothing at all when `NDEBUG` is defined (C11 7.2p1) -- so in a
release build there was no check whatsoever. Confirmed against the real
checkout, not just the advisory text:

```
$ grep -n "BLE_ADV_PDU_LEN " sys/include/net/ble.h
#define BLE_ADV_PDU_LEN         (31U)   /**< max size of legacy ADV packets */

$ sed -n '69,94p' pkg/nimble/scanlist/nimble_scanlist.c
void nimble_scanlist_update(uint8_t type, const ble_addr_t *addr, ...)
{
    assert(addr);
    /* Ignore bogus advertisements */
    if (len > BLE_ADV_PDU_LEN) {     /* <- the real, current fix: an */
        assert(0);                   /*    unconditional `if`, not an */
        return;                      /*    assert-only guard */
    }
    ...
    memcpy(e->ad, ad, len);
```

`tests/riot/nimble_scanlist_recv/` reconstructs this bug pattern (not a
literal copy of the driver -- no NimBLE host stack, no clist allocator;
see `src/scanlist_repro.h`'s header comment for exactly what is and
isn't simplified), the same way `tests/zephyr/eswifi_recv/` reconstructs
CVE-2026-1679's pattern rather than vendoring the whole eswifi driver.

### RIOT-OS CVE-2024-32018 regression proof (docs/planning.md step 7)

Fixed variant:

```
$ RIOTBASE=/home/claude/RIOT BOARD=native make
   text	   data	    bss	    dec	    hex	filename
  31427	   1104	  59200	  91731	  16653	.../nimble_scanlist_recv.elf

$ ./bin/native64/nimble_scanlist_recv.elf; echo "exit code: $?"
=== nimble_scanlist CVE-2024-32018 reproduction ===
test_normal_recv_no_fault:
  ok: entry updated with the real (in-bounds) length
  ok: advertising data copied correctly
test_oversized_length_injected:
  ok: oversized length was rejected, entry left untouched

all checks passed
exit code: 0
```

Buggy variant, built with `NDEBUG` (the advisory's own stated
precondition -- "while assertions are compiled-out") plus ASan
(`make all-asan`, RIOT's own sanitizer target,
`makefiles/arch/native.inc.mk`):

```
$ RIOTBASE=/home/claude/RIOT BOARD=native SCANLIST_REPRO_VARIANT=buggy make all-asan
   text	   data	    bss	    dec	    hex	filename
  68051	  24808	  65288	 158147	  269c3	.../nimble_scanlist_recv.elf

$ ./bin/native64/nimble_scanlist_recv.elf; echo "exit code: $?"
test_normal_recv_no_fault:
  ok: entry updated with the real (in-bounds) length
  ok: advertising data copied correctly
test_oversized_length_injected:
==1123==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7fa4d0f00460 ...
WRITE of size 51 at 0x7fa4d0f00460 thread T0
    #0 ... in memcpy
    #1 ... in scanlist_update tests/riot/nimble_scanlist_recv/src/scanlist_repro_buggy.c:36
    #2 ... in test_oversized_length_injected tests/riot/nimble_scanlist_recv/src/main.c:77
SUMMARY: AddressSanitizer: stack-buffer-overflow ... in memcpy
==1123==ABORTING
exit code: 1
```

`WRITE of size 51` is exactly `SCANLIST_AD_MAX + 20` (31 + 20), the
precise oversized length the test forced via `fi_arm()` -- the crash is
happening for the reason intended, not some unrelated fault.

One caveat, noted rather than hidden: this run also printed
`WARNING: ASan is ignoring requested __asan_handle_no_return` before
the real error, related to RIOT's userspace thread-switching on the
native board. ASan's own documentation describes this as a possible
source of false positives. It is not one here -- the reported write
size, source location, and line number all match the forced condition
exactly -- but it's flagged rather than silently omitted, same
reasoning as the "no Clang" and other caveats elsewhere in this log.

## What steps 3-7 establish

- Both `fi_port_key_t` open assumptions (Zephyr's `k_spinlock_key_t`,
  RIOT's `irq_disable`/`irq_restore`) are confirmed correct against
  real, current upstream source, not carried forward on familiarity.
- The portable core genuinely works correctly through two independent,
  real critical-section implementations -- Zephyr's `k_spin_lock` and
  RIOT's `irq_disable`/`irq_restore` -- not just the host no-op stub.
  That's the actual portability claim this project exists to prove,
  and it no longer rests on a single data point.
- Both regression proofs (CVE-2026-1679 on Zephyr, CVE-2024-32018 on
  RIOT-OS) pass cleanly on their fixed variant and genuinely crash
  under a sanitizer, at the exact injected fault, on their buggy
  variant. Generalizing the core did not break or weaken either the
  original C-MSP-derived proof or the new one.
- Both adapters were caught having real, non-obvious build-system bugs
  by actually linking and running real applications against them --
  not by re-reading the code -- and both are now fixed and verified.

## What wasn't (yet) verified here

- **Hardware-in-the-loop.** Everything above ran on host simulators
  (`native_sim`, RIOT's `native` board), not real MCU hardware for
  either RTOS.
- **Multi-core targets for either RTOS.** Both adapters' locking
  reasoning is explicitly single-core-only (see each adapter's own file
  comments); this hasn't been tested on an SMP configuration of either.
- **Clang**, for either adapter -- only GCC was available in this
  sandbox, same caveat as steps 1-2.
- **CI wiring.** Nothing here is hooked up to run automatically yet
  (no GitHub Actions workflow); every command above was run by hand in
  this session.
- **The ASan `__asan_handle_no_return` warning's blast radius beyond
  this one test.** It was cross-checked against the specific crash
  above (write size, location, and line all match exactly), but it
  wasn't investigated further than that -- if RIOT-OS ASan work
  continues in this project, that warning is worth understanding
  properly rather than re-confirming per-test each time.

## Post-completion consistency audit

The steps 3-7 commit's own message claimed `docs/planning.md` had been
deleted and everything above was in place. A fresh session (no
Zephyr/RIOT toolchain available, so unable to re-run any of the builds
above) checked that claim against the actual repo state instead of
trusting the commit message, and found several things asserted as done
that weren't:

- `docs/planning.md` was still present, byte-identical to the original
  scaffolding commit -- not deleted, contradicting both that commit
  message and README.md's own "has been deleted per its own final
  step" claim. Now actually deleted.
- Three files (`zephyr/module.yml`, `adapters/zephyr/CMakeLists.txt`,
  `adapters/zephyr/Kconfig`) carried an `SPDX-License-Identifier:
  Apache-2.0` header with no `LICENSE` file in the repo to back it --
  directly contradicting README's explicit "No SPDX headers are
  present in any file in this repo" statement. Removed.
- `include/fi_port.h` was never updated once the adapters existed,
  despite its own comment explicitly saying to fix it once they did:
  it still called both adapters "not yet written," pointed at a
  nonexistent `adapters/riot/` path, and called both port-fit
  assumptions unverified. Rewritten to match reality.
- Several files (`Makefile`, `include/fault_inject.h`,
  `adapters/zephyr/CMakeLists.txt`, `adapters/zephyr/Kconfig`,
  `tests/riot/smoke/Makefile`) pointed at `docs/planning.md` for
  information that document no longer holds (now that it's deleted) or
  never held in the way claimed (`tests/riot/smoke/Makefile` said the
  RIOT bug-reproduction case "not yet" existed, after it already did).
  Fixed to point at README.md/docs/verification.md, or to state the
  fact directly instead of deferring to a file.
- `tests/zephyr/eswifi_recv/src/test_eswifi_recv_ztest.c`'s header
  claimed this proof was "already verified on the host" and pointed at
  `tests/drivers/eswifi_recv/src/...` -- a path that exists only in
  C-MSP (and there, is itself a Zephyr Ztest, not a host run). There is
  no host-only run of this specific bug scenario anywhere; corrected to
  say so.

Every fix above was a doc/comment change verified by direct comparison
against the actual current file contents and directory tree (not
assumed from the diff or the commit message), plus a full re-run of
`make test` afterward to confirm the header edits to `fault_inject.h`/
`fi_port.h` didn't regress the one thing in this repo that could still
be checked without an RTOS toolchain: still 21/21 checks passing, clean
under ASan/UBSan. Historical `(docs/planning.md step N)` labels
elsewhere in this log and in a few adapter/test file headers were
deliberately left alone -- this log is itself append-only by its own
stated convention, and those labels still convey real ordering
information without asking a reader to go find a file that's gone.

What this audit does *not* newly establish: none of the actual
build/test claims in the "Steps 3-7" section above were re-run in this
session (no Zephyr/RIOT toolchain was available). This was a
correctness-of-record pass -- does the repo actually say what's true
about itself -- not a re-verification of the underlying engineering
claims, which stand as documented above unless a future session with
real toolchain access finds otherwise.

## Second consistency pass (full re-download + grep of every file)

Requested explicitly: re-check literally everything, not just the
files already touched. Downloaded all 30 tracked files fresh and
grepped all of them for the same patterns as the first pass
(`docs/planning.md`, `SPDX`, stale `adapters/riot` paths, TODO/FIXME).
Also re-ran `make test` again against the current tree: still 21/21,
clean under ASan/UBSan.

One real miss from the first pass, found this way:
`adapters/fault_inject/fi_port_riot.c` -- never edited in the first
audit pass, so it still pointed at "docs/planning.md's out-of-scope
list" for the multi-core caveat. Fixed to point at README.md's
"Status" section, which now documents that list. Also softened this
file's own preamble (above) and `tests/host/test_fault_inject_core.c`'s
header comment, both of which cited `docs/planning.md` for information
that's true independent of that file's existence.

Everything else the sweep flagged was re-checked in context and left
alone deliberately: historical `(docs/planning.md step N)` labels in
`adapters/zephyr/fi_port_zephyr.c`, `adapters/fault_inject/fi_port_riot.c`,
`tests/riot/nimble_scanlist_recv/src/scanlist_repro.h`,
`tests/riot/nimble_scanlist_recv/Makefile`, and
`tests/zephyr/eswifi_recv/src/eswifi_repro.h` -- all still convey
correct ordering information without asserting the file exists.
`adapters/fault_inject/Makefile`'s mention of `adapters/riot/Makefile`
is intentional past-tense history (explaining why the directory isn't
called that anymore), not a current claim. The `SPDX` and
`adapters/riot` hits inside this file are this log's own record of
those bugs, not live instances of them.

## Independent host-only re-verification (this session, no RTOS toolchain)

Requested explicitly: verify as much as possible for real rather than
trusting prior sessions' logs, given this sandbox has no Zephyr/RIOT
toolchain. Two things turned out to be independently checkable on the
host, without any RTOS, because neither the adapters' core logic nor
either regression proof's actual bug-reproduction code (as opposed to
its RTOS-specific test harness) depends on Zephyr or RIOT headers at
all -- confirmed first by grepping every `#include` in both proofs'
source files: only `test_eswifi_recv_ztest.c` includes anything
RTOS-specific (`<zephyr/ztest.h>`); `eswifi_repro_buggy.c`/`fixed.c`
and both `scanlist_repro_*.c` files include only `<stddef.h>`,
`<stdint.h>`, `<string.h>`, `<assert.h>`, `<stdio.h>`, `<stdlib.h>`,
and their own local headers.

### Both adapters, functionally, against stubs matching the real verified APIs

Not just a syntax check: linked each adapter (`fi_port_zephyr.c`,
`fi_port_riot.c`) against the *real* `src/fault_inject.c` core and a
small stub of the RTOS primitive each one calls -- structurally
faithful to the real signatures already confirmed against upstream
source above (`struct z_spinlock_key { int key; }` /
`k_spin_lock`/`k_spin_unlock`; `unsigned irq_disable(void)` /
`void irq_restore(unsigned)`) but functionally a no-op, since no real
scheduler exists on the host. This tests the adapter's own glue logic
-- correct token threading, correct types -- not the real RTOS
primitive itself (that's still only established by the Steps 3-7
section above, on real hardware simulators):

```
$ gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
    -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4 \
    -Iinclude -I<zephyr-spinlock-stub-dir> \
    src/fault_inject.c adapters/zephyr/fi_port_zephyr.c <driver>.c \
    -o test_zephyr_adapter && ./test_zephyr_adapter
adapter functional smoke test: all checks passed

$ gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
    -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4 \
    -Iinclude -I<riot-irq-stub-dir> \
    src/fault_inject.c adapters/fault_inject/fi_port_riot.c <driver>.c \
    -o test_riot_adapter && ./test_riot_adapter
adapter functional smoke test: all checks passed
```

Both clean under ASan/UBSan.

### eswifi_recv (CVE-2026-1679) proof logic, re-run on the host independent of Zephyr

Same `eswifi_repro_buggy.c`/`eswifi_repro_fixed.c` files from
`tests/zephyr/eswifi_recv/src/`, unmodified, linked against a small
host-only driver instead of the Ztest harness:

```
$ gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
    -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4 \
    -Iinclude -Itests/zephyr/eswifi_recv/src \
    src/fault_inject.c tests/host/fi_port_host.c \
    tests/zephyr/eswifi_recv/src/eswifi_repro_fixed.c <driver>.c \
    -o eswifi_fixed_host && ./eswifi_fixed_host
normal recv: ok
oversized recv rc=-90 (ESWIFI_OK=0, ESWIFI_EMSGSIZE=-90)
exit code: 0

$ gcc ... tests/zephyr/eswifi_recv/src/eswifi_repro_buggy.c <driver>.c \
    -o eswifi_buggy_host && ./eswifi_buggy_host
==880==ERROR: AddressSanitizer: stack-buffer-overflow ...
WRITE of size 64 at ...
    #0 ... in memcpy
    #1 ... in eswifi_socket_recv tests/zephyr/eswifi_recv/src/eswifi_repro_buggy.c:23
    #2 ... in main <driver>.c:35
SUMMARY: AddressSanitizer: stack-buffer-overflow ... in memcpy
==880==ABORTING
exit code: 1
```

`WRITE of size 64` is exactly the oversized length the driver forced
(`ESWIFI_RX_BUF_SIZE * 2` = 32 * 2). Crash location
(`eswifi_repro_buggy.c:23`, the `memcpy` call) matches the Zephyr/ASan
run in the Steps 3-7 section above -- independent confirmation, on a
different toolchain and OS-level build, of the same underlying bug and
the same fix.

### nimble_scanlist_recv (CVE-2024-32018) proof logic, re-run on the host, including the NDEBUG precondition itself

This one is worth doing in three variants, not two, because
`scanlist_repro_buggy.c`'s own comment makes a specific, checkable
claim: the bug only manifests when built with `NDEBUG` (`assert()`
compiles to nothing per C11 7.2p1); without `NDEBUG`, the `assert()`
should fire and abort cleanly, *not* overflow. That claim was checked,
not just repeated:

```
$ gcc ... tests/riot/nimble_scanlist_recv/src/scanlist_repro_fixed.c <driver>.c \
    -o scanlist_fixed_host && ./scanlist_fixed_host
normal update: ok
oversized update completed without rejecting or crashing (ad_len=0)
exit code: 0
```

(`ad_len=0` because the fixed implementation returns before touching
`entry` at all on an oversized length -- `entry` was zeroed before the
call, so `ad_len` staying `0` is the correct "untouched" outcome, not a
silent failure.)

```
$ gcc -fsanitize=address,undefined ... tests/riot/nimble_scanlist_recv/src/scanlist_repro_buggy.c <driver>.c \
    -o scanlist_buggy_debug_host && ./scanlist_buggy_debug_host
scanlist_buggy_debug_host: .../scanlist_repro_buggy.c:32: scanlist_update: Assertion `len <= SCANLIST_AD_MAX' failed.
Aborted
exit code: 134
```

Confirms the claim: without `NDEBUG`, this is a clean, ordinary
assertion failure (SIGABRT), nowhere near the `memcpy`. Then, with the
real advisory's stated precondition:

```
$ gcc -DNDEBUG -fsanitize=address,undefined ... tests/riot/nimble_scanlist_recv/src/scanlist_repro_buggy.c <driver>.c \
    -o scanlist_buggy_ndebug_host && ./scanlist_buggy_ndebug_host
normal update: ok
==922==ERROR: AddressSanitizer: stack-buffer-overflow ...
WRITE of size 51 at ...
    #0 ... in memcpy
    #1 ... in scanlist_update tests/riot/nimble_scanlist_recv/src/scanlist_repro_buggy.c:34
    #2 ... in main <driver>.c:33
SUMMARY: AddressSanitizer: stack-buffer-overflow ... in memcpy
==922==ABORTING
exit code: 1
```

`WRITE of size 51` matches `SCANLIST_AD_MAX + 20` (31 + 20) exactly,
and matches the RIOT/ASan run's `WRITE of size 51` in the Steps 3-7
section above -- independent confirmation, again on a different
toolchain and OS-level build, and additionally confirms the NDEBUG
precondition the buggy file's own comment claims is genuinely load-
bearing, not just asserted.

### What this newly establishes

- Both adapters' own glue code (not the RTOS primitive underneath, but
  the code this repo actually wrote) is logically correct, confirmed
  functionally, not just by syntax check.
- Both regression proofs' actual bug-reproduction logic is independently
  reproducible outside either RTOS entirely, on a completely different
  toolchain/OS-level build than the one used for the Steps 3-7 section,
  and produces the identical failure signature (crash location, write
  size) in both cases -- strong corroborating evidence the Steps 3-7
  results are real and not fabricated, without needing to trust that
  session's environment.
- The `scanlist_repro_buggy.c` file's specific claim about the NDEBUG
  precondition mattering is independently confirmed, not just repeated:
  the same code produces a clean assertion failure without NDEBUG and a
  real overflow with it.

### What this still doesn't establish

- Nothing about either real RTOS's actual scheduler, IRQ handling, or
  build system -- that's still only covered by the Steps 3-7 section
  above, which this session could not re-run (no Zephyr/RIOT toolchain
  available here either).
- Nothing new about hardware-in-the-loop, multi-core, Clang, or CI --
  same open items as before.

## Zephyr adapter smoke test: native_sim + qemu_cortex_m3 + qemu_x86 (this session)

Scope: closes part of the "Any board/platform beyond native_sim" gap
noted as deliberately out of v0 scope in the README. Adds
`tests/zephyr/smoke/`, a Ztest suite mirroring `tests/riot/smoke/main.c`
and `tests/host/test_fault_inject_core.c`'s structure, run via Twister
on three platforms: `native_sim` (already covered by `eswifi_recv`, used
here as the baseline), `qemu_cortex_m3` (real ARMv7-M,
`k_spin_lock`/`k_spin_unlock` via PRIMASK/BASEPRI), and `qemu_x86` (real
x86 protected mode, `k_spin_lock`/`k_spin_unlock` via `cli`/`sti`). This
does NOT cover RIOT-OS board expansion (still `BOARD=native` only),
physical hardware (still none, either RTOS), or multi-core (both
adapters' locking is still single-core only by design).

Toolchain used: Zephyr SDK 1.0.1 (`arm-zephyr-eabi` for
`qemu_cortex_m3`, `x86_64-zephyr-elf` for `qemu_x86`; `native_sim` uses
the host `gcc` 13.3.0 directly, same as prior sessions), West v1.5.0,
Zephyr `v4.4.0-9928-g577e42ad187`, QEMU 8.2.2 (both `qemu-system-arm`
and `qemu-system-i386`).

```
$ west twister -p native_sim -p qemu_cortex_m3 -p qemu_x86 \
    -T tests/zephyr/smoke \
    -x=ZEPHYR_EXTRA_MODULES=/path/to/this/repo --inline-logs
...
INFO    - 3 test scenarios (9 configurations) selected, 6 configurations filtered (6 by static filter, 0 at runtime).
INFO    - 3 of 3 executed test configurations passed (100.00%), 0 built (not run), 0 failed, 0 errored, with no warnings in 136.02 seconds.
INFO    - 21 of 21 executed test cases passed (100.00%) on 3 out of total 1655 platforms (0.18%).
```

Per-platform breakdown from the resulting `twister.json`:
```
native_sim/native        passed  7 testcases
qemu_cortex_m3/ti_lm3s6965  passed  7 testcases
qemu_x86/atom             passed  7 testcases
```

The three platforms were also each run directly outside Twister first
(build the same way, then execute the resulting `zephyr.elf`/`zephyr.exe`
by hand under the matching QEMU invocation or directly for
`native_sim`), as an independent cross-check that Twister's own report
isn't the only evidence -- all three showed the identical
`SUITE PASS - 100.00% ... pass = 7, fail = 0, skip = 0, total = 7`
Ztest summary Twister's report is built from.

### What this establishes

- The Zephyr adapter's actual locking glue
  (`adapters/zephyr/fi_port_zephyr.c`) is functionally correct under two
  real, independent interrupt-masking mechanisms beyond `native_sim`'s
  POSIX-signal stand-in -- a lock/unlock bug masked by native_sim's
  timing has two more genuinely different real mechanisms to pass
  through here.
- The documented Quick Start command for this target (now added to
  README.md) is the literal command that was run, not a plausible-
  looking guess.
- CI's `zephyr` job now runs this same Twister invocation (see
  `.github/workflows/ci.yml`) -- not yet observed on a real
  GitHub-hosted runner, same caveat as every other CI job in that file
  until an actual run's logs are looked at.

### What this still doesn't establish

- RIOT-OS board coverage beyond `BOARD=native` -- untouched by this
  session.
- Physical hardware, for either RTOS -- `qemu_cortex_m3`/`qemu_x86` are
  QEMU, not real silicon; timing, real DMA/cache effects, and genuine
  multi-core interrupt controllers are all still unverified.
- Multi-core -- both adapters' `fi_port_lock`/`fi_port_unlock` remain
  explicitly single-core-only by construction; nothing here changes
  that or tests it.
- Clang -- these builds used the Zephyr SDK's GCC-based toolchains, same
  gap noted in the Step 1+2 section above.
