###############################################################################
# MicroPython 1.24.0 — QNX 8.0 Cross-Build Makefile
#
# Upstream:  https://github.com/micropython/micropython
# License:   MIT
#
# This Makefile builds MicroPython using the 'unix' port as a base,
# adapted for the QNX 8.0 real-time operating system.
#
# MicroPython is linked against libsqlite3 to enable the sqlite3 module,
# providing persistent database storage on QNX targets.
#
# Usage:
#   make -C ports/micropython install -j$(nproc)
#   make -C ports/micropython install CPU=x86_64 -j$(nproc)
#   make -C ports/micropython install CPU=aarch64le -j$(nproc)
#
# Prerequisites:
#   - QNX SDP 8.0 sourced (source ~/qnx800/qnxsdp-env.sh)
#   - SQLite installed (make -C ports/sqlite install)
#   - Python 3 on host (for MicroPython's build scripts)
#   - git on host
###############################################################################

MICROPYTHON_VERSION := 1.24.0
MICROPYTHON_REPO    := https://github.com/micropython/micropython.git
MICROPYTHON_TAG     := v$(MICROPYTHON_VERSION)
MICROPYTHON_DIR     := micropython-$(MICROPYTHON_VERSION)

# --- QNX Toolchain Setup ---
ifndef QNX_TARGET
$(error QNX_TARGET is not set. Please source ~/qnx800/qnxsdp-env.sh)
endif

ifndef QNX_HOST
$(error QNX_HOST is not set. Please source ~/qnx800/qnxsdp-env.sh)
endif

CPU ?= aarch64le

ifeq ($(CPU),aarch64le)
    CROSS_COMPILE := $(QNX_HOST)/usr/bin/aarch64-unknown-nto-qnx8.0.0-
    SYSROOT       := $(QNX_TARGET)/aarch64le
    VARIANT       := le
else ifeq ($(CPU),x86_64)
    CROSS_COMPILE := $(QNX_HOST)/usr/bin/x86_64-pc-nto-qnx8.0.0-
    SYSROOT       := $(QNX_TARGET)/x86_64
    VARIANT       :=
else
$(error Unsupported CPU: $(CPU). Use CPU=aarch64le or CPU=x86_64)
endif

CC      := $(CROSS_COMPILE)gcc
CXX     := $(CROSS_COMPILE)g++
AR      := $(CROSS_COMPILE)ar
STRIP   := $(CROSS_COMPILE)strip
READELF := $(CROSS_COMPILE)readelf

INSTALL_PREFIX := $(SYSROOT)/usr/local
BIN_DIR        := $(INSTALL_PREFIX)/bin

SQLITE_INCLUDE := $(SYSROOT)/usr/local/include
SQLITE_LIB     := $(SYSROOT)/usr/local/lib

BUILD_DIR := build/$(CPU)

# --- Host Python (for MicroPython build system) ---
PYTHON := python3

# --- QNX-Specific Compiler Flags ---
QNX_CFLAGS := \
    --sysroot=$(SYSROOT) \
    -I$(SYSROOT)/usr/include \
    -I$(SQLITE_INCLUDE) \
    -D_QNX_SOURCE \
    -D__QNX__ \
    -DMICROPY_NLR_SETJMP=1 \
    -O2 -Wall -Wno-unused-but-set-variable

QNX_LDFLAGS := \
    --sysroot=$(SYSROOT) \
    -L$(SYSROOT)/usr/lib \
    -L$(SQLITE_LIB) \
    -lsqlite3 \
    -lpthread \
    -ldl \
    -lm \
    -lsocket

# MicroPython build variant name for QNX
VARIANT_NAME := qnx-$(CPU)

.PHONY: all clone patch submodules build install clean distclean

all: build

## 1. Clone MicroPython
clone: $(MICROPYTHON_DIR)/.git

$(MICROPYTHON_DIR)/.git:
	@echo ">>> Cloning MicroPython $(MICROPYTHON_VERSION)..."
	git clone --depth 1 --branch $(MICROPYTHON_TAG) $(MICROPYTHON_REPO) $(MICROPYTHON_DIR)

## 2. Initialize submodules (required for MicroPython)
submodules: clone
	@echo ">>> Initializing MicroPython submodules..."
	cd $(MICROPYTHON_DIR) && git submodule update --init --depth 1 \
	    lib/micropython-lib \
	    lib/berkeley-db-1.xx \
	    lib/axtls

## 3. Apply QNX patches
patch: submodules patches/0001-add-qnx-platform.patch
	@echo ">>> Applying QNX patches..."
	cd $(MICROPYTHON_DIR) && \
	    for p in $(CURDIR)/patches/*.patch; do \
	        echo "  Applying $$p"; \
	        git apply --ignore-whitespace "$$p" || echo "  (patch may already be applied, continuing)"; \
	    done

## 4. Build MicroPython cross-compiler (required by MicroPython build system)
mpy-cross: clone submodules
	@echo ">>> Building mpy-cross (host tool)..."
	$(MAKE) -C $(MICROPYTHON_DIR)/mpy-cross

## 5. Build MicroPython for QNX
build: patch mpy-cross
	@echo ">>> Building MicroPython $(MICROPYTHON_VERSION) for QNX (CPU=$(CPU))..."
	mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(MICROPYTHON_DIR)/ports/unix \
	    CROSS_COMPILE="$(CROSS_COMPILE)" \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    STRIP="$(STRIP)" \
	    CFLAGS_EXTRA="$(QNX_CFLAGS)" \
	    LDFLAGS_EXTRA="$(QNX_LDFLAGS)" \
	    MICROPY_PY_USSL=0 \
	    MICROPY_SSL_AXTLS=0 \
	    MICROPY_PY_WEBREPL=0 \
	    BUILD="$(CURDIR)/$(BUILD_DIR)" \
	    PROG="micropython-qnx" \
	    -j$(shell nproc) \
	    all
	@echo ">>> MicroPython built: $(BUILD_DIR)/micropython-qnx"

## 6. Install to QNX_TARGET sysroot
install: build
	@echo ">>> Installing MicroPython to $(BIN_DIR)..."
	install -d $(BIN_DIR)
	$(STRIP) $(BUILD_DIR)/micropython-qnx
	install -m 755 $(BUILD_DIR)/micropython-qnx $(BIN_DIR)/micropython
	@echo ""
	@echo "=== MicroPython installation complete ==="
	@echo "  Binary: $(BIN_DIR)/micropython"
	@echo ""
	@echo "  To verify: scp and run on QNX target:"
	@echo "    micropython -c \"import sys; print(sys.version)\""

## Shortcuts for all CPU targets
install-all:
	$(MAKE) install CPU=aarch64le
	$(MAKE) install CPU=x86_64

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C $(MICROPYTHON_DIR)/ports/unix clean 2>/dev/null || true

distclean: clean
	rm -rf $(MICROPYTHON_DIR)
