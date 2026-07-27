/* main.c
 *
 * The actual test body for the RIOT-OS adapter smoke test (see
 * ../Makefile's header comment for what this proves and doesn't).
 * Deliberately mirrors tests/host/test_fault_inject_core.c's structure
 * and CHECK macro -- same core, same public API, same assertions where
 * they apply -- so a reviewer can compare the two directly and see that
 * what changed is only the port underneath (fi_port_host.c's no-op vs
 * fi_port_riot.c's real irq_disable/irq_restore), not the test intent.
 *
 * Not a byte-for-byte copy of that file: FI_MAX_POINTS here is 4 (see
 * ../Makefile's CFLAGS), so the table-full test's ids match that
 * capacity.
 *
 * main() calls exit() explicitly rather than returning. An earlier
 * version of this file assumed `return N;` from main() -- like
 * tests/minimal, RIOT's own size-test app -- would terminate the
 * native-board process with that exit code, the way it would for an
 * ordinary hosted C program. It doesn't: confirmed by actually running
 * the built binary and watching it sit there indefinitely after
 * printing "all checks passed". RIOT's native board keeps the process
 * alive after main's thread finishes -- the scheduler and its other
 * threads (idle, etc.) are still running -- so nothing was wrong with
 * tests/minimal returning 0 (nobody was reading its exit code either;
 * it's a size test), but this app's whole purpose is to report a real
 * pass/fail exit code, so it needs exit() -- confirmed to actually
 * terminate the native process, matching real precedent elsewhere in
 * RIOT's own tree (tests/pkg/wolfcrypt-ed25519-verify's tools).
 */

#include "fault_inject.h"
#include <stdio.h>
#include <stdlib.h>

static int g_failures = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);           \
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
          "real call was NOT evaluated -- the fault genuinely replaced it, "
          "through the real irq_disable/irq_restore critical section");
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

/* FI_MAX_POINTS is 4 here (../Makefile's CFLAGS), matching the host
 * suite's approach of reading the real build-time capacity rather than
 * hardcoding it, so this stays correct if that flag ever changes. */
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

    CHECK(fi_hit_count(100) == 1, "existing entries survive a failed allocation attempt");
}

int main(void)
{
#ifndef CONFIG_FAULT_INJECTION
#error "This test requires CONFIG_FAULT_INJECTION"
#endif
    printf("=== fault_inject RIOT-OS adapter smoke test ===\n");

    test_disarmed_passes_through();
    test_armed_injects_and_skips_real_call();
    test_disarm_restores_passthrough();
    test_hit_count_increments_armed_or_not();
    test_points_are_independent();
    test_reset_all_clears_everything();
    test_table_full_is_handled_safely();

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        exit(1);
    }
    printf("\nall checks passed\n");
    exit(0);
}
