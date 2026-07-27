# RIOT module for the portable fault-injection core's RIOT-OS adapter.
#
# RIOT discovers external modules by requiring
# $(EXTERNAL_MODULE_DIRS)/<module-name>/Makefile to exist, where
# <module-name> is exactly the string an app puts in USEMODULE (see
# makefiles/dependency_resolution.inc.mk) -- the lookup is on the
# directory name itself, not on any MODULE variable set inside it. This
# directory is therefore named "fault_inject", not "riot": an earlier
# version of this file set MODULE := fault_inject while the directory
# was still called "riot" and assumed that satisfied discovery -- it
# doesn't; that only sets the module's identity *after* it's found.
# Caught by an actual failed build (adapters/riot/Makefile was silently
# never -included), not by re-reading this comment. Makefile.base's own
# `basename $(CURDIR)` default now already resolves to "fault_inject",
# so no explicit MODULE assignment is needed here at all.

# The portable core (src/fault_inject.c) lives at the repo root, shared
# with the Zephyr adapter -- it is deliberately not copied here. RIOT's
# module auto-detection (Makefile.base's `SRC := $(wildcard *.c)`) only
# ever sees files physically inside this directory, so SRC is set
# explicitly instead: this adapter's own port file, plus the one core
# translation unit, and nothing else.
#
# fault_inject.c is referenced by bare filename, with VPATH pointing at
# its real directory, rather than `SRC += ../../src/fault_inject.c`. An
# earlier version of this file did that directly, and it silently built
# with no error -- but Makefile.base derives each object's path from its
# SRC entry verbatim ($(SRC:%.c=$(BINDIR)/$(MODULE)/%.o)), so a `../../`
# in SRC produces an object two directories above where the final
# link's `find $(BINDIR)/fault_inject/ -name "*.o"` ever looks. The
# object compiled correctly and then silently took no part in the link,
# surfacing as "undefined reference to fi_arm" etc. Caught by actually
# linking a real RIOT app against this module, not by reading this
# Makefile. With VPATH, GNU Make itself resolves the bare `fault_inject.c`
# prerequisite to `../../src/fault_inject.c` (real path preserved in
# debug info and in $< for the compile command), while the object
# target path -- derived from the bare SRC entry -- correctly stays
# under $(BINDIR)/$(MODULE)/.
VPATH += ../../src
SRC += fi_port_riot.c
SRC += fault_inject.c

include $(RIOTBASE)/Makefile.base
