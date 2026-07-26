/* test_disabled_compiles_out.c
 *
 * Compiled WITHOUT -DCONFIG_FAULT_INJECTION, and with neither
 * src/fault_inject.c nor tests/host/fi_port_host.c linked in at all --
 * see the Makefile. This proves FI_POINT(id, real_call) truly expands
 * to exactly (real_call): the binary has no fault-injection symbols in
 * it whatsoever, and normal behavior is unaffected. Generalized from
 * C-MSP's tests/drivers/eswifi_recv/src/test_disabled_compiles_out.c,
 * against the portable core instead of a specific driver repro.
 */

#include "fault_inject.h"
#include <stdio.h>

#ifdef CONFIG_FAULT_INJECTION
#error "This test must be built with CONFIG_FAULT_INJECTION undefined"
#endif

static int add_one(int x)
{
    return x + 1;
}

int main(void)
{
    int rc = FI_POINT(1, add_one(41));

    if (rc != 42) {
        fprintf(stderr, "FAIL: FI_POINT altered the real call's result "
                         "with fault injection compiled out (got %d, want 42)\n",
                rc);
        return 1;
    }
    printf("  ok: FI_POINT is a pass-through with fault injection compiled out\n");
    printf("all checks passed\n");
    return 0;
}
