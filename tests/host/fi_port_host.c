/* fi_port_host.c
 *
 * Host-only stub implementation of fi_port.h. This is NOT one of the
 * v0 adapters (Zephyr, RIOT-OS) and must never be mistaken for one --
 * it exists purely so src/fault_inject.c can be compiled and
 * unit-tested on a bare host, with no RTOS present at all, mirroring
 * C-MSP's host-only build path.
 *
 * No-op locking is safe here ONLY because these host tests
 * (test_fault_inject_core.c) are single-threaded, run nothing from
 * signal/interrupt context, and don't exercise concurrent access --
 * same caveat C-MSP's host build carried. Linking this into anything
 * multi-threaded or ISR-capable would silently remove the registry's
 * IRQ-safety and is a correctness bug, not a style choice.
 */

#include "fi_port.h"

fi_port_key_t fi_port_lock(void)
{
    fi_port_key_t key = { 0 };
    return key;
}

void fi_port_unlock(fi_port_key_t key)
{
    (void)key;
}
