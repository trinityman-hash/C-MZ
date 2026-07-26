/* fi_port.h
 *
 * Porting interface the portable fault-injection core (src/fault_inject.c)
 * depends on. Every adapter -- one per RTOS, plus the host-only test
 * harness -- must supply exactly these two functions so the core never
 * touches an RTOS API directly. v0 has three implementations of this
 * interface in total: adapters/zephyr/fi_port_zephyr.c,
 * adapters/riot/fi_port_riot.c (both not yet written), and
 * tests/host/fi_port_host.c (a no-op stub, host tests only -- not a
 * real adapter, see the warning in that file).
 *
 * Rationale for the shape: a fault point can legitimately be armed,
 * disarmed, or hit from different threads -- and, on the hit side, from
 * an ISR -- so the registry needs a genuine IRQ-safe critical section,
 * not just a mutex. Zephyr's k_spin_lock/k_spin_unlock and RIOT-OS's
 * irq_disable/irq_restore both already follow the same shape: enter a
 * critical section, get back an opaque token representing prior state,
 * restore it on exit. That's the standard pattern for a single-core
 * IRQ-safe critical section, and it's the basis for this interface.
 *
 * This is a design decision carried from docs/planning.md and NOT yet
 * verified against a real RIOT-OS build (no RIOT toolchain has been set
 * up anywhere in this project yet -- see docs/planning.md step 5/7). If
 * it turns out to be wrong once adapters/riot/ actually exists and
 * builds, fix this file and this comment, don't defend the assumption.
 *
 * fi_port_key_t deliberately does NOT reuse any RTOS-native key type
 * directly (e.g. Zephyr's k_spinlock_key_t) -- doing so would pull an
 * RTOS header into a file that must build with zero RTOS dependency.
 * Instead it's one word wide, and each adapter is responsible for
 * making its native key fit inside it:
 *   - RIOT-OS: irq_disable() already returns a plain `unsigned`, so this
 *     fits trivially. Not yet verified by an actual RIOT-OS build.
 *   - Zephyr: k_spinlock_key_t is expected to be a small integer-sized
 *     struct that fits in one `unsigned long`, based on general
 *     familiarity with Zephyr's spinlock API shape -- but this has NOT
 *     been checked against a real Zephyr checkout (no toolchain access
 *     at the time this was written). Verify this for real during the
 *     Zephyr adapter port (docs/planning.md step 3), before trusting
 *     it. If k_spinlock_key_t doesn't fit in one word, fi_port_key_t
 *     needs to grow (e.g. a fixed-size byte array) or become a pointer
 *     to adapter-owned storage instead -- don't just widen it blindly;
 *     re-check both adapters against whatever the new shape is.
 */

#ifndef FI_PORT_H
#define FI_PORT_H

typedef struct {
    unsigned long state;
} fi_port_key_t;

/* Enter an IRQ-safe critical section. Must be safe to call from ISR
 * context (a fault point may be hit from one). Returns an opaque token
 * that must be passed back to fi_port_unlock() to restore the prior
 * interrupt/lock state -- callers must not inspect or construct this
 * value themselves. */
fi_port_key_t fi_port_lock(void);

/* Leave the critical section entered by the matching fi_port_lock()
 * call. key must be the exact value that call returned. Lock/unlock
 * calls must nest correctly (this codebase only ever nests them
 * correctly by construction -- each core function takes the lock once
 * and unlocks it once before returning) and must not interleave with a
 * mismatched key. */
void fi_port_unlock(fi_port_key_t key);

#endif /* FI_PORT_H */
