"""
QNX Vehicle Sensor Data Logger
================================
A demonstration of SQLite + MicroPython running on QNX SDP 8.0.

This script simulates an automotive sensor data logging system:
- Generates synthetic vehicle telemetry at 100ms intervals
- Stores all readings persistently in a SQLite database
- Demonstrates QNX real-time scheduling via high-precision timing
- Provides live query output: stats per sensor over a rolling window

Sensors simulated:
  - Vehicle speed (km/h)
  - Engine RPM
  - Coolant temperature (degrees C)
  - Battery voltage (V)

Target: QNX SDP 8.0 on QEMU x86_64 or Raspberry Pi 5 (aarch64le)
Author: QNiX Hackathon 2026
"""

import sqlite3
import sys
import math
import os

try:
    import utime
    import urandom
except ImportError:
    # CPython fallback for local host testing / dry-run verification
    import time as _time
    import random as _random

    class utime:
        @staticmethod
        def ticks_us():
            return int(_time.monotonic() * 1_000_000)

        @staticmethod
        def ticks_diff(t1, t0):
            return t1 - t0

        @staticmethod
        def sleep_ms(ms):
            _time.sleep(ms / 1000.0)

    class urandom:
        @staticmethod
        def getrandbits(k):
            return _random.getrandbits(k)

try:
    import qnx_rt
except ImportError:
    # Fallback when running outside QNX MicroPython (e.g. host dry-run)
    import time as _rt_time

    class qnx_rt:
        SCHED_FIFO = 1
        SCHED_RR = 2
        SCHED_OTHER = 3

        @staticmethod
        def clock_cycles():
            return int(_rt_time.perf_counter_ns())

        @staticmethod
        def cycles_per_sec():
            return 1_000_000_000

        @staticmethod
        def get_priority():
            return 10

        @staticmethod
        def clock_period_ns():
            return 1_000_000

        @staticmethod
        def sysinfo():
            return {
                "pid": os.getpid(),
                "tid": 1,
                "priority": 10,
                "policy": "SCHED_RR",
                "cycles_per_sec": 1_000_000_000,
                "clock_cycles": int(_rt_time.perf_counter_ns()),
            }

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
DB_PATH        = os.environ.get("QNX_DB_PATH", "/tmp/qnx_sensor_log.db" if os.path.isdir("/tmp") else "qnx_sensor_log.db")
LOG_INTERVAL_MS = int(os.environ.get("QNX_LOG_INTERVAL_MS", "100"))   # Sample every 100 ms
STATS_INTERVAL  = int(os.environ.get("QNX_STATS_INTERVAL", "50"))     # Print stats every 50 samples (5 seconds)
MAX_RECORDS     = 10000                                               # Rotate DB after this many records
DEMO_RUNTIME_S  = float(os.environ.get("QNX_DEMO_RUNTIME_S", "30"))   # Run for 30 seconds then show summary

# ---------------------------------------------------------------------------
# Sensor simulation state (models a realistic drive cycle)
# ---------------------------------------------------------------------------
class VehicleSimulator:
    """
    Simple physics-based vehicle simulator.
    Produces realistic speed, RPM, temperature, and battery voltage
    readings that change smoothly over time — as a real car would.
    """
    def __init__(self):
        self.speed      = 60.0   # km/h
        self.rpm        = 2000   # engine RPM
        self.temp       = 90.0   # coolant temp degC
        self.batt       = 13.8   # battery voltage V
        self.t          = 0      # time counter
        self._accel     = 0.0

    def tick(self):
        """Advance simulation by one time step (100ms)."""
        self.t += 0.1

        # Speed: sinusoidal variation around 80 km/h, plus random noise
        target_speed = 80.0 + 25.0 * math.sin(self.t * 0.08) + self._noise(5.0)
        self.speed += 0.1 * (target_speed - self.speed)
        self.speed  = max(0.0, min(140.0, self.speed))

        # RPM: proportional to speed with gear simulation
        gear_ratio = 1.0 + (self.speed / 140.0) * 2.5
        self.rpm   = int(self.speed * gear_ratio * 20 + self._noise(100))
        self.rpm   = max(700, min(6500, self.rpm))

        # Coolant temp: warms up, then thermostat holds ~90-95C
        if self.temp < 90.0:
            self.temp += 0.05
        else:
            self.temp = 91.5 + 2.5 * math.sin(self.t * 0.03) + self._noise(0.3)

        # Battery: slight variation with load
        self.batt = 13.8 + 0.3 * math.sin(self.t * 0.15) + self._noise(0.05)

        return self.snapshot()

    def snapshot(self):
        return (self.speed, self.rpm, self.temp, self.batt)

    def _noise(self, amplitude):
        """Return small random noise in [-amplitude, +amplitude]."""
        return amplitude * (urandom.getrandbits(16) / 32768.0 - 1.0)


# ---------------------------------------------------------------------------
# Database layer
# ---------------------------------------------------------------------------
def init_db(conn):
    """Create sensor log table if it doesn't exist."""
    conn.execute("""
        CREATE TABLE IF NOT EXISTS sensor_log (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp_us  INTEGER NOT NULL,
            qnx_cycles    INTEGER NOT NULL DEFAULT 0,
            jitter_us     INTEGER NOT NULL DEFAULT 0,
            speed_kmh     REAL    NOT NULL,
            rpm           INTEGER NOT NULL,
            temp_c        REAL    NOT NULL,
            batt_v        REAL    NOT NULL
        )
    """)
    conn.execute("""
        CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_log (timestamp_us)
    """)
    conn.commit()
    print("[DB] Initialised SQLite database: {}".format(DB_PATH))
    print("[DB] SQLite version: {}".format(sqlite3.sqlite_version))


def insert_reading(conn, ts_us, qnx_cycles, jitter_us, speed, rpm, temp, batt):
    """Insert one sensor reading with QNX hardware cycle count and loop jitter."""
    conn.execute(
        "INSERT INTO sensor_log (timestamp_us, qnx_cycles, jitter_us, speed_kmh, rpm, temp_c, batt_v) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)",
        (ts_us, qnx_cycles, jitter_us, speed, rpm, temp, batt)
    )
    # Note: we commit in batches (every STATS_INTERVAL rows) for performance


def get_stats(conn, last_n=500):
    """
    Return min/max/avg for each sensor over the last N records.
    Demonstrates SQLite's aggregation functions on a QNX real-time target.
    """
    row = conn.execute("""
        SELECT
            MIN(speed_kmh), MAX(speed_kmh), AVG(speed_kmh),
            MIN(rpm),       MAX(rpm),       AVG(rpm),
            MIN(temp_c),    MAX(temp_c),    AVG(temp_c),
            MIN(batt_v),    MAX(batt_v),    AVG(batt_v),
            COUNT(*)
        FROM (
            SELECT speed_kmh, rpm, temp_c, batt_v
            FROM sensor_log
            ORDER BY id DESC
            LIMIT ?
        )
    """, (last_n,)).fetchone()
    return row


def rotate_if_needed(conn):
    """Prune oldest records to prevent unbounded database growth."""
    count = conn.execute("SELECT COUNT(*) FROM sensor_log").fetchone()[0]
    if count > MAX_RECORDS:
        conn.execute("""
            DELETE FROM sensor_log WHERE id IN (
                SELECT id FROM sensor_log ORDER BY id ASC LIMIT ?
            )
        """, (count - MAX_RECORDS + 100,))
        conn.commit()


# ---------------------------------------------------------------------------
# Display helpers
# ---------------------------------------------------------------------------
def banner():
    info = qnx_rt.sysinfo()
    print()
    print("=" * 64)
    print("  QNX Vehicle Sensor Data Logger")
    print("  SQLite + MicroPython + qnx_rt (<sys/neutrino.h>) on QNX 8.0")
    print("=" * 64)
    print("  Platform      : {}".format(sys.platform))
    print("  Python        : {}".format(sys.version))
    print("  QNX Thread    : PID={} TID={} Policy={} Priority={}".format(
        info["pid"], info["tid"], info["policy"], info["priority"]
    ))
    print("  ClockCycles   : {} Hz (Kernel Tick: {} ns)".format(
        info["cycles_per_sec"], qnx_rt.clock_period_ns()
    ))
    print("  Database      : {}".format(DB_PATH))
    print("  Interval      : {} ms".format(LOG_INTERVAL_MS))
    print("=" * 64)
    print()


def print_reading(elapsed_ms, jitter_us, speed, rpm, temp, batt):
    """Print a single sensor reading line with QNX real-time scheduling jitter."""
    minutes = (elapsed_ms // 60000)
    seconds = (elapsed_ms % 60000) // 1000
    millis  = elapsed_ms % 1000
    print("[{:02d}:{:02d}.{:03d} | jitter={:+4d}us] speed={:5.1f} km/h  rpm={:4d}  temp={:4.1f}C  batt={:.2f}V".format(
        minutes, seconds, millis, jitter_us,
        speed, rpm, temp, batt
    ))


def print_stats(stats, sample_count):
    """Print a summary statistics block."""
    if stats is None or stats[12] == 0:
        return
    sp_min, sp_max, sp_avg = stats[0],  stats[1],  stats[2]
    rp_min, rp_max, rp_avg = stats[3],  stats[4],  stats[5]
    tm_min, tm_max, tm_avg = stats[6],  stats[7],  stats[8]
    bt_min, bt_max, bt_avg = stats[9],  stats[10], stats[11]
    n = stats[12]

    print()
    print("--- Live Stats (last {} samples from SQLite) ---".format(n))
    print("  speed:  min={:5.1f}  max={:5.1f}  avg={:5.1f} km/h".format(sp_min, sp_max, sp_avg))
    print("  rpm:    min={:4d}    max={:4d}    avg={:5.0f}".format(int(rp_min), int(rp_max), rp_avg))
    print("  temp:   min={:.1f}    max={:.1f}    avg={:.1f} C".format(tm_min, tm_max, tm_avg))
    print("  batt:   min={:.2f}    max={:.2f}    avg={:.2f} V".format(bt_min, bt_max, bt_avg))
    print()


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def main():
    banner()

    # Open SQLite database (created on QNX filesystem at /tmp/)
    conn = sqlite3.connect(DB_PATH)
    init_db(conn)

    sim = VehicleSimulator()
    sample_count = 0
    start_us = utime.ticks_us()
    expected_us = start_us

    print("[LOG] Starting sensor logging. Press Ctrl+C to stop.\n")

    try:
        while True:
            # --- Real-time timing: use QNX CLOCK_MONOTONIC + ClockCycles() ---
            tick_start = utime.ticks_us()
            hw_cycles  = qnx_rt.clock_cycles()
            elapsed_ms = utime.ticks_diff(tick_start, start_us) // 1000
            jitter_us  = utime.ticks_diff(tick_start, expected_us)

            # Stop after DEMO_RUNTIME_S seconds
            if elapsed_ms >= DEMO_RUNTIME_S * 1000:
                break

            # --- Simulate sensors ---
            speed, rpm, temp, batt = sim.tick()

            # --- Store in SQLite ---
            insert_reading(conn, tick_start, hw_cycles, jitter_us, speed, rpm, temp, batt)
            sample_count += 1

            # --- Print reading every sample ---
            print_reading(elapsed_ms, jitter_us, speed, rpm, temp, batt)

            # --- Commit + stats every STATS_INTERVAL samples ---
            if sample_count % STATS_INTERVAL == 0:
                conn.commit()
                rotate_if_needed(conn)
                stats = get_stats(conn, last_n=STATS_INTERVAL)
                print_stats(stats, sample_count)

            # --- Sleep until next interval (real-time scheduling) ---
            expected_us += LOG_INTERVAL_MS * 1000
            tick_end = utime.ticks_us()
            sleep_ms = utime.ticks_diff(expected_us, tick_end) // 1000
            if sleep_ms > 0:
                utime.sleep_ms(sleep_ms)

    except KeyboardInterrupt:
        print("\n[LOG] Interrupted by user.")

    # --- Final commit and summary ---
    conn.commit()

    total_records = conn.execute("SELECT COUNT(*) FROM sensor_log").fetchone()[0]
    final_stats = get_stats(conn, last_n=total_records)

    end_us = utime.ticks_us()
    total_ms = utime.ticks_diff(end_us, start_us) // 1000

    print("\n" + "=" * 60)
    print("  FINAL SUMMARY")
    print("=" * 60)
    print("  Runtime:         {:.1f} seconds".format(total_ms / 1000.0))
    print("  Samples logged:  {}".format(sample_count))
    print("  Avg sample rate: {:.1f} Hz".format(sample_count / (total_ms / 1000.0)))
    print("  DB records:      {}".format(total_records))
    print("  DB path:         {}".format(DB_PATH))
    print_stats(final_stats, total_records)

    conn.close()
    print("[DB] Database closed. Data persisted on QNX filesystem.")
    print("\nTo inspect the database on QNX:")
    print("  sqlite3 {} 'SELECT * FROM sensor_log LIMIT 10;'".format(DB_PATH))


if __name__ == "__main__":
    main()
