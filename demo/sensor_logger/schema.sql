-- QNX Vehicle Sensor Data Logger — SQLite Schema
-- SQLite 3.47.0 | QNX SDP 8.0
--
-- This schema is used by the MicroPython demo (main.py).
-- It is provided here for reference and for use with the sqlite3 CLI.

-- Main sensor log table
CREATE TABLE IF NOT EXISTS sensor_log (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    -- High-resolution timestamp in microseconds (from QNX CLOCK_MONOTONIC)
    timestamp_us INTEGER NOT NULL,
    -- 64-bit hardware cycle counter from QNX ClockCycles() (<sys/neutrino.h>)
    qnx_cycles   INTEGER NOT NULL DEFAULT 0,
    -- Real-time scheduling loop jitter in microseconds
    jitter_us    INTEGER NOT NULL DEFAULT 0,
    -- Vehicle speed in km/h
    speed_kmh    REAL    NOT NULL,
    -- Engine RPM
    rpm          INTEGER NOT NULL,
    -- Coolant temperature in degrees Celsius
    temp_c       REAL    NOT NULL,
    -- Battery voltage in Volts
    batt_v       REAL    NOT NULL
);

-- Index for efficient time-range queries
CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_log (timestamp_us);

-- Example view: aggregate stats per 1-second window
CREATE VIEW IF NOT EXISTS stats_per_second AS
SELECT
    (timestamp_us / 1000000) AS second,
    COUNT(*)                  AS sample_count,
    AVG(speed_kmh)            AS avg_speed_kmh,
    AVG(rpm)                  AS avg_rpm,
    AVG(temp_c)               AS avg_temp_c,
    AVG(batt_v)               AS avg_batt_v,
    MIN(speed_kmh)            AS min_speed,
    MAX(speed_kmh)            AS max_speed
FROM sensor_log
GROUP BY second
ORDER BY second;

-- -------------------------------------------------------------------------
-- Useful queries for exploring the logged data:
-- -------------------------------------------------------------------------
-- Show last 10 readings:
--   SELECT * FROM sensor_log ORDER BY id DESC LIMIT 10;
--
-- Show per-second averages:
--   SELECT * FROM stats_per_second;
--
-- Show overall stats:
--   SELECT
--     COUNT(*) as total_samples,
--     MIN(speed_kmh), MAX(speed_kmh), AVG(speed_kmh),
--     MIN(rpm), MAX(rpm), AVG(rpm),
--     MIN(temp_c), MAX(temp_c), AVG(temp_c),
--     MIN(batt_v), MAX(batt_v), AVG(batt_v)
--   FROM sensor_log;
--
-- Check SQLite version:
--   SELECT sqlite_version();
