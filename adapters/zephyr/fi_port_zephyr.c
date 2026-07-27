/* fi_port_zephyr.c
 *
 * Zephyr implementation of the fi_port.h porting interface, using
 * Zephyr's own IRQ-safe spinlock (k_spin_lock/k_spin_unlock) -- the same
 * primitive C-MSP's Zephyr-only fault_inject.c used directly, now behind
 * the portable core's port boundary instead of an `#ifdef __ZEPHYR__`
 * inside src/fault_inject.c.
 *
 * fi_port_key_t <-> k_spinlock_key_t verification (docs/planning.md step 3):
 * fi_port.h flagged k_spinlock_key_t fitting in one `unsigned long` as an
 * unverified assumption ("based on general familiarity ... not checked
 * against a real Zephyr checkout"). Checked for real against
 * zephyrproject-rtos/zephyr, main branch,
 * include/zephyr/spinlock.h, fetched 2026-07-26:
 *
 *   struct z_spinlock_key { int key; };
 *   typedef struct z_spinlock_key k_spinlock_key_t;
 *
 * A single `int` field. C guarantees `long` (and so `unsigned long`) is
 * at least as wide as `int` on every conforming implementation, so this
 * fits with no truncation on every Zephyr-supported target, 32-bit or
 * 64-bit. The assumption in fi_port.h was correct; this comment replaces
 * "expected" with "confirmed against source" per that file's own
 * instruction to fix the comment, not defend the assumption, once an
 * adapter actually exists to check it against.
 *
 * A single static `struct k_spinlock` here is deliberate and matches
 * C-MSP: one registry, one lock, all of `src/fault_inject.c`'s five
 * public entry points serialize through it. No RTOS header is included
 * by src/fault_inject.c itself -- this file is the only place
 * <zephyr/spinlock.h> is ever pulled in.
 */

#include "fi_port.h"
#include <zephyr/spinlock.h>

static struct k_spinlock fi_lock;

fi_port_key_t fi_port_lock(void)
{
    k_spinlock_key_t zkey = k_spin_lock(&fi_lock);
    fi_port_key_t key;

    key.state = (unsigned long)zkey.key;
    return key;
}

void fi_port_unlock(fi_port_key_t key)
{
    k_spinlock_key_t zkey;

    zkey.key = (int)key.state;
    k_spin_unlock(&fi_lock, zkey);
}
