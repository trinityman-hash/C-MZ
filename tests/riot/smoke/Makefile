# RIOT-OS adapter smoke test (docs/planning.md step 5): proves
# adapters/riot/fi_port_riot.c actually compiles and links against a
# real RIOT-OS checkout, and that the portable core behaves correctly
# through it -- not the same thing as a host-only build with the no-op
# stub port (tests/host/), and not yet the real bug-reproduction proof
# case planning.md step 6 still calls for.
#
# This app lives outside any RIOT-OS checkout (RIOT is not vendored
# into this repo), so RIOTBASE must be supplied by the caller, e.g.:
#
#   RIOTBASE=/path/to/RIOT BOARD=native make
#   ./bin/native/fault_inject_riot_smoke.elf
#
# There is deliberately no relative-path default for RIOTBASE (unlike
# RIOT's own in-tree app examples, which default to
# $(CURDIR)/../../..) -- that default only makes sense for apps that
# live inside a RIOT checkout, which this one does not.

APPLICATION = fault_inject_riot_smoke
BOARD ?= native

ifeq (,$(RIOTBASE))
  $(error RIOTBASE is not set. Point it at a real RIOT-OS checkout, e.g. \
    "RIOTBASE=/path/to/RIOT make")
endif

EXTERNAL_MODULE_DIRS += $(CURDIR)/../../../adapters
USEMODULE += fault_inject

# Deliberately small, matching the equivalent host-test override in the
# root Makefile -- keeps the table-full test fast and easy to
# hand-verify.
CFLAGS += -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4

DEVELHELP ?= 1
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
