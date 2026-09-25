#
# PS4 (OpenOrbis) MIDP build configuration. Shares the Vita platform
# sources (TARGET_PLATFORM = vita); PS4-specific code is under #ifdef PS4.
# Modeled on ../linux_sdl_gcc/GNUmakefile and ../linux_stubs_gcc/GNUmakefile.
#
HOST_PLATFORM           = linux
HOST_OS                 = linux
HOST_CPU                = i386
HOST_COMPILER           = gcc
TARGET_PLATFORM         = vita
TARGET_OS               = ps4
TARGET_CPU              = c
TARGET_COMPILER         = gcc
TARGET_VM               = cldc_vm
FN_SEP=/
MIDP_DIR                = $(shell cygpath -m $(abspath $(CURDIR)/../..))
ifndef MIDP_OUTPUT_DIR
MIDP_OUTPUT_DIR         = $(shell cygpath -m $(CURDIR)/output)
endif
include Options.gmk
include $(MIDP_DIR)/build/common/Top.gmk
include ps4.gmk
