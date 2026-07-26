/* fault_inject.h
 *
 * Deterministic fault-injection primitive -- portable core.
 *
 * Generalized from C-MSP's Zephyr-only fault_inject.h
 * (https://github.com/trinityman-hash/C-MSP/blob/main/include/fault_inject.h).
 * Same primitive, same public API, same FI_POINT contract. The only
 * thing that changed getting here is where the registry's critical
 * section comes from -- see fi_port.h. This file is deliberately close
 * to a straight copy of C-MSP's version; don't "improve" it without a
 * reason tied to portability, since C-MSP's version is the proven
 * reference (docs/planning.md is the tie-breaker on any conflict).
 *
 * A "fault point" is a macro dropped at a call site that could plausibly
 * fail (or, as in C-MSP's repro, at a call site that returns an
 * externally-controlled value a test wants to force to an otherwise
 * hard-to-reach case). When armed, the injected value is substituted
 * *instead of* making the real call -- the real call is not evaluated,
 * same as forcing a real driver call to fail without actually invoking
 * the driver. When fault injection is not compiled in, FI_POINT expands
 * to exactly the original call: no check, no branch, nothing. The macro
 * does not exist in that build.
 *
 * This is deliberately simpler than Linux's probability/interval/budget
 * model: deterministic arm/fire is easier to write a reliable, non-flaky
 * test against, and that's what v0 needs.
 */

#ifndef FAULT_INJECT_H
#define FAULT_INJECT_H

#include <stdint.h>

#ifdef CONFIG_FAULT_INJECTION_MAX_POINTS
#define FI_MAX_POINTS CONFIG_FAULT_INJECTION_MAX_POINTS
#else
#define FI_MAX_POINTS 32 /* default; override via -DCONFIG_FAULT_INJECTION_MAX_POINTS */
#endif

#ifdef CONFIG_FAULT_INJECTION

/* Increments the point's hit count every time it is evaluated (armed or
 * not), so a test can prove the point was actually reached. Returns 0 if
 * the point should pass through to the real call, or the armed nonzero
 * injected value otherwise. */
int fi_should_fail(uint32_t fault_id);

void fi_arm(uint32_t fault_id, int inject_value);
void fi_disarm(uint32_t fault_id);
void fi_reset_all(void);
uint32_t fi_hit_count(uint32_t fault_id);

/* Thread safety: fi_should_fail()/fi_arm()/fi_disarm()/fi_reset_all()/
 * fi_hit_count() are safe to call concurrently -- including from an ISR
 * -- on any adapter that correctly implements fi_port.h, since the
 * registry (src/fault_inject.c) takes the port lock on every access,
 * unconditionally. This is a change from C-MSP, where locking was only
 * compiled in under __ZEPHYR__ and the host-only build had no locking
 * at all. Here, "no RTOS present" is just another port: see
 * tests/host/fi_port_host.c, which supplies a real (no-op,
 * single-threaded-only) implementation instead of the core
 * special-casing that case away. */

/* GCC/Clang statement-expression: evaluates fi_should_fail() exactly
 * once, and evaluates real_call only when the point is not armed, so an
 * armed fault genuinely replaces the call rather than merely overriding
 * its result after the fact. Both v0 target adapters (Zephyr, RIOT-OS)
 * build with GCC or Clang, so this is not a portability gap for v0; it
 * would need revisiting before supporting an MSVC-only target. */
#define FI_POINT(id, real_call)                    \
    ({ int _fi_rc = fi_should_fail(id);             \
       (_fi_rc != 0) ? _fi_rc : (real_call); })

#else

#define FI_POINT(id, real_call) (real_call)

#endif /* CONFIG_FAULT_INJECTION */

#endif /* FAULT_INJECT_H */
