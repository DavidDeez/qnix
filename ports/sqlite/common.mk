###############################################################################
# SQLite 3.47.0 — QNX 8.0 Common Build Configuration (qconfig.mk / qmacros.mk)
#
# Can be included by QNX recursive Makefiles (LIST=OS CPU VARIANT)
###############################################################################

ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

NAME=sqlite3
USEFILE=

# QNX 8.0 POSIX and SQLite feature flags
CCFLAGS += -D_QNX_SOURCE \
           -DSQLITE_OS_OTHER=0 \
           -DHAVE_USLEEP=1 \
           -DHAVE_LOCALTIME_R=1 \
           -DSQLITE_THREADSAFE=1 \
           -DSQLITE_USE_URI=1 \
           -DSQLITE_ENABLE_FTS5=1 \
           -DSQLITE_ENABLE_JSON1=1 \
           -DSQLITE_ENABLE_RTREE=1 \
           -DSQLITE_ENABLE_COLUMN_METADATA=1 \
           -O2

LIBS += pthread dl m

include $(MKFILES_ROOT)/qtargets.mk
