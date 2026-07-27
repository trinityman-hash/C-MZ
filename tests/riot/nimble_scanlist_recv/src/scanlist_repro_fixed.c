/* scanlist_repro_fixed.c
 *
 * Mirrors the real current upstream fix in
 * pkg/nimble/scanlist/nimble_scanlist.c's nimble_scanlist_update():
 * an unconditional `if`, not an assert, rejects an oversized length
 * before entry->ad is touched at all. See scanlist_repro.h for the
 * full CVE-2024-32018 background and what is/isn't a literal copy.
 */

#include "scanlist_repro.h"
#include "fault_inject.h"
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

    /* Real upstream fix: runs unconditionally, in every build. */
    if (len > SCANLIST_AD_MAX) {
        return;
    }

    memcpy(entry->ad, ad_data, (size_t)len);
    entry->ad_len = (uint8_t)len;
}
