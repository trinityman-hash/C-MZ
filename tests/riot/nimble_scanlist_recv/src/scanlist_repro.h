/* scanlist_repro.h
 *
 * docs/planning.md step 6: a real RIOT-OS bug reproduction, found
 * through actual research (web_search + reading the real source in a
 * real RIOT-OS checkout), not invented -- same discipline as C-MSP's
 * CVE-2026-1679 choice and this repo's own Zephyr eswifi_recv proof.
 *
 * Real bug: CVE-2024-32018 / GHSA-899m-q6pp-hmp3, in
 * https://github.com/RIOT-OS/RIOT/blob/master/pkg/nimble/scanlist/nimble_scanlist.c
 * (nimble_scanlist_update(), around lines 74-87 at the time the
 * advisory was filed). Confirmed against RIOT-OS/RIOT master branch,
 * fetched 2026-07-26 -- the file's current (fixed) content is quoted
 * for the "how it should behave" side below, and the advisory's own
 * description ("len is checked in an assertion and subsequently used
 * in a call to memcpy() ... write past the end of the fixed-length
 * e->ad buffer") is what the "buggy" variant reconstructs.
 *
 * The real bug's shape: `nimble_scanlist_entry_t.ad` is a fixed
 * `uint8_t ad[BLE_ADV_PDU_LEN]` buffer (BLE_ADV_PDU_LEN is 31, per
 * sys/include/net/ble.h, confirmed against the same checkout). The
 * pre-fix code's only defense against an oversized `len` was
 * `assert(len <= BLE_ADV_PDU_LEN)` before `memcpy(e->ad, ad, len)` --
 * and `assert()` compiles to nothing at all in an NDEBUG (release)
 * build, per the C standard, so in a release build there was no check
 * whatsoever. The real fix (current master, quoted here) replaced that
 * with an actual `if` that runs unconditionally:
 *
 *   if (len > BLE_ADV_PDU_LEN) {
 *       assert(0);
 *       return;
 *   }
 *
 * `len` itself is not attacker-supplied over an open socket -- it
 * comes from the NimBLE host stack parsing an over-the-air BLE
 * advertising PDU from a controller/radio. As with the eswifi case
 * (tests/zephyr/eswifi_recv), you cannot easily make real BLE hardware
 * report a bogus oversized length on demand in a test; that external,
 * hard-to-reach condition is exactly what a fault point exists to
 * manufacture instead. `ble_adv_get_len()` below stands in for
 * whatever real call in the NimBLE stack ultimately hands
 * nimble_scanlist_update() its `len` argument.
 *
 * This is a small, simplified reconstruction of the bug pattern, not a
 * copy of the real driver: no NimBLE host stack, no clist pool
 * allocator, no ztimer -- those are orthogonal to the one thing being
 * proven (an assert-only length check is silently absent in a release
 * build, and a real fault point can force the exact oversized-length
 * condition needed to demonstrate that deterministically, in both a
 * debug and a release build, without real BLE hardware). Two
 * implementations of the same function signature are provided in
 * separate translation units (scanlist_repro_buggy.c /
 * scanlist_repro_fixed.c), same convention as eswifi_repro.h.
 */

#ifndef SCANLIST_REPRO_H
#define SCANLIST_REPRO_H

#include <stddef.h>
#include <stdint.h>

/* Real value, sys/include/net/ble.h, RIOT-OS/RIOT master, confirmed
 * 2026-07-26: #define BLE_ADV_PDU_LEN (31U) */
#define SCANLIST_AD_MAX 31

/* Stand-in for the fixed-size scanlist entry (nimble_scanlist_entry_t's
 * ad/ad_len fields only -- the rest of that real struct, the BLE
 * address, RSSI, timestamps, clist node, is not relevant to this bug). */
struct scan_entry {
    uint8_t ad[SCANLIST_AD_MAX];
    uint8_t ad_len;
};

/* Stand-in for whatever real NimBLE stack call ultimately reports an
 * advertising PDU's length. reported_len is what it would return when
 * not fault-injected. */
struct ble_adv_report_mock {
    int reported_len;
};

/* Fault point id for the length-report call. */
#define FI_SCANLIST_ADV_LEN 1u

/* Returns report->reported_len. Represents the real NimBLE call that
 * FI_POINT wraps in the implementations below. */
int ble_adv_get_len(const struct ble_adv_report_mock *report);

/* Reads the reported length (via the FI_POINT-wrapped call) and copies
 * that many bytes from ad_data into entry->ad.
 *
 * Buggy implementation: the real pre-fix defense -- assert() only,
 * built with NDEBUG defined (the real precondition the advisory
 * describes: "while assertions are compiled-out") -- so the check is
 * physically absent from the binary, and the memcpy runs unguarded.
 * Fixed implementation: the real current upstream fix -- an
 * unconditional `if`, not an assert -- rejects an oversized length
 * before touching entry->ad at all.
 */
void scanlist_update(struct scan_entry *entry,
                      const struct ble_adv_report_mock *report,
                      const uint8_t *ad_data);

#endif /* SCANLIST_REPRO_H */
