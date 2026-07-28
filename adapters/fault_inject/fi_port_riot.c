/* fi_port_riot.c
 *
 * RIOT-OS implementation of the fi_port.h porting interface, using
 * RIOT's own IRQ disable/restore primitive (irq_disable/irq_restore) --
 * the same shape fi_port.h's rationale is built on: enter a critical
 * section, get back an opaque token representing prior state, restore
 * it on exit.
 *
 * fi_port_key_t <-> RIOT's irq state verification: fi_port.h originally
 * flagged this as "asserted with reasonable confidence... not yet
 * verified by an actual RIOT-OS build." Checked for real against
 * RIOT-OS/RIOT, master branch, core/lib/include/irq.h, fetched
 * 2026-07-26:
 *
 *   unsigned irq_disable(void);
 *   void     irq_restore(unsigned state);
 *
 * Both plain `unsigned`, which fits with no truncation inside
 * fi_port_key_t's `unsigned long` (C guarantees unsigned long is at
 * least as wide as unsigned int) on every RIOT-supported target. The
 * assumption in fi_port.h was correct; this comment replaces "asserted
 * with reasonable confidence" with "confirmed against source."
 *
 * Unlike the Zephyr side (one static struct k_spinlock instance),
 * RIOT's irq_disable()/irq_restore() are global, not tied to a lock
 * object -- there is nothing here to instantiate. This is still a
 * correct IRQ-safe critical section for a single-core target: the
 * registry in src/fault_inject.c never nests fi_port_lock() calls (each
 * public function takes it once, unlocks it once), so a single global
 * IRQ-disable per call is sufficient and matches how RIOT code
 * elsewhere protects short critical sections. Multi-core RIOT targets
 * are out of scope for v0 -- see README.md's "Status" section -- and
 * would need revisiting this if that ever changes.
 */

#include "fi_port.h"
#include "irq.h"

fi_port_key_t fi_port_lock(void)
{
    fi_port_key_t key;

    key.state = (unsigned long)irq_disable();
    return key;
}

void fi_port_unlock(fi_port_key_t key)
{
    irq_restore((unsigned)key.state);
}
