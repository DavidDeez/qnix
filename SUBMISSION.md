# SQLite + MicroPython for QNX 8.0

## Project Description (Hackathon Submission)

### What it does

This project ports two foundational open-source libraries to QNX 8.0 (SDP 8.0):

**SQLite 3.47.0** — The world's most widely deployed database engine. Available as a shared library (`libsqlite3.so`), static library, and interactive shell (`sqlite3`). Used in billions of devices from smartphones to aircraft black boxes.

**MicroPython 1.24.0** — A lean, efficient Python 3 runtime for embedded and real-time systems. Adapted from the unix port for QNX 8.0, with SQLite support compiled in, enabling Python scripting directly on the RTOS.

**Demo — Vehicle Sensor Data Logger**: A MicroPython application running on QNX 8.0 (QEMU or Raspberry Pi 5) that simulates automotive telemetry. It samples four vehicle sensors at 100ms intervals, stores every reading in a SQLite database on the QNX filesystem, and runs live SQL aggregation queries to show real-time statistics.

---

### How we built it

SQLite is released as a single C file (the "amalgamation"). We wrote a QNX cross-build Makefile that compiles it with `ntoaarch64-gcc` / `ntox86_64-gcc`, enabling key compile-time features (`SQLITE_THREADSAFE`, `SQLITE_ENABLE_FTS5`, `SQLITE_ENABLE_JSON1`) and linking against QNX's native pthread and POSIX libraries.

MicroPython's unix port is designed for POSIX systems, and QNX 8.0 is fully POSIX.1-2017 compliant. We added two patches:
- `0001-add-qnx-platform.patch`: Adds `__QNX__` and `_QNX_SOURCE` defines to `mpconfigport.h`, disables Linux-specific networking extensions not present on QNX, and sets `MICROPY_NLR_SETJMP=1` for safe non-local returns across all QNX CPU targets.
- `0002-qnx-thread-support.patch`: Adapts the `_thread` module for QNX pthreads, using `SCHED_RR` scheduling and a 256KB default stack size appropriate for QNX's thread model.

Both ports follow the [qnx-ports/build-files](https://github.com/qnx-ports/build-files) conventions for consistency with the QNX open-source community.

---

### Challenges

- **MicroPython's NLR (Non-Local Return)**: MicroPython uses aggressive setjmp/longjmp patterns for exception handling. Getting this to work reliably across QNX's x86_64 and aarch64le toolchains required explicitly setting `MICROPY_NLR_SETJMP=1`.
- **Linux-specific headers**: MicroPython's unix port uses `<linux/if_packet.h>` for `AF_PACKET` sockets, which don't exist on QNX. The patch correctly gates these behind `#ifndef __QNX__`.
- **Thread priorities**: QNX's thread priority model (1-255 for SCHED_FIFO) differs from Linux's; `_thread` module defaults were adjusted accordingly.
- **SQLite's VFS layer**: SQLite uses a pluggable Virtual File System. QNX's POSIX-compatible libc provides all required system calls (`pread`, `pwrite`, `flock`, `fsync`), so the default unix VFS works without modification.

---

### What we learned

QNX's POSIX compliance is excellent — most POSIX-compatible C code ports with minimal changes. The main friction points are Linux-specific extensions (AF_PACKET, eventfd, timerfd) that QNX does not implement, and subtle differences in thread scheduling defaults.

The MicroPython unix port is remarkably portable. With just 2 small patches (~80 lines of diff total), we have a full Python 3 interpreter running on a production RTOS. This opens the door to rapid prototyping, automated testing, and scripted configuration of QNX-based embedded systems.

---

### Accomplishments

- Successfully cross-compiled SQLite 3.47.0 for QNX 8.0 (aarch64le + x86_64)
- Successfully cross-compiled MicroPython 1.24.0 for QNX 8.0 with SQLite integration
- Demonstrated both running live on QNX SDP 8.0 in QEMU
- Wrote comprehensive build documentation following qnx-ports conventions
- Opened upstream PR to [micropython/micropython](https://github.com/micropython/micropython) adding QNX as a supported platform

---

### What's next

- Publish SQLite build files to [qnx-ports/build-files](https://github.com/qnx-ports/build-files) as a community contribution
- Extend the MicroPython sqlite3 module with QNX-specific filesystem path helpers
- Port Python's standard `sqlite3` test suite to run on QNX
- Explore porting CPython 3.12's `sqlite3` module as a follow-on project

---

### Built with

- QNX SDP 8.0 (QNX Everywhere)
- SQLite 3.47.0 (amalgamation)
- MicroPython 1.24.0
- QEMU (x86_64 target)
- Raspberry Pi 5 (aarch64le target)
- Ubuntu 22.04 (build host)
- GNU Make + QNX cross-compiler toolchain
