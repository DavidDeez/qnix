###############################################################################
# MicroPython User C Module Build File for `sqlite3` on QNX 8.0
#
# Included automatically by MicroPython's build system when
# USER_C_MODULES is passed to `make -C ports/unix`.
###############################################################################

SQLITE3_MOD_DIR := $(USERMOD_DIR)

# Add our C module source to the MicroPython user module sources
SRC_USERMOD_C += $(SQLITE3_MOD_DIR)/modsqlite3.c

# Include path for module headers
CFLAGS_USERMOD += -I$(SQLITE3_MOD_DIR)
