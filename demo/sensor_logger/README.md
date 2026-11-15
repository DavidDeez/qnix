# QNX Vehicle Sensor Data Logger — Demo Guide

This directory contains the end-to-end demonstration application for the **SQLite + MicroPython on QNX 8.0** port.

## Overview

The demo simulates an automotive telemetry logger running on QNX SDP 8.0:
- Samples 4 synthetic vehicle sensors (`speed_kmh`, `rpm`, `temp_c`, `batt_v`) every **100 ms**
- Uses QNX's high-resolution monotonic clock (`CLOCK_MONOTONIC` via `utime.ticks_us()`) for microsecond timestamp accuracy and deterministic loop pacing
- Persists all readings into a local SQLite 3.47.0 database (`/tmp/qnx_sensor_log.db`)
- Periodically executes SQL aggregation queries (`MIN`, `MAX`, `AVG`, `COUNT`) to display live rolling telemetry statistics

## Files

- [`main.py`](main.py) — The MicroPython sensor logger application (compatible with both MicroPython on QNX and standard Python 3 for local testing)
- [`schema.sql`](schema.sql) — Reference SQLite schema and analytical view (`stats_per_second`)
- [`../run_demo.sh`](../run_demo.sh) — Automated deployment and execution script for QNX targets (QEMU x86_64 or Raspberry Pi 5 `aarch64le`)

## Running on QNX 8.0 (QEMU or Raspberry Pi 5)

From your Linux build host (after building `ports/sqlite` and `ports/micropython`):

```bash
source ~/qnx800/qnxsdp-env.sh
TARGET_HOST=<qnx-target-ip> CPU=aarch64le ./demo/run_demo.sh
```

Or directly on the QNX target shell:

```bash
export LD_LIBRARY_PATH=/data/home/qnxuser/qnix-demo/lib:$LD_LIBRARY_PATH
/data/home/qnxuser/qnix-demo/micropython /data/home/qnxuser/qnix-demo/demo/main.py
```

## Inspecting the Logged Database on QNX

After the demo finishes, inspect the persisted database using the ported `sqlite3` CLI shell:

```bash
/data/home/qnxuser/qnix-demo/sqlite3 /tmp/qnx_sensor_log.db < schema.sql
/data/home/qnxuser/qnix-demo/sqlite3 -header -column /tmp/qnx_sensor_log.db "SELECT * FROM stats_per_second LIMIT 10;"
```
