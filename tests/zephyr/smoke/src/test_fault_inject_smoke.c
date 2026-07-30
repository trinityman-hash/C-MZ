/* test_fault_inject_smoke.c
 *
 * Zephyr adapter smoke test: proves adapters/zephyr/fi_port_zephyr.c
 * actually compiles and links against a real Zephyr checkout, and that
 * the portable core behaves correctly through it -- not the same thing
 * as the host-only build with the no-op stub port (tests/host/), and
 * not the CVE-2026-1679 regression proof (that's ../eswifi_recv/, which
 * is deliberately native_sim + ASan only -- see that directory's own
 * testcase.yaml comment for why).
 *
 * This file exists to close a gap: tests/riot/smoke/main.c has a
 * dedicated adapter smoke test for RIOT-OS; there was no equivalent
 * for Zephyr before this file -- eswifi_recv covers a specific CVE
 * pattern under one platform (native_sim) with ASan, it does not by
 * itself prove the adapter's locking is correct on real target
 * hardware. Deliberately mirrors tests/riot/smoke/main.c and
 * tests/host/test_fault_inject_core.c's structure and assertions --
 * same core, same public API, same intent -- so a reviewer can compare
 * all three directly and see that what changes between them is only
 * the port underneath (host stub vs. real irq_disable/irq_restore vs.
 * real k_spin_lock/k_spin_unlock), not the test's intent.
 *
 * platform_allow in testcase.yaml intentionally lists three platforms
 * spanning two real, independent instruction sets, not just
 * native_sim:
 *   - native_sim   -- POSIX arch; runs as a host process (build-host
 *                     ISA), same class of coverage the eswifi_recv
 *                     test already has.
 *   - qemu_cortex_m3 -- real ARMv7-M code under QEMU: k_spin_lock on
 *                     this arch masks interrupts via PRIMASK/BASEPRI,
 *                     a real Cortex-M exception-priority mechanism,
 *                     nothing like native_sim's POSIX signal-based
 *                     stand-in.
 *   - qemu_x86     -- real x86 protected-mode code under QEMU:
 *                     k_spin_lock here masks interrupts via cli/sti
 *                     (the IF flag), a different mechanism again.
 * A lock/unlock bug that happens to be masked by native_sim's POSIX
 * arch (or by the build host's own scheduler timing) has two
 * genuinely different real interrupt-masking mechanisms to also pass
 * through here before this suite reports green. No ASan/UBSan on the
 * two QEMU platforms -- those sanitizers need a hosted libc, which
 * bare cross-compiled QEMU targets don't have; this suite's job is
 * functional/locking correctness on real targets, not memory safety
 * (that remains eswifi_recv's job, on native_sim, where ASan actually
 * works).
 */

#include <zephyr/ztest.h>
#include "fault_inject.h"

ZTEST_SUITE(fault_inject_smoke, NULL, NULL, NULL, NULL, NULL);

enum {
	FI_TEST_POINT_A = 1,
	FI_TEST_POINT_B = 2,
};

static int real_call_evaluations;

static int real_call_ok(void)
{
	real_call_evaluations++;
	return 0; /* 0 == "no error", matching FI_POINT's pass-through contract */
}

ZTEST(fault_inject_smoke, test_disarmed_passes_through)
{
	fi_reset_all();
	real_call_evaluations = 0;

	int rc = FI_POINT(FI_TEST_POINT_A, real_call_ok());

	zassert_equal(rc, 0, "returns the real call's result when disarmed");
	zassert_equal(real_call_evaluations, 1, "real call was evaluated exactly once");
}

ZTEST(fault_inject_smoke, test_armed_injects_and_skips_real_call)
{
	fi_reset_all();
	real_call_evaluations = 0;
	fi_arm(FI_TEST_POINT_A, -5);

	int rc = FI_POINT(FI_TEST_POINT_A, real_call_ok());

	zassert_equal(rc, -5, "returns the armed injected value");
	zassert_equal(real_call_evaluations, 0,
		      "real call was NOT evaluated -- the fault genuinely replaced it, "
		      "through the real k_spin_lock/k_spin_unlock critical section");
}

ZTEST(fault_inject_smoke, test_disarm_restores_passthrough)
{
	fi_reset_all();
	fi_arm(FI_TEST_POINT_A, -5);
	fi_disarm(FI_TEST_POINT_A);
	real_call_evaluations = 0;

	int rc = FI_POINT(FI_TEST_POINT_A, real_call_ok());

	zassert_equal(rc, 0, "passes through again after disarm");
	zassert_equal(real_call_evaluations, 1, "real call evaluated again after disarm");
}

ZTEST(fault_inject_smoke, test_hit_count_increments_armed_or_not)
{
	fi_reset_all();

	(void)fi_should_fail(FI_TEST_POINT_A);
	zassert_equal(fi_hit_count(FI_TEST_POINT_A), 1, "hit count counts a disarmed evaluation");

	fi_arm(FI_TEST_POINT_A, 7);
	(void)fi_should_fail(FI_TEST_POINT_A);
	zassert_equal(fi_hit_count(FI_TEST_POINT_A), 2, "hit count also counts an armed evaluation");
}

ZTEST(fault_inject_smoke, test_points_are_independent)
{
	fi_reset_all();
	fi_arm(FI_TEST_POINT_A, -1);

	zassert_equal(fi_should_fail(FI_TEST_POINT_A), -1, "point A is armed");
	zassert_equal(fi_should_fail(FI_TEST_POINT_B), 0, "point B is unaffected by arming A");
	zassert_equal(fi_hit_count(FI_TEST_POINT_A), 1, "point A hit count only reflects point A");
	zassert_equal(fi_hit_count(FI_TEST_POINT_B), 1, "point B hit count only reflects point B");
}

ZTEST(fault_inject_smoke, test_reset_all_clears_everything)
{
	fi_reset_all();
	fi_arm(FI_TEST_POINT_A, -1);
	(void)fi_should_fail(FI_TEST_POINT_A);
	fi_arm(FI_TEST_POINT_B, -2);
	(void)fi_should_fail(FI_TEST_POINT_B);

	fi_reset_all();

	zassert_equal(fi_hit_count(FI_TEST_POINT_A), 0, "point A hit count cleared");
	zassert_equal(fi_hit_count(FI_TEST_POINT_B), 0, "point B hit count cleared");
	zassert_equal(fi_should_fail(FI_TEST_POINT_A), 0, "point A no longer armed after reset");
}

/* FI_MAX_POINTS is read from the header (driven by
 * CONFIG_FAULT_INJECTION_MAX_POINTS=8, set in testcase.yaml), not
 * hardcoded, matching tests/host/test_fault_inject_core.c and
 * tests/riot/smoke/main.c's approach. */
ZTEST(fault_inject_smoke, test_table_full_is_handled_safely)
{
	fi_reset_all();

	for (uint32_t i = 0; i < FI_MAX_POINTS; i++) {
		uint32_t id = 100 + i;
		(void)fi_should_fail(id); /* creates the entry */
	}
	zassert_equal(fi_hit_count(100), 1,
		      "an entry created while the table had room works normally");

	uint32_t overflow_id = 100 + FI_MAX_POINTS;
	int rc = fi_should_fail(overflow_id);

	zassert_equal(rc, 0, "a new id past table capacity is treated as not-armed, not a crash");
	zassert_equal(fi_hit_count(overflow_id), 0,
		      "an id that couldn't be allocated has no hit count (it was never stored)");
	zassert_equal(fi_hit_count(100), 1, "existing entries survive a failed allocation attempt");
}
