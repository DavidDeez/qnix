###############################################################################
# SQLite 3.47.0 — QNX 8.0 Cross-Build Makefile
#
# Upstream: https://github.com/sqlite/sqlite
# License:  Public Domain
#
# Usage:
#   make -C ports/sqlite install -j$(nproc)
#   make -C ports/sqlite install CPU=x86_64 -j$(nproc)
#   make -C ports/sqlite install CPU=aarch64le -j$(nproc)
###############################################################################

SQLITE_VERSION      := 3470000
SQLITE_VERSION_DOTS := 3.47.0
SQLITE_YEAR         := 2024
SQLITE_ZIP          := sqlite-amalgamation-$(SQLITE_VERSION).zip
SQLITE_URL          := https://www.sqlite.org/$(SQLITE_YEAR)/$(SQLITE_ZIP)
SQLITE_DIR          := sqlite-amalgamation-$(SQLITE_VERSION)

# --- QNX Toolchain Setup ---
# Source ~/qnx800/qnxsdp-env.sh before running this Makefile.
# QNX_TARGET and QNX_HOST must be set.

ifndef QNX_TARGET
$(error QNX_TARGET is not set. Please source ~/qnx800/qnxsdp-env.sh)
endif

ifndef QNX_HOST
$(error QNX_HOST is not set. Please source ~/qnx800/qnxsdp-env.sh)
endif

# Default to aarch64le (Raspberry Pi 5) if CPU not specified
CPU ?= aarch64le

# Select cross-compiler based on CPU target
ifeq ($(CPU),aarch64le)
    CC      := $(QNX_HOST)/usr/bin/aarch64-unknown-nto-qnx8.0.0-gcc
    AR      := $(QNX_HOST)/usr/bin/aarch64-unknown-nto-qnx8.0.0-ar
    STRIP   := $(QNX_HOST)/usr/bin/aarch64-unknown-nto-qnx8.0.0-strip
    SYSROOT := $(QNX_TARGET)/aarch64le
else ifeq ($(CPU),x86_64)
    CC      := $(QNX_HOST)/usr/bin/x86_64-pc-nto-qnx8.0.0-gcc
    AR      := $(QNX_HOST)/usr/bin/x86_64-pc-nto-qnx8.0.0-ar
    STRIP   := $(QNX_HOST)/usr/bin/x86_64-pc-nto-qnx8.0.0-strip
    SYSROOT := $(QNX_TARGET)/x86_64
else
$(error Unsupported CPU: $(CPU). Use CPU=aarch64le or CPU=x86_64)
endif

INSTALL_PREFIX  := $(SYSROOT)/usr/local
INCLUDE_DIR     := $(INSTALL_PREFIX)/include
LIB_DIR         := $(INSTALL_PREFIX)/lib
BIN_DIR         := $(INSTALL_PREFIX)/bin

BUILD_DIR       := build/$(CPU)

# --- Compiler Flags ---
# -D_QNX_SOURCE     : Enable QNX-specific POSIX extensions
# SQLITE_THREADSAFE : Use pthreads for thread safety
# HAVE_USLEEP       : QNX supports usleep() (POSIX)
# HAVE_LOCALTIME_R  : QNX provides thread-safe localtime_r()
# SQLITE_USE_URI    : Enable URI filenames (useful for in-memory DBs)
# SQLITE_ENABLE_FTS5: Enable Full-Text Search version 5
# SQLITE_ENABLE_JSON1: Enable JSON SQL functions
CFLAGS := \
    --sysroot=$(SYSROOT) \
    -I$(SYSROOT)/usr/include \
    -D_QNX_SOURCE \
    -DSQLITE_OS_OTHER=0 \
    -DHAVE_USLEEP=1 \
    -DHAVE_LOCALTIME_R=1 \
    -DSQLITE_THREADSAFE=1 \
    -DSQLITE_USE_URI=1 \
    -DSQLITE_ENABLE_FTS5=1 \
    -DSQLITE_ENABLE_JSON1=1 \
    -DSQLITE_ENABLE_RTREE=1 \
    -DSQLITE_ENABLE_COLUMN_METADATA=1 \
    -O2 -fPIC -Wall

LDFLAGS := \
    --sysroot=$(SYSROOT) \
    -L$(SYSROOT)/usr/lib \
    -lpthread \
    -ldl \
    -lm

SO_VERSION_MAJOR := 0
SO_VERSION_MINOR := 8
SO_VERSION_PATCH := 6
SO_FULL_VERSION  := $(SO_VERSION_MAJOR).$(SO_VERSION_MINOR).$(SO_VERSION_PATCH)

LIBSO_VERSIONED  := libsqlite3.so.$(SO_FULL_VERSION)
LIBSO_MAJOR      := libsqlite3.so.$(SO_VERSION_MAJOR)
LIBSO_LINK       := libsqlite3.so

.PHONY: all download extract build build-lib build-cli install clean distclean

all: build

## 1. Download SQLite amalgamation
download: $(SQLITE_ZIP)

$(SQLITE_ZIP):
	@echo ">>> Downloading SQLite $(SQLITE_VERSION_DOTS) amalgamation..."
	wget -q --show-progress "$(SQLITE_URL)" -O $(SQLITE_ZIP)

## 2. Extract
extract: download $(SQLITE_DIR)/sqlite3.c

$(SQLITE_DIR)/sqlite3.c: $(SQLITE_ZIP)
	@echo ">>> Extracting $(SQLITE_ZIP)..."
	unzip -q -o $(SQLITE_ZIP)

## 3. Build
build: build-lib build-cli

build-lib: $(SQLITE_DIR)/sqlite3.c
	@echo ">>> Building libsqlite3 for QNX (CPU=$(CPU))..."
	mkdir -p $(BUILD_DIR)
	# Compile amalgamation to position-independent object
	$(CC) $(CFLAGS) -c $(SQLITE_DIR)/sqlite3.c -o $(BUILD_DIR)/sqlite3.o
	# Build shared library
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(LIBSO_MAJOR) \
	    $(BUILD_DIR)/sqlite3.o -o $(BUILD_DIR)/$(LIBSO_VERSIONED) \
	    $(LDFLAGS)
	# Build static library
	$(AR) rcs $(BUILD_DIR)/libsqlite3.a $(BUILD_DIR)/sqlite3.o
	@echo ">>> libsqlite3 built successfully."

build-cli: $(SQLITE_DIR)/sqlite3.c build-lib
	@echo ">>> Building sqlite3 CLI shell for QNX (CPU=$(CPU))..."
	$(CC) $(CFLAGS) \
	    -I$(SQLITE_DIR) \
	    $(SQLITE_DIR)/shell.c \
	    $(BUILD_DIR)/sqlite3.o \
	    -o $(BUILD_DIR)/sqlite3 \
	    $(LDFLAGS) -lreadline -lncurses 2>/dev/null || \
	$(CC) $(CFLAGS) \
	    -I$(SQLITE_DIR) \
	    -DHAVE_READLINE=0 \
	    $(SQLITE_DIR)/shell.c \
	    $(BUILD_DIR)/sqlite3.o \
	    -o $(BUILD_DIR)/sqlite3 \
	    $(LDFLAGS)
	@echo ">>> sqlite3 CLI built successfully."

## 4. Install to QNX_TARGET sysroot
install: build
	@echo ">>> Installing SQLite $(SQLITE_VERSION_DOTS) to $(INSTALL_PREFIX)..."
	install -d $(INCLUDE_DIR) $(LIB_DIR) $(BIN_DIR)
	# Headers
	install -m 644 $(SQLITE_DIR)/sqlite3.h $(INCLUDE_DIR)/sqlite3.h
	install -m 644 $(SQLITE_DIR)/sqlite3ext.h $(INCLUDE_DIR)/sqlite3ext.h
	# Shared library with versioned symlinks
	install -m 755 $(BUILD_DIR)/$(LIBSO_VERSIONED) $(LIB_DIR)/$(LIBSO_VERSIONED)
	ln -sf $(LIBSO_VERSIONED) $(LIB_DIR)/$(LIBSO_MAJOR)
	ln -sf $(LIBSO_MAJOR)     $(LIB_DIR)/$(LIBSO_LINK)
	# Static library
	install -m 644 $(BUILD_DIR)/libsqlite3.a $(LIB_DIR)/libsqlite3.a
	# CLI binary (stripped)
	$(STRIP) $(BUILD_DIR)/sqlite3
	install -m 755 $(BUILD_DIR)/sqlite3 $(BIN_DIR)/sqlite3
	@echo ""
	@echo "=== SQLite installation complete ==="
	@echo "  Headers : $(INCLUDE_DIR)/sqlite3.h"
	@echo "  Library : $(LIB_DIR)/$(LIBSO_VERSIONED)"
	@echo "  Static  : $(LIB_DIR)/libsqlite3.a"
	@echo "  Binary  : $(BIN_DIR)/sqlite3"

## 5. Clean
clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf $(SQLITE_DIR) $(SQLITE_ZIP)
