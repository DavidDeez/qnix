"""
Automated Test & Verification Suite for QNX SQLite + MicroPython Port
=====================================================================
Validates:
1. Repository structure & required Devpost files (LICENSE, README, Makefiles, patches)
2. QNX cross-build Makefile flags (__QNX__, _QNX_SOURCE, sysroot, libsqlite3 linking)
3. MicroPython QNX patch headers and unified diff structure
4. End-to-end execution of the Vehicle Sensor Data Logger & SQLite schema views
"""

import os
import sqlite3
import subprocess
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent


class TestQnxPortRepository(unittest.TestCase):
    def test_required_files_exist(self):
        required = [
            "README.md",
            "LICENSE",
            "SUBMISSION.md",
            "ports/sqlite/Makefile",
            "ports/sqlite/common.mk",
            "ports/sqlite/README.md",
            "ports/micropython/Makefile",
            "ports/micropython/README.md",
            "ports/micropython/cmodules/sqlite3/modsqlite3.c",
            "ports/micropython/cmodules/sqlite3/micropython.mk",
            "ports/micropython/patches/0001-add-qnx-platform.patch",
            "ports/micropython/patches/0002-qnx-thread-support.patch",
            "ports/micropython/patches/0003-qnx-filesystem.patch",
            "demo/sensor_logger/main.py",
            "demo/sensor_logger/schema.sql",
            "demo/run_demo.sh",
        ]
        for rel_path in required:
            full_path = REPO_ROOT / rel_path
            self.assertTrue(full_path.exists(), f"Missing required file: {rel_path}")

    def test_sqlite_makefile_qnx_flags(self):
        makefile = (REPO_ROOT / "ports/sqlite/Makefile").read_text(encoding="utf-8")
        self.assertIn("aarch64-unknown-nto-qnx8.0.0-gcc", makefile)
        self.assertIn("x86_64-pc-nto-qnx8.0.0-gcc", makefile)
        self.assertIn("-D_QNX_SOURCE", makefile)
        self.assertIn("-DSQLITE_THREADSAFE=1", makefile)
        self.assertIn("-DSQLITE_ENABLE_FTS5=1", makefile)
        self.assertIn("libsqlite3.so", makefile)

    def test_micropython_makefile_and_patches(self):
        makefile = (REPO_ROOT / "ports/micropython/Makefile").read_text(encoding="utf-8")
        self.assertIn("-D__QNX__", makefile)
        self.assertIn("-D_QNX_SOURCE", makefile)
        self.assertIn("-lsqlite3", makefile)
        self.assertIn("-lsocket", makefile)
        self.assertIn("USER_C_MODULES", makefile)

        modsqlite3_c = (REPO_ROOT / "ports/micropython/cmodules/sqlite3/modsqlite3.c").read_text(encoding="utf-8")
        self.assertIn("MP_REGISTER_MODULE(MP_QSTR_sqlite3, mp_module_sqlite3);", modsqlite3_c)
        self.assertIn("sqlite3_open_v2", modsqlite3_c)
        self.assertIn("sqlite3_prepare_v2", modsqlite3_c)
        self.assertIn("sqlite3_step", modsqlite3_c)

        patch1 = (REPO_ROOT / "ports/micropython/patches/0001-add-qnx-platform.patch").read_text(encoding="utf-8")
        self.assertIn('#define MICROPY_PLATFORM_STR "qnx"', patch1)
        self.assertIn("#define MICROPY_NLR_SETJMP   (1)", patch1)

        patch2 = (REPO_ROOT / "ports/micropython/patches/0002-qnx-thread-support.patch").read_text(encoding="utf-8")
        self.assertIn("pthread_attr_setschedpolicy(&attr, SCHED_RR);", patch2)

    def test_sensor_logger_and_sqlite_schema(self):
        db_path = REPO_ROOT / "demo" / "verify_test.db"
        if db_path.exists():
            db_path.unlink()

        env = os.environ.copy()
        env["QNX_DB_PATH"] = str(db_path)
        env["QNX_DEMO_RUNTIME_S"] = "1.1"
        env["QNX_STATS_INTERVAL"] = "5"

        try:
            proc = subprocess.run(
                [sys.executable, str(REPO_ROOT / "demo/sensor_logger/main.py")],
                env=env,
                capture_output=True,
                text=True,
                check=True,
            )
            self.assertIn("QNX Vehicle Sensor Data Logger", proc.stdout)
            self.assertIn("FINAL SUMMARY", proc.stdout)
            self.assertTrue(db_path.exists(), "Database file was not created by sensor logger")

            conn = sqlite3.connect(str(db_path))
            schema_sql = (REPO_ROOT / "demo/sensor_logger/schema.sql").read_text(encoding="utf-8")
            conn.executescript(schema_sql)

            count = conn.execute("SELECT COUNT(*) FROM sensor_log").fetchone()[0]
            self.assertGreaterEqual(count, 8, f"Expected >= 8 logged rows, got {count}")

            view_rows = conn.execute("SELECT * FROM stats_per_second").fetchall()
            self.assertGreaterEqual(len(view_rows), 1, "stats_per_second view returned no rows")
            conn.close()
        finally:
            if db_path.exists():
                db_path.unlink()


if __name__ == "__main__":
    unittest.main(verbosity=2)
