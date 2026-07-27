# Host-only build/test path for the portable fault-injection core
# (src/fault_inject.c), exercised through the host stub port
# (tests/host/fi_port_host.c -- NOT a real RTOS adapter, see that
# file). This does not build or verify either RTOS adapter -- both now
# exist and are verified (see docs/verification.md), but that requires
# each RTOS's own toolchain (west/Twister for Zephyr, RIOT's own build
# system for RIOT-OS): `west twister -T tests/zephyr/eswifi_recv` and
# `RIOTBASE=/path/to/RIOT BOARD=native make` under tests/riot/, per
# README.md.

CC = gcc
INC = -Iinclude
WARN = -Wall -Wextra -Werror
SAN = -fsanitize=address,undefined -fno-omit-frame-pointer -g
STD = -std=c11

.PHONY: all test clean

all: test

# --- Core test: portable registry logic + FI_POINT contract, against
#     the host stub port. FI_MAX_POINTS is deliberately overridden to a
#     small value here so the table-full test is fast and easy to
#     hand-verify -- see the comment on test_table_full_is_handled_safely.
build/test_core: src/fault_inject.c tests/host/fi_port_host.c tests/host/test_fault_inject_core.c
	@mkdir -p build
	$(CC) $(STD) $(WARN) $(SAN) -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4 $(INC) $^ -o $@

# --- Disabled build: CONFIG_FAULT_INJECTION is NOT defined, and
#     neither fault_inject.c nor fi_port_host.c is linked in at all.
#     Proves FI_POINT compiles to exactly the original call -- no
#     fault-injection machinery is even present in the binary.
build/test_disabled: tests/host/test_disabled_compiles_out.c
	@mkdir -p build
	$(CC) $(STD) $(WARN) $(SAN) $(INC) $^ -o $@

test: build/test_core build/test_disabled
	@echo "=== core test (expected: all checks pass) ==="
	./build/test_core
	@echo ""
	@echo "=== disabled build (expected: all checks pass, no FI symbols linked) ==="
	./build/test_disabled

clean:
	rm -rf build
