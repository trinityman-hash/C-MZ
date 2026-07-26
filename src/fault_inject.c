/* fault_inject.c
 *
 * Deterministic, table-backed fault registry. Only built when
 * CONFIG_FAULT_INJECTION is defined -- see fault_inject.h for why the
 * header makes FI_POINT vanish entirely otherwise.
 *
 * Generalized from C-MSP's Zephyr-only src/fault_inject.c
 * (https://github.com/trinityman-hash/C-MSP/blob/main/src/fault_inject.c).
 * The registry logic (fi_lookup, fi_lookup_or_create, and all five
 * public functions) is unchanged from C-MSP. The only real change:
 * locking no longer branches on `#ifdef __ZEPHYR__` inside this file --
 * every access takes fi_port_lock()/fi_port_unlock() unconditionally,
 * and it's each adapter's job (including the host test harness) to
 * supply a correct implementation of that interface. This file has zero
 * RTOS-specific code and zero RTOS header dependency, by construction.
 */

#ifdef CONFIG_FAULT_INJECTION

#include "fault_inject.h"
#include "fi_port.h"
#include <stdbool.h>
#include <stddef.h>

struct fi_entry {
    uint32_t id;
    bool in_use;
    bool armed;
    int inject_value;
    uint32_t hit_count;
};

static struct fi_entry fi_table[FI_MAX_POINTS];

/* Both helpers assume the caller already holds the port lock. */

static struct fi_entry *fi_lookup(uint32_t fault_id)
{
    for (int i = 0; i < FI_MAX_POINTS; i++) {
        if (fi_table[i].in_use && fi_table[i].id == fault_id) {
            return &fi_table[i];
        }
    }
    return NULL;
}

static struct fi_entry *fi_lookup_or_create(uint32_t fault_id)
{
    struct fi_entry *e = fi_lookup(fault_id);
    if (e != NULL) {
        return e;
    }
    for (int i = 0; i < FI_MAX_POINTS; i++) {
        if (!fi_table[i].in_use) {
            fi_table[i].in_use = true;
            fi_table[i].id = fault_id;
            fi_table[i].armed = false;
            fi_table[i].inject_value = 0;
            fi_table[i].hit_count = 0;
            return &fi_table[i];
        }
    }
    return NULL; /* table full; caller treats as "not armed" */
}

int fi_should_fail(uint32_t fault_id)
{
    fi_port_key_t key = fi_port_lock();
    struct fi_entry *e = fi_lookup_or_create(fault_id);
    if (e == NULL) {
        fi_port_unlock(key);
        return 0;
    }
    e->hit_count++;
    int rc = e->armed ? e->inject_value : 0;
    fi_port_unlock(key);
    return rc;
}

void fi_arm(uint32_t fault_id, int inject_value)
{
    fi_port_key_t key = fi_port_lock();
    struct fi_entry *e = fi_lookup_or_create(fault_id);
    if (e != NULL) {
        e->armed = true;
        e->inject_value = inject_value;
    }
    fi_port_unlock(key);
}

void fi_disarm(uint32_t fault_id)
{
    fi_port_key_t key = fi_port_lock();
    struct fi_entry *e = fi_lookup(fault_id);
    if (e != NULL) {
        e->armed = false;
    }
    fi_port_unlock(key);
}

void fi_reset_all(void)
{
    fi_port_key_t key = fi_port_lock();
    for (int i = 0; i < FI_MAX_POINTS; i++) {
        fi_table[i].in_use = false;
        fi_table[i].armed = false;
        fi_table[i].inject_value = 0;
        fi_table[i].hit_count = 0;
        fi_table[i].id = 0;
    }
    fi_port_unlock(key);
}

uint32_t fi_hit_count(uint32_t fault_id)
{
    fi_port_key_t key = fi_port_lock();
    struct fi_entry *e = fi_lookup(fault_id);
    uint32_t count = e != NULL ? e->hit_count : 0;
    fi_port_unlock(key);
    return count;
}

#endif /* CONFIG_FAULT_INJECTION */
