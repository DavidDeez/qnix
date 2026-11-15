#!/usr/bin/env bash
# =============================================================================
# QNX Hackathon — End-to-End Demo Runner
# SQLite + MicroPython on QNX SDP 8.0
#
# This script transfers the built binaries to a QNX target (QEMU or RPi5),
# deploys the demo app, and runs the sensor logger.
#
# Usage:
#   ./demo/run_demo.sh [TARGET_HOST]
#
# If TARGET_HOST is not given, defaults to localhost (assumes QEMU is running
# with SSH port-forwarded to localhost:2222 or TARGET_HOST set via env).
#
# Prerequisites:
#   - QNX SDP 8.0 sourced (source ~/qnx800/qnxsdp-env.sh)
#   - SQLite and MicroPython already built:
#       make -C ports/sqlite install
#       make -C ports/micropython install
#   - QEMU running QNX SDP 8.0 image, OR Raspberry Pi 5 with QNX
#   - SSH access to target as 'qnxuser'
# =============================================================================

set -euo pipefail

# --- Configuration ---
TARGET_HOST="${TARGET_HOST:-localhost}"
TARGET_USER="${TARGET_USER:-qnxuser}"
SSH_PORT="${SSH_PORT:-22}"
CPU="${CPU:-aarch64le}"

# QEMU typically uses port 2222 for SSH port-forwarding
if [[ "$TARGET_HOST" == "localhost" ]]; then
    SSH_PORT="${SSH_PORT:-2222}"
fi

TARGET_DIR="/data/home/qnxuser/qnix-demo"
DEMO_SCRIPT="demo/sensor_logger/main.py"

SSH_OPTS="-p ${SSH_PORT} -o StrictHostKeyChecking=no -o ConnectTimeout=10"

# --- Validate QNX environment ---
if [[ -z "${QNX_TARGET:-}" ]]; then
    echo "[ERROR] QNX_TARGET is not set."
    echo "        Please run: source ~/qnx800/qnxsdp-env.sh"
    exit 1
fi

SYSROOT="${QNX_TARGET}/${CPU}"
BIN_DIR="${SYSROOT}/usr/local/bin"
LIB_DIR="${SYSROOT}/usr/local/lib"

echo "============================================================"
echo "  QNX Vehicle Sensor Logger — Demo Runner"
echo "============================================================"
echo "  Target   : ${TARGET_USER}@${TARGET_HOST}:${SSH_PORT}"
echo "  CPU      : ${CPU}"
echo "  Sysroot  : ${SYSROOT}"
echo "============================================================"
echo ""

# --- Check binaries exist ---
if [[ ! -f "${BIN_DIR}/sqlite3" ]]; then
    echo "[ERROR] sqlite3 not found at ${BIN_DIR}/sqlite3"
    echo "        Run: make -C ports/sqlite install CPU=${CPU}"
    exit 1
fi

if [[ ! -f "${BIN_DIR}/micropython" ]]; then
    echo "[ERROR] micropython not found at ${BIN_DIR}/micropython"
    echo "        Run: make -C ports/micropython install CPU=${CPU}"
    exit 1
fi

# --- Create target directories ---
echo "[STEP 1] Setting up target directories..."
ssh ${SSH_OPTS} "${TARGET_USER}@${TARGET_HOST}" \
    "mkdir -p ${TARGET_DIR}/lib ${TARGET_DIR}/demo"

# --- Transfer libraries ---
echo "[STEP 2] Transferring libsqlite3..."
scp -P "${SSH_PORT}" \
    "${LIB_DIR}/libsqlite3.so.0.8.6" \
    "${TARGET_USER}@${TARGET_HOST}:${TARGET_DIR}/lib/"

ssh ${SSH_OPTS} "${TARGET_USER}@${TARGET_HOST}" \
    "cd ${TARGET_DIR}/lib && \
     ln -sf libsqlite3.so.0.8.6 libsqlite3.so.0 && \
     ln -sf libsqlite3.so.0 libsqlite3.so"

# --- Transfer binaries ---
echo "[STEP 3] Transferring sqlite3 and micropython binaries..."
scp -P "${SSH_PORT}" \
    "${BIN_DIR}/sqlite3" \
    "${BIN_DIR}/micropython" \
    "${TARGET_USER}@${TARGET_HOST}:${TARGET_DIR}/"

# --- Transfer demo script ---
echo "[STEP 4] Transferring demo application..."
scp -P "${SSH_PORT}" \
    "${DEMO_SCRIPT}" \
    "${TARGET_USER}@${TARGET_HOST}:${TARGET_DIR}/demo/"

# --- Run the demo ---
echo ""
echo "[STEP 5] Running sensor logger demo on QNX target..."
echo "------------------------------------------------------------"

ssh ${SSH_OPTS} "${TARGET_USER}@${TARGET_HOST}" bash <<EOF
    export LD_LIBRARY_PATH="${TARGET_DIR}/lib:\$LD_LIBRARY_PATH"
    export PATH="${TARGET_DIR}:\$PATH"

    # Quick sanity checks
    echo "[QNX] uname: \$(uname -a)"
    echo "[QNX] sqlite3 version: \$(${TARGET_DIR}/sqlite3 :memory: 'SELECT sqlite_version();')"
    echo "[QNX] micropython version:"
    ${TARGET_DIR}/micropython -c "import sys; print('  ', sys.version)"
    echo ""

    # Run the sensor logger demo
    ${TARGET_DIR}/micropython ${TARGET_DIR}/demo/main.py
EOF

echo ""
echo "============================================================"
echo "  Demo complete!"
echo "  The SQLite database was created at /tmp/qnx_sensor_log.db"
echo "  on the QNX target."
echo ""
echo "  To inspect the data:"
echo "    ssh -p ${SSH_PORT} ${TARGET_USER}@${TARGET_HOST}"
echo "    ./qnix-demo/sqlite3 /tmp/qnx_sensor_log.db"
echo "    sqlite> SELECT * FROM stats_per_second;"
echo "============================================================"
