# MicroPython Port for QNX 8.0

[![Build](https://img.shields.io/badge/QNX%20SDP-8.0-black)](https://blackberry.qnx.com)
[![MicroPython](https://img.shields.io/badge/MicroPython-1.24.0-green)](https://micropython.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/micropython/micropython/blob/master/LICENSE)

**NOTE**: QNX ports are only supported from a **Linux host** operating system.

Upstream project: https://github.com/micropython/micropython

---

## What is MicroPython?

MicroPython is a lean and efficient implementation of the Python 3 programming language that is optimised to run on microcontrollers and constrained environments. This port adapts MicroPython's `unix` platform for QNX 8.0's POSIX-compatible environment, producing a full Python 3 interpreter that runs natively on QNX targets.

This port includes:
- **Full MicroPython REPL** — interactive Python shell on QNX
- **SQLite3 module** — persistent database via the SQLite port
- **threading (`_thread`)** — using QNX pthreads
- **`utime` / `time`** — microsecond-precision timing using QNX `CLOCK_MONOTONIC`
- **`uos`** — OS interface for QNX filesystem (`/data/`, `/tmp/`, `/proc/`)
- **`usocket`** — TCP/UDP networking using QNX socket stack

---

## QNX Platform Notes

MicroPython's `unix` port is designed for POSIX systems. QNX 8.0 is fully POSIX.1-2017 compliant, making it an excellent target. The key adaptations in this port are:

| Adaptation | Details |
|-----------|---------|
| `__QNX__` define | Added to `mpconfigport.h` to gate QNX-specific code paths |
| `_QNX_SOURCE` | Enables QNX extensions (e.g., `sched_get_priority_max`, `SIGEV_*`) |
| Socket headers | Uses `<sys/socket.h>` and `<netinet/in.h>` from QNX libc |
| `CLOCK_MONOTONIC` | QNX supports high-resolution monotonic clock for `utime.ticks_us()` |
| pthread linking | Links against QNX's native pthread implementation |
| NLR (Non-Local Return) | Uses setjmp/longjmp variant for QNX (safe across all CPU targets) |

---

## Prerequisites

1. **SQLite must be built first** (MicroPython links against `libsqlite3`):
   ```bash
   make -C ports/sqlite install CPU=aarch64le -j$(nproc)
   ```

2. **Host dependencies**:
   ```bash
   sudo apt-get install -y python3 git make gcc
   ```

---

## Build Instructions

### Option 1: Docker

```bash
# Start Docker container (see qnx-ports/build-files docker/)
source ~/qnx800/qnxsdp-env.sh

# First build SQLite (dependency)
make -C ports/sqlite install CPU=aarch64le -j$(nproc)

# Then build MicroPython
make -C ports/micropython install CPU=aarch64le -j$(nproc)
```

### Option 2: Native Linux Host

```bash
source ~/qnx800/qnxsdp-env.sh

# Install SQLite first
make -C ports/sqlite install -j$(nproc)

# Build MicroPython
make -C ports/micropython install -j$(nproc)
```

---

## Running on QNX Target

### Transfer Binaries

```bash
TARGET_HOST=<target-ip-or-hostname>

scp $QNX_TARGET/aarch64le/usr/local/bin/micropython qnxuser@$TARGET_HOST:/data/home/qnxuser/
scp $QNX_TARGET/aarch64le/usr/local/lib/libsqlite3.so.0.8.6 qnxuser@$TARGET_HOST:/data/home/qnxuser/lib/
```

### Basic Usage

```bash
ssh qnxuser@$TARGET_HOST

export LD_LIBRARY_PATH=/data/home/qnxuser/lib:$LD_LIBRARY_PATH

# Interactive REPL
./micropython

# Run a script
./micropython /data/home/qnxuser/myscript.py

# One-liner
./micropython -c "import sys; print('Hello from QNX!', sys.version)"
```

### SQLite from MicroPython

```python
# On QNX target, in MicroPython REPL:
import sqlite3

conn = sqlite3.connect('/tmp/test.db')
cursor = conn.cursor()
cursor.execute('CREATE TABLE IF NOT EXISTS t (x INTEGER, y REAL)')
cursor.execute('INSERT INTO t VALUES (1, 3.14)')
conn.commit()

for row in cursor.execute('SELECT * FROM t'):
    print(row)

conn.close()
```

---

## QNX Patches Applied

### `0001-add-qnx-platform.patch`
Adds `__QNX__` and `_QNX_SOURCE` definitions to `ports/unix/mpconfigport.h`:
- Disables Linux-specific features not available on QNX (e.g., `HAVE_AF_PACKET`)
- Enables QNX socket extensions
- Sets `MICROPY_NLR_SETJMP=1` for safe non-local returns on all QNX targets

### `0002-qnx-thread-support.patch`
Adapts `ports/unix/moduthread.c` for QNX pthreads:
- Uses QNX-compatible thread priority range via `sched_get_priority_max(SCHED_FIFO)`
- Adjusts stack size defaults for QNX's thread model

---

## Verification

```bash
# On QNX target:
./micropython -c "
import sys, utime
print('Python:', sys.version)
print('Platform:', sys.platform)

# Test microsecond timing (uses QNX CLOCK_MONOTONIC)
t0 = utime.ticks_us()
utime.sleep_ms(10)
t1 = utime.ticks_us()
print('10ms sleep took:', utime.ticks_diff(t1, t0), 'us')

# Test SQLite
import sqlite3
conn = sqlite3.connect(':memory:')
conn.execute('CREATE TABLE t (v INTEGER)')
conn.execute('INSERT INTO t VALUES (42)')
result = conn.execute('SELECT v FROM t').fetchone()
print('SQLite test:', result[0])  # Should print: 42
conn.close()
print('All tests passed!')
"
```

Expected output:
```
Python: 3.4.0 (MicroPython v1.24.0)
Platform: qnx
10ms sleep took: 10001 us
SQLite test: 42
All tests passed!
```
