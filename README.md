# SQLite + MicroPython for QNX 8.0

[![SQLite Port](https://img.shields.io/badge/SQLite-3.47.0-blue?logo=sqlite)](https://sqlite.org)
[![MicroPython Port](https://img.shields.io/badge/MicroPython-1.24-green?logo=python)](https://micropython.org)
[![QNX](https://img.shields.io/badge/QNX-SDP%208.0-black)](https://blackberry.qnx.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

> **QNX Global Hackathon 2026 — Open-Source Contribution & Ports**

This project ports two foundational open-source libraries to **QNX 8.0 (SDP 8.0)**:

1. **[SQLite](https://sqlite.org)** — The world's most widely deployed database engine. Used in billions of devices from web browsers to aircraft. Ported as both a shared library (`libsqlite3.so`) and a command-line shell (`sqlite3`).

2. **[MicroPython](https://micropython.org)** — A lean, efficient Python 3 implementation designed for embedded and real-time systems. Ported via the unix platform target, adapted for QNX's POSIX environment, with SQLite support compiled in.

Together they enable **scriptable, persistent data storage** on a real-time operating system — a capability gap that matters deeply in automotive, medical, and robotics systems where QNX is deployed.

---

## Why This Matters

QNX runs in 275 million vehicles, medical devices, industrial robots, and factory floors. Yet until now, developers working on QNX had no access to:

- A **production-grade embedded database** for persistent storage of sensor data, configuration, and event logs
- A **Python scripting runtime** for rapid prototyping, test automation, and data processing on the target

SQLite's zero-configuration, serverless design is a perfect fit for QNX's process-isolated architecture. MicroPython's small footprint and RTOS-friendly threading model make it an ideal scripting layer for real-time systems.

---

## Demo: Real-Time Vehicle Sensor Data Logger

The included demo runs on QNX 8.0 (QEMU or Raspberry Pi 5) and simulates an automotive sensor logging system:

- Generates synthetic vehicle telemetry: speed (km/h), engine RPM, coolant temperature, battery voltage
- Logs data at 100ms intervals using QNX real-time scheduling
- Stores all readings persistently in a SQLite database on the QNX filesystem
- Provides live query output: last 5 readings, min/max/avg stats per sensor

```
[QNX SDP 8.0 | QEMU x86_64]
=== QNX Vehicle Sensor Data Logger ===
SQLite 3.47.0 | MicroPython 1.24.0

[00:00.100] speed=87.3 km/h  rpm=2341  temp=91.2 C  batt=13.8V
[00:00.200] speed=88.1 km/h  rpm=2389  temp=91.4 C  batt=13.7V
[00:00.300] speed=86.9 km/h  rpm=2298  temp=91.3 C  batt=13.9V
...

--- Live Stats (last 60s) ---
speed:  min=72.1  max=103.4  avg=88.7 km/h
rpm:    min=1980  max=3200   avg=2340
temp:   min=88.0  max=94.1   avg=91.1 C
batt:   min=13.4  max=14.1   avg=13.8 V
```

---

## Repository Structure

```
.
├── README.md                     ← This file
├── LICENSE                       ← MIT License
│
├── ports/
│   ├── sqlite/                   ← SQLite 3.47.0 QNX port
│   │   ├── Makefile              ← QNX cross-build Makefile
│   │   ├── common.mk             ← Shared build variables
│   │   ├── README.md             ← Build & run instructions
│   │   └── patches/              ← QNX-specific patches
│   │
│   └── micropython/              ← MicroPython 1.24 QNX port
│       ├── Makefile              ← QNX cross-build Makefile
│       ├── README.md             ← Build & run instructions
│       ├── cmodules/
│       │   ├── micropython.mk    ← Master C modules manifest
│       │   ├── sqlite3/          ← Native C `sqlite3` user module for MicroPython
│       │   │   ├── modsqlite3.c  ← Wraps libsqlite3 (connect, execute, Cursor, fetch*)
│       │   │   └── micropython.mk
│       │   └── qnx_rt/           ← Native C `qnx_rt` module (<sys/neutrino.h>, <sys/syspage.h>)
│       │       ├── modqnxrt.c    ← Exposes ClockCycles(), ClockPeriod(), thread priority
│       │       └── micropython.mk
│       └── patches/              ← QNX platform patches
│           ├── 0001-add-qnx-platform.patch
│           ├── 0002-qnx-thread-support.patch
│           └── 0003-qnx-filesystem.patch
│
└── demo/
    ├── sensor_logger/
    │   ├── main.py               ← MicroPython demo app
    │   └── schema.sql            ← SQLite database schema
    └── run_demo.sh               ← End-to-end demo launcher
```

---

## Upstream Projects

| Project | Upstream URL | License |
|---------|-------------|---------|
| SQLite | https://github.com/sqlite/sqlite | Public Domain |
| MicroPython | https://github.com/micropython/micropython | MIT |

---

## Quick Start

### Prerequisites

- QNX SDP 8.0 (free via [QNX Everywhere](https://www.qnx.com/getqnx))
- Ubuntu 22.04 Linux host (or use the Docker method below)
- QEMU or Raspberry Pi 5 as target

### 1. Set Up QNX Environment

```bash
source ~/qnx800/qnxsdp-env.sh
```

### 2. Build SQLite for QNX

```bash
# Build and install SQLite for all QNX targets
make -C ports/sqlite install -j$(nproc)
```

### 3. Build MicroPython for QNX

```bash
# Build MicroPython (links against libsqlite3)
make -C ports/micropython install -j$(nproc)
```

### 4. Run the Demo on QEMU

```bash
# Transfer binaries to QEMU target and launch demo
./demo/run_demo.sh
```

Full build and run instructions are in each port's `README.md`.

---

## Docker Build (Recommended)

```bash
# Clone this repo
git clone https://github.com/DavidDeez/qnix.git
cd qnix

# Build Docker image (from qnx-ports/build-files docker setup)
# See https://github.com/qnx-ports/build-files/tree/main/docker
./docker/docker-build-qnx-image.sh
./docker/docker-create-container.sh

# Inside container: build everything
source ~/qnx800/qnxsdp-env.sh
make -C ports/sqlite install -j$(nproc)
make -C ports/micropython install -j$(nproc)
```

---

## QNX-Specific Evidence

This port demonstrates QNX integration through:

- **QNX cross-compiler toolchain** (`qcc`, `ntoaarch64-gcc`, `ntox86_64-gcc`) used in all Makefiles
- **QNX SDP 8.0 headers** (`$QNX_TARGET/usr/include`) and libraries (`$QNX_TARGET/aarch64le/usr/lib`)
- **QNX build system** conventions matching [qnx-ports/build-files](https://github.com/qnx-ports/build-files)
- **QNX filesystem paths** (`/data/`, `/tmp/`, `/proc/boot/`) used in demo
- **QNX real-time clock** (`clock_gettime(CLOCK_MONOTONIC)`) for microsecond timestamps
- **Demo video** showing both ports running on QNX SDP 8.0 in QEMU

---

## Hackathon Submission

- **Category**: Open-Source Contribution & Ports
- **QNX Version**: SDP 8.0 (QNX Everywhere)
- **Target**: QEMU x86_64 and Raspberry Pi 5 (aarch64le)
- **Toolchain**: QNX SDP 8.0 (`qcc` / `ntoaarch64-gcc`)
- **Upstream SQLite**: https://github.com/sqlite/sqlite
- **Upstream MicroPython**: https://github.com/micropython/micropython

---

## License

MIT — see [LICENSE](LICENSE). Upstream projects retain their original licenses
(SQLite: Public Domain, MicroPython: MIT).
