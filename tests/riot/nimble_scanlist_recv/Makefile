# CVE-2024-32018 reproduction (docs/planning.md step 6) -- see
# src/scanlist_repro.h for the real bug this rebuilds.
#
# Same invocation convention as ../smoke/Makefile:
#
#   RIOTBASE=/path/to/RIOT BOARD=native make
#   ./bin/native/nimble_scanlist_recv.elf
#
# For the buggy variant under ASan (the actual crash proof, same
# discipline as tests/zephyr/eswifi_recv's buggy variant -- built and
# run by hand, not a permanent green-checkmark suite entry):
#
#   RIOTBASE=/path/to/RIOT BOARD=native SCANLIST_REPRO_VARIANT=buggy \
#     make all-asan
#   ./bin/native/nimble_scanlist_recv.elf

APPLICATION = nimble_scanlist_recv
BOARD ?= native

ifeq (,$(RIOTBASE))
  $(error RIOTBASE is not set. Point it at a real RIOT-OS checkout, e.g. \
    "RIOTBASE=/path/to/RIOT make")
endif

EXTERNAL_MODULE_DIRS += $(CURDIR)/../../../adapters
USEMODULE += fault_inject

# Deliberately small, matching every other build in this repo -- see
# ../smoke/Makefile's identical comment.
CFLAGS += -DCONFIG_FAULT_INJECTION -DCONFIG_FAULT_INJECTION_MAX_POINTS=4

# Sources live under src/, not this Makefile's own directory, so RIOT's
# default `SRC := $(wildcard *.c)` (Makefile.base, applied to the
# application module same as any other) would find nothing -- SRC is
# set explicitly instead. Unlike adapters/fault_inject/Makefile's own
# fault_inject.c reference, these paths descend into a subdirectory
# rather than climbing out of one, so they stay inside this
# application's own BINDIR subtree and need no VPATH trick.
SRC += src/main.c

SCANLIST_REPRO_VARIANT ?= fixed

ifeq ($(SCANLIST_REPRO_VARIANT),fixed)
  SRC += src/scanlist_repro_fixed.c
else ifeq ($(SCANLIST_REPRO_VARIANT),buggy)
  SRC += src/scanlist_repro_buggy.c
  # Real precondition from GHSA-899m-q6pp-hmp3 ("while assertions are
  # compiled-out") -- see src/scanlist_repro_buggy.c's header comment.
  CFLAGS += -DNDEBUG
else
  $(error SCANLIST_REPRO_VARIANT must be "fixed" or "buggy", got \
    "$(SCANLIST_REPRO_VARIANT)")
endif

DEVELHELP ?= 1
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
