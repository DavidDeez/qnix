###############################################################################
# Master MicroPython User C Modules Manifest for QNX 8.0
#
# Includes both:
#   1. sqlite3 — Native libsqlite3 database bindings
#   2. qnx_rt  — Native QNX Neutrino (<sys/neutrino.h>) real-time primitives
###############################################################################

include $(USERMOD_DIR)/sqlite3/micropython.mk
include $(USERMOD_DIR)/qnx_rt/micropython.mk
