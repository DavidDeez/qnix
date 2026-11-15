###############################################################################
# MicroPython User C Module Build File for `qnx_rt` on QNX 8.0
#
# Exposes QNX Neutrino RTOS kernel primitives (<sys/neutrino.h>,
# <sys/syspage.h>, ClockCycles(), ClockPeriod(), SCHED_FIFO / SCHED_RR)
# to MicroPython scripts.
###############################################################################

QNX_RT_MOD_DIR := $(USERMOD_DIR)

SRC_USERMOD_C += $(QNX_RT_MOD_DIR)/modqnxrt.c

CFLAGS_USERMOD += -I$(QNX_RT_MOD_DIR)
