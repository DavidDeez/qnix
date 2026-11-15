# SQLite Port for QNX 8.0

[![Build](https://img.shields.io/badge/QNX%20SDP-8.0-black)](https://blackberry.qnx.com)
[![SQLite Version](https://img.shields.io/badge/SQLite-3.47.0-blue)](https://sqlite.org/releaselog/3_47_0.html)
[![License](https://img.shields.io/badge/License-Public%20Domain-lightgrey)](https://sqlite.org/copyright.html)

**NOTE**: QNX ports are only supported from a **Linux host** operating system.

Upstream project: https://github.com/sqlite/sqlite

---

## What is SQLite?

SQLite is a C-language library that implements a small, fast, self-contained, high-reliability, full-featured SQL database engine. It is the most widely deployed database in the world — found in every smartphone, web browser, aircraft, and embedded system.

This port builds SQLite as:
- **`libsqlite3.so`** — shared library for linking with other QNX applications
- **`libsqlite3.a`** — static library
- **`sqlite3`** — interactive SQL shell binary

---

## QNX Targets Supported

| Target | Architecture | QNX Version |
|--------|-------------|-------------|
| QEMU x86_64 | x86_64 | SDP 8.0 |
| Raspberry Pi 5 | aarch64le | SDP 8.0 |

---

## Build Instructions

### Option 1: Docker (Recommended)

```bash
# Install Docker: https://docs.docker.com/engine/install/ubuntu/

# Create workspace
mkdir -p ~/qnx_workspace && cd ~/qnx_workspace
git clone https://github.com/<your-username>/qnix-hackathon.git

# Build Docker image (see qnx-ports/build-files docker/)
cd ~/qnx_workspace/qnix-hackathon
./docker/docker-build-qnx-image.sh
./docker/docker-create-container.sh

# Inside the Docker container:
source ~/qnx800/qnxsdp-env.sh
make -C ports/sqlite install -j$(nproc)
```

### Option 2: Native Linux Host (Ubuntu 22.04)

```bash
# Install prerequisites
sudo apt-get update
sudo apt-get install -y make wget unzip

# Source QNX SDP environment
source ~/qnx800/qnxsdp-env.sh

# Clone this repo
git clone https://github.com/<your-username>/qnix-hackathon.git
cd qnix-hackathon

# Build SQLite for all QNX targets
make -C ports/sqlite install -j$(nproc)

# Or build for a specific architecture:
make -C ports/sqlite install CPU=x86_64 -j$(nproc)
make -C ports/sqlite install CPU=aarch64le -j$(nproc)
```

---

## Build Output

After a successful build, the following files are installed under `$QNX_TARGET`:

```
$QNX_TARGET/
├── x86_64/
│   └── usr/local/
│       ├── bin/sqlite3
│       ├── include/sqlite3.h
│       ├── lib/libsqlite3.so -> libsqlite3.so.0
│       ├── lib/libsqlite3.so.0 -> libsqlite3.so.0.8.6
│       ├── lib/libsqlite3.so.0.8.6
│       └── lib/libsqlite3.a
└── aarch64le/
    └── usr/local/
        ├── bin/sqlite3
        ├── include/sqlite3.h
        ├── lib/libsqlite3.so -> libsqlite3.so.0
        ├── lib/libsqlite3.so.0 -> libsqlite3.so.0.8.6
        ├── lib/libsqlite3.so.0.8.6
        └── lib/libsqlite3.a
```

---

## Running on QNX Target

### Transfer to Target

```bash
TARGET_HOST=<target-ip-or-hostname>

# Copy sqlite3 binary and shared library
scp $QNX_TARGET/aarch64le/usr/local/bin/sqlite3 qnxuser@$TARGET_HOST:/data/home/qnxuser/
scp $QNX_TARGET/aarch64le/usr/local/lib/libsqlite3.so.0.8.6 qnxuser@$TARGET_HOST:/data/home/qnxuser/lib/

# Or for QEMU x86_64:
scp $QNX_TARGET/x86_64/usr/local/bin/sqlite3 qnxuser@$TARGET_HOST:/data/home/qnxuser/
scp $QNX_TARGET/x86_64/usr/local/lib/libsqlite3.so.0.8.6 qnxuser@$TARGET_HOST:/data/home/qnxuser/lib/
```

### Run on Target

```bash
ssh qnxuser@$TARGET_HOST

export LD_LIBRARY_PATH=/data/home/qnxuser/lib:$LD_LIBRARY_PATH

# Open SQLite shell
./sqlite3 /tmp/test.db

# Or run inline SQL
./sqlite3 /tmp/test.db "CREATE TABLE sensors (ts INTEGER, speed REAL, rpm INTEGER);
INSERT INTO sensors VALUES (1000, 87.3, 2341);
SELECT * FROM sensors;"
```

---

## QNX Build System Notes

SQLite requires minimal patches for QNX:

- `SQLITE_OS_OTHER=0` — use the standard POSIX VFS (not a custom one)
- `HAVE_USLEEP=1` — QNX supports `usleep()` via POSIX
- `HAVE_LOCALTIME_R=1` — QNX provides thread-safe `localtime_r()`
- `SQLITE_THREADSAFE=1` — Enable multi-threading using QNX pthreads
- `-D_QNX_SOURCE` — Enable QNX-specific POSIX extensions

---

## Verification

```bash
# On the QNX target, verify SQLite works:
./sqlite3 :memory: "SELECT sqlite_version();"
# Expected output: 3.47.0

./sqlite3 :memory: "
  CREATE TABLE t (id INTEGER PRIMARY KEY, value REAL);
  INSERT INTO t VALUES (1, 3.14159);
  INSERT INTO t VALUES (2, 2.71828);
  SELECT id, value FROM t ORDER BY id;
"
# Expected output:
# 1|3.14159
# 2|2.71828
```
