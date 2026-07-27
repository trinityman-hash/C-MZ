/* scanlist_repro_buggy.c
 *
 * Reconstructs the real pre-fix defense described in GHSA-899m-q6pp-hmp3:
 * "len is checked in an assertion and subsequently used in a call to
 * memcpy()". assert() compiles to nothing at all when NDEBUG is
 * defined (C11 7.2p1), so this file only actually demonstrates the
 * real bug when built with NDEBUG -- see ../Makefile: the buggy build
 * variant defines NDEBUG deliberately, matching the advisory's own
 * stated precondition ("while assertions are compiled-out"), not as
 * an arbitrary test flag. See scanlist_repro.h for the full
 * CVE-2024-32018 background.
 */

#include "scanlist_repro.h"
#include "fault_inject.h"
#include <assert.h>
#include <string.h>

int ble_adv_get_len(const struct ble_adv_report_mock *report)
{
    return report->reported_len;
}

void scanlist_update(struct scan_entry *entry,
                      const struct ble_adv_report_mock *report,
                      const uint8_t *ad_data)
{
    int len = FI_POINT(FI_SCANLIST_ADV_LEN, ble_adv_get_len(report));

    /* Real pre-fix defense: a no-op in this build, since ../Makefile
     * defines NDEBUG for the buggy variant. */
    assert(len <= SCANLIST_AD_MAX);

    memcpy(entry->ad, ad_data, (size_t)len);
    entry->ad_len = (uint8_t)len;
}
