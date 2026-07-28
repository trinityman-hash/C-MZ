/* test_fault_inject_core.c
 *
 * Unit tests for the portable core (src/fault_inject.c) itself, run
 * against the host stub port (fi_port_host.c) -- see the warning in
 * that file about why it's not a real adapter. This is NOT a proof of
 * portability -- that requires the Zephyr and RIOT-OS adapters, both
 * of which now exist and are verified for real (docs/verification.md)
 * -- it's the fastest way to verify the core's registry logic and the
 * FI_POINT contract are actually correct before porting anything onto
 * it. Ported adapters must still get their own real-target
 * verification (the CVE-2026-1679 and CVE-2024-32018 regression checks
 * documented there).
 *
 * No test framework dependency, matching C-MSP's host-only style
 * (tests/drivers/eswifi_recv/src/test_eswifi_recv.c).
 *
 * FI_MAX_POINTS is set to 4 for this binary specifically (see
 * Makefile), not the library default of 32, so the table-full test
 * below runs fast and its behavior is easy to hand-verify -- the test
 * itself reads FI_MAX_POINTS from the header rather than hardcoding 4,
 * so it stays correct if that build flag ever changes.
 */

#include "fault_inject.h"
#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);  \
            g_failures++;                                                    \
        } else {                                                             \
            printf("  ok: %s\n", msg);                                       \
        }                                                                     \
    } while (0)

enum {
    FI_TEST_POINT_A = 1,
    FI_TEST_POINT_B = 2,
};

static int g_real_call_evaluations;

static int real_call_ok(void)
{
    g_real_call_evaluations++;
    return 0; /* 0 == "no error", matching FI_POINT's pass-through contract */
}

static void test_disarmed_passes_through(void)
{
    printf("test_disarmed_passes_through:\n");
    fi_reset_all();
    g_real_call_evaluations = 0;

    int rc = FI_POINT(FI_TEST_POINT_A, real_call_ok());

    CHECK(rc == 0, "returns the real call's result when disarmed");
    CHECK(g_real_call_evaluations == 1, "real call was evaluated exactly once");
}

static void test_armed_injects_and_skips_real_call(void)
{
    printf("test_armed_injects_and_skips_real_call:\n");
    fi_reset_all();
    g_real_call_evaluations = 0;
    fi_arm(FI_TEST_POINT_A, -5);

    int rc = FI_POINT(FI_TEST_POINT_A, real_call_ok());

    CHECK(rc == -5, "returns the armed injected value");
    CHECK(g_real_call_evaluations == 0,
          "real call was NOT evaluated -- the fault genuinely replaced it");
}

static void test_disarm_restores_passthrough(void)
{
    printf("test_disarm_restores_passthrough:\n");
    fi_reset_all();
    fi_arm(FI_TEST_POINT_A, -5);
    fi_disarm(FI_TEST_POINT_A);
    g_real_call_evaluations = 0;

    int rc = FI_POINT(FI_TEST_POINT_A, real_call_ok());

    CHECK(rc == 0, "passes through again after disarm");
    CHECK(g_real_call_evaluations == 1, "real call evaluated again after disarm");
}

static void test_hit_count_increments_armed_or_not(void)
{
    printf("test_hit_count_increments_armed_or_not:\n");
    fi_reset_all();

    (void)fi_should_fail(FI_TEST_POINT_A);
    CHECK(fi_hit_count(FI_TEST_POINT_A) == 1, "hit count counts a disarmed evaluation");

    fi_arm(FI_TEST_POINT_A, 7);
    (void)fi_should_fail(FI_TEST_POINT_A);
    CHECK(fi_hit_count(FI_TEST_POINT_A) == 2, "hit count also counts an armed evaluation");
}

static void test_points_are_independent(void)
{
    printf("test_points_are_independent:\n");
    fi_reset_all();
    fi_arm(FI_TEST_POINT_A, -1);

    CHECK(fi_should_fail(FI_TEST_POINT_A) == -1, "point A is armed");
    CHECK(fi_should_fail(FI_TEST_POINT_B) == 0, "point B is unaffected by arming A");
    CHECK(fi_hit_count(FI_TEST_POINT_A) == 1, "point A hit count only reflects point A");
    CHECK(fi_hit_count(FI_TEST_POINT_B) == 1, "point B hit count only reflects point B");
}

static void test_reset_all_clears_everything(void)
{
    printf("test_reset_all_clears_everything:\n");
    fi_reset_all();
    fi_arm(FI_TEST_POINT_A, -1);
    (void)fi_should_fail(FI_TEST_POINT_A);
    fi_arm(FI_TEST_POINT_B, -2);
    (void)fi_should_fail(FI_TEST_POINT_B);

    fi_reset_all();

    CHECK(fi_hit_count(FI_TEST_POINT_A) == 0, "point A hit count cleared");
    CHECK(fi_hit_count(FI_TEST_POINT_B) == 0, "point B hit count cleared");
    CHECK(fi_should_fail(FI_TEST_POINT_A) == 0, "point A no longer armed after reset");
}

/* Fills the table to FI_MAX_POINTS distinct ids, then checks that one
 * more *new* id is safely rejected (treated as "not armed", per the
 * documented contract in fault_inject.c) rather than corrupting an
 * existing entry or crashing. Ids used here deliberately don't collide
 * with FI_TEST_POINT_A/B (1, 2) from the other tests, since fi_reset_all()
 * only clears in_use flags, not the id space semantics being tested. */
static void test_table_full_is_handled_safely(void)
{
    printf("test_table_full_is_handled_safely:\n");
    fi_reset_all();

    for (uint32_t i = 0; i < FI_MAX_POINTS; i++) {
        uint32_t id = 100 + i;
        (void)fi_should_fail(id); /* creates the entry */
    }
    CHECK(fi_hit_count(100) == 1, "an entry created while the table had room works normally");

    uint32_t overflow_id = 100 + FI_MAX_POINTS;
    int rc = fi_should_fail(overflow_id);
    CHECK(rc == 0, "a new id past table capacity is treated as not-armed, not a crash");
    CHECK(fi_hit_count(overflow_id) == 0,
          "an id that couldn't be allocated has no hit count (it was never stored)");

    /* Existing entries must be untouched by the failed allocation attempt. */
    CHECK(fi_hit_count(100) == 1, "existing entries survive a failed allocation attempt");
}

int main(void)
{
#ifndef CONFIG_FAULT_INJECTION
#error "This test requires CONFIG_FAULT_INJECTION"
#endif
    test_disarmed_passes_through();
    test_armed_injects_and_skips_real_call();
    test_disarm_restores_passthrough();
    test_hit_count_increments_armed_or_not();
    test_points_are_independent();
    test_reset_all_clears_everything();
    test_table_full_is_handled_safely();

    if (g_failures > 0) {
        fprintf(stderr, "\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    printf("\nall checks passed\n");
    return 0;
}
