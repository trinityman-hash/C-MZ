/* main.c
 *
 * Test harness for the CVE-2024-32018 reproduction (scanlist_repro.h).
 * Same two-case structure as tests/zephyr/eswifi_recv/src's ztest file:
 * a normal, in-bounds call that must behave correctly, then a
 * fault-injected oversized-length call. Built against the fixed
 * variant, both cases pass cleanly. Built against the buggy variant
 * (with NDEBUG, so its assert() is a no-op -- see ../Makefile), the
 * second case is expected to crash under ASan: that crash, not a
 * green checkmark, is the actual proof for that half of the build.
 *
 * Same exit()-not-return convention as ../../smoke/main.c, and same
 * reasoning: a RIOT native-board process does not terminate just
 * because main() returns.
 */

#include "scanlist_repro.h"
#include "fault_inject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_normal_recv_no_fault(void)
{
    printf("test_normal_recv_no_fault:\n");
    fi_reset_all();

    struct scan_entry entry;
    memset(&entry, 0xAA, sizeof(entry));
    struct ble_adv_report_mock report = { .reported_len = 10 };
    uint8_t ad_data[10];
    for (unsigned i = 0; i < sizeof(ad_data); i++) {
        ad_data[i] = (uint8_t)(i + 1);
    }

    scanlist_update(&entry, &report, ad_data);

    CHECK(entry.ad_len == 10, "entry updated with the real (in-bounds) length");
    CHECK(memcmp(entry.ad, ad_data, 10) == 0, "advertising data copied correctly");
}

/* Forces the external condition real BLE hardware would only rarely
 * (and non-deterministically) produce: an advertisement report whose
 * length exceeds SCANLIST_AD_MAX. On the fixed variant this must be
 * safely rejected. On the buggy variant (NDEBUG, assert() is a no-op)
 * this is expected to overflow entry.ad and crash under ASan -- see
 * ../Makefile and this file's own header comment.
 */
static void test_oversized_length_injected(void)
{
    printf("test_oversized_length_injected:\n");
    fi_reset_all();
    fi_arm(FI_SCANLIST_ADV_LEN, SCANLIST_AD_MAX + 20);

    struct scan_entry entry;
    memset(&entry, 0, sizeof(entry));
    struct ble_adv_report_mock report = { .reported_len = 5 }; /* overridden by fi_arm above */
    uint8_t ad_data[SCANLIST_AD_MAX + 20];
    for (unsigned i = 0; i < sizeof(ad_data); i++) {
        ad_data[i] = (uint8_t)(i + 1);
    }

    scanlist_update(&entry, &report, ad_data);

    /* Only reached at all on the fixed variant -- the buggy variant is
     * expected to have already crashed inside scanlist_update() above. */
    CHECK(entry.ad_len == 0, "oversized length was rejected, entry left untouched");
}

int main(void)
{
#ifndef CONFIG_FAULT_INJECTION
#error "This test requires CONFIG_FAULT_INJECTION"
#endif
    printf("=== nimble_scanlist CVE-2024-32018 reproduction ===\n");

    test_normal_recv_no_fault();
    test_oversized_length_injected();

    if (g_failures > 0) {
        printf("\n%d check(s) FAILED\n", g_failures);
        exit(1);
    }
    printf("\nall checks passed\n");
    exit(0);
}
