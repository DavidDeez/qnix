/*
 * This file is part of the MicroPython QNX 8.0 Port project.
 *
 * Native C extension module providing `import sqlite3` for MicroPython
 * on QNX SDP 8.0, backed by libsqlite3 (SQLite 3.47.0).
 *
 * Implements:
 *   - sqlite3.sqlite_version        (str, e.g. "3.47.0")
 *   - sqlite3.connect(path)         -> Connection
 *   - Connection.execute(sql, [params]) -> Cursor
 *   - Connection.executescript(sql) -> Cursor
 *   - Connection.cursor()           -> Cursor
 *   - Connection.commit()           -> None
 *   - Connection.rollback()         -> None
 *   - Connection.close()            -> None
 *   - Cursor.execute(sql, [params]) -> Cursor
 *   - Cursor.fetchone()             -> tuple | None
 *   - Cursor.fetchall()             -> list[tuple]
 *   - Cursor.close()                -> None
 *   - Cursor iterator protocol      (`for row in conn.execute(...)`)
 *
 * SPDX-License-Identifier: MIT
 */

#include "py/runtime.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/objtuple.h"
#include "py/objlist.h"
#include "py/mperrno.h"

#include <sqlite3.h>
#include <string.h>

// Forward declarations of MicroPython types
static const mp_obj_type_t mp_sqlite3_connection_type;
static const mp_obj_type_t mp_sqlite3_cursor_type;

// Raise a RuntimeError formatted with the current SQLite error message
static void NORETURN mp_sqlite3_raise_error(sqlite3 *db, int rc) {
    const char *errmsg = (db != NULL) ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    mp_raise_msg_varg(&mp_type_RuntimeError,
        MP_ERROR_TEXT("sqlite3 error (%d): %s"), rc, errmsg ? errmsg : "unknown");
}

// ===========================================================================
// Object Structs
// ===========================================================================

typedef struct _mp_sqlite3_connection_obj_t {
    mp_obj_base_t base;
    sqlite3 *db;
    bool in_transaction;
} mp_sqlite3_connection_obj_t;

typedef struct _mp_sqlite3_cursor_obj_t {
    mp_obj_base_t base;
    mp_sqlite3_connection_obj_t *conn;
    sqlite3_stmt *stmt;
    int last_rc;
    bool has_row;
} mp_sqlite3_cursor_obj_t;

// ===========================================================================
// Helper: Statement cleanup, parameter binding, row materialisation
// ===========================================================================

static void cursor_finalize_stmt(mp_sqlite3_cursor_obj_t *self) {
    if (self->stmt != NULL) {
        sqlite3_finalize(self->stmt);
        self->stmt = NULL;
    }
    self->has_row = false;
    self->last_rc = SQLITE_OK;
}

static void cursor_bind_param(sqlite3 *db, sqlite3_stmt *stmt, int idx, mp_obj_t val) {
    int rc = SQLITE_OK;
    if (val == mp_const_none) {
        rc = sqlite3_bind_null(stmt, idx);
    } else if (mp_obj_is_bool(val)) {
        rc = sqlite3_bind_int(stmt, idx, mp_obj_is_true(val) ? 1 : 0);
    } else if (mp_obj_is_int(val)) {
        mp_int_t v = mp_obj_get_int_truncated(val);
        rc = sqlite3_bind_int64(stmt, idx, (sqlite3_int64)v);
    } else if (mp_obj_is_float(val)) {
        mp_float_t v = mp_obj_get_float(val);
        rc = sqlite3_bind_double(stmt, idx, (double)v);
    } else if (mp_obj_is_str_or_bytes(val)) {
        size_t len = 0;
        const char *str = mp_obj_str_get_data(val, &len);
        if (mp_obj_is_str(val)) {
            rc = sqlite3_bind_text(stmt, idx, str, (int)len, SQLITE_TRANSIENT);
        } else {
            rc = sqlite3_bind_blob(stmt, idx, str, (int)len, SQLITE_TRANSIENT);
        }
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("unsupported SQLite parameter type"));
    }

    if (rc != SQLITE_OK) {
        mp_sqlite3_raise_error(db, rc);
    }
}

static void cursor_bind_params(sqlite3 *db, sqlite3_stmt *stmt, mp_obj_t params_obj) {
    if (params_obj == MP_OBJ_NULL || params_obj == mp_const_none) {
        return;
    }
    size_t len = 0;
    mp_obj_t *items = NULL;
    mp_obj_get_array(params_obj, &len, &items);

    for (size_t i = 0; i < len; i++) {
        cursor_bind_param(db, stmt, (int)(i + 1), items[i]);
    }
}

static mp_obj_t cursor_materialize_row(sqlite3_stmt *stmt) {
    int col_count = sqlite3_column_count(stmt);
    mp_obj_tuple_t *row = MP_OBJ_TO_PTR(mp_obj_new_tuple(col_count, NULL));

    for (int i = 0; i < col_count; i++) {
        int col_type = sqlite3_column_type(stmt, i);
        switch (col_type) {
            case SQLITE_INTEGER: {
                sqlite3_int64 val = sqlite3_column_int64(stmt, i);
                row->items[i] = mp_obj_new_int_from_ll(val);
                break;
            }
            case SQLITE_FLOAT: {
                double val = sqlite3_column_double(stmt, i);
                row->items[i] = mp_obj_new_float((mp_float_t)val);
                break;
            }
            case SQLITE_TEXT: {
                const char *txt = (const char *)sqlite3_column_text(stmt, i);
                int bytes = sqlite3_column_bytes(stmt, i);
                row->items[i] = mp_obj_new_str(txt ? txt : "", bytes);
                break;
            }
            case SQLITE_BLOB: {
                const void *blob = sqlite3_column_blob(stmt, i);
                int bytes = sqlite3_column_bytes(stmt, i);
                row->items[i] = mp_obj_new_bytes(blob ? blob : "", bytes);
                break;
            }
            case SQLITE_NULL:
            default:
                row->items[i] = mp_const_none;
                break;
        }
    }
    return MP_OBJ_FROM_PTR(row);
}

// ===========================================================================
// Cursor Methods
// ===========================================================================

static mp_obj_t cursor_execute_internal(mp_sqlite3_cursor_obj_t *self, size_t n_args, const mp_obj_t *args) {
    if (self->conn == NULL || self->conn->db == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Cannot operate on a closed database"));
    }

    cursor_finalize_stmt(self);

    size_t sql_len = 0;
    const char *sql = mp_obj_str_get_data(args[1], &sql_len);
    sqlite3 *db = self->conn->db;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, (int)sql_len, &stmt, NULL);
    if (rc != SQLITE_OK) {
        mp_sqlite3_raise_error(db, rc);
    }

    if (stmt == NULL) {
        // Empty statement / comment-only SQL
        return MP_OBJ_FROM_PTR(self);
    }

    self->stmt = stmt;

    if (n_args >= 3) {
        cursor_bind_params(db, stmt, args[2]);
    }

    // Step once immediately so INSERT/UPDATE/CREATE execute right away
    // and SELECT has its first row buffered for fetchone()/iteration.
    rc = sqlite3_step(stmt);
    self->last_rc = rc;
    if (rc == SQLITE_ROW) {
        self->has_row = true;
    } else if (rc == SQLITE_DONE) {
        self->has_row = false;
        cursor_finalize_stmt(self);
    } else {
        cursor_finalize_stmt(self);
        mp_sqlite3_raise_error(db, rc);
    }

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_sqlite3_cursor_execute(size_t n_args, const mp_obj_t *args) {
    mp_sqlite3_cursor_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    return cursor_execute_internal(self, n_args, args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_sqlite3_cursor_execute_obj, 2, 3, mp_sqlite3_cursor_execute);

static mp_obj_t mp_sqlite3_cursor_fetchone(mp_obj_t self_in) {
    mp_sqlite3_cursor_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->stmt == NULL || !self->has_row) {
        return mp_const_none;
    }

    mp_obj_t row = cursor_materialize_row(self->stmt);

    // Advance cursor to next row
    int rc = sqlite3_step(self->stmt);
    self->last_rc = rc;
    if (rc == SQLITE_ROW) {
        self->has_row = true;
    } else if (rc == SQLITE_DONE) {
        cursor_finalize_stmt(self);
    } else {
        sqlite3 *db = self->conn ? self->conn->db : NULL;
        cursor_finalize_stmt(self);
        mp_sqlite3_raise_error(db, rc);
    }

    return row;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_cursor_fetchone_obj, mp_sqlite3_cursor_fetchone);

static mp_obj_t mp_sqlite3_cursor_fetchall(mp_obj_t self_in) {
    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (;;) {
        mp_obj_t row = mp_sqlite3_cursor_fetchone(self_in);
        if (row == mp_const_none) {
            break;
        }
        mp_obj_list_append(list, row);
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_cursor_fetchall_obj, mp_sqlite3_cursor_fetchall);

static mp_obj_t mp_sqlite3_cursor_close(mp_obj_t self_in) {
    mp_sqlite3_cursor_obj_t *self = MP_OBJ_TO_PTR(self_in);
    cursor_finalize_stmt(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_cursor_close_obj, mp_sqlite3_cursor_close);

static mp_obj_t mp_sqlite3_cursor_iternext(mp_obj_t self_in) {
    mp_obj_t row = mp_sqlite3_cursor_fetchone(self_in);
    if (row == mp_const_none) {
        return MP_OBJ_STOP_ITERATION;
    }
    return row;
}

static const mp_rom_map_elem_t mp_sqlite3_cursor_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_execute),  MP_ROM_PTR(&mp_sqlite3_cursor_execute_obj) },
    { MP_ROM_QSTR(MP_QSTR_fetchone), MP_ROM_PTR(&mp_sqlite3_cursor_fetchone_obj) },
    { MP_ROM_QSTR(MP_QSTR_fetchall), MP_ROM_PTR(&mp_sqlite3_cursor_fetchall_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),    MP_ROM_PTR(&mp_sqlite3_cursor_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__),  MP_ROM_PTR(&mp_sqlite3_cursor_close_obj) },
};
static MP_DEFINE_CONST_DICT(mp_sqlite3_cursor_locals_dict, mp_sqlite3_cursor_locals_dict_table);

#if defined(MP_DEFINE_CONST_OBJ_TYPE)
static MP_DEFINE_CONST_OBJ_TYPE(
    mp_sqlite3_cursor_type,
    MP_QSTR_Cursor,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    iter, mp_sqlite3_cursor_iternext,
    locals_dict, &mp_sqlite3_cursor_locals_dict
);
#else
static const mp_obj_type_t mp_sqlite3_cursor_type = {
    { &mp_type_type },
    .name = MP_QSTR_Cursor,
    .getiter = mp_identity_getiter,
    .iternext = mp_sqlite3_cursor_iternext,
    .locals_dict = (mp_obj_dict_t *)&mp_sqlite3_cursor_locals_dict,
};
#endif

// ===========================================================================
// Connection Methods
// ===========================================================================

static mp_sqlite3_cursor_obj_t *connection_new_cursor(mp_sqlite3_connection_obj_t *conn) {
    mp_sqlite3_cursor_obj_t *cursor = mp_obj_malloc(mp_sqlite3_cursor_obj_t, &mp_sqlite3_cursor_type);
    cursor->conn = conn;
    cursor->stmt = NULL;
    cursor->last_rc = SQLITE_OK;
    cursor->has_row = false;
    return cursor;
}

static mp_obj_t mp_sqlite3_connection_cursor(mp_obj_t self_in) {
    mp_sqlite3_connection_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->db == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Cannot operate on a closed database"));
    }
    return MP_OBJ_FROM_PTR(connection_new_cursor(self));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_connection_cursor_obj, mp_sqlite3_connection_cursor);

static mp_obj_t mp_sqlite3_connection_execute(size_t n_args, const mp_obj_t *args) {
    mp_sqlite3_connection_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_sqlite3_cursor_obj_t *cursor = connection_new_cursor(self);
    mp_obj_t cursor_args[3] = {
        MP_OBJ_FROM_PTR(cursor),
        args[1],
        (n_args >= 3) ? args[2] : mp_const_none
    };
    return cursor_execute_internal(cursor, n_args, cursor_args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_sqlite3_connection_execute_obj, 2, 3, mp_sqlite3_connection_execute);

static mp_obj_t mp_sqlite3_connection_executescript(mp_obj_t self_in, mp_obj_t sql_in) {
    mp_sqlite3_connection_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->db == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Cannot operate on a closed database"));
    }
    const char *sql = mp_obj_str_get_str(sql_in);
    char *errmsg = NULL;
    int rc = sqlite3_exec(self->db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg != NULL) {
            sqlite3_free(errmsg);
        }
        mp_sqlite3_raise_error(self->db, rc);
    }
    return MP_OBJ_FROM_PTR(connection_new_cursor(self));
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_sqlite3_connection_executescript_obj, mp_sqlite3_connection_executescript);

static mp_obj_t mp_sqlite3_connection_commit(mp_obj_t self_in) {
    mp_sqlite3_connection_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->db == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Cannot operate on a closed database"));
    }
    // In SQLite autocommit mode (sqlite3_get_autocommit(db) != 0), commit() is a no-op
    if (!sqlite3_get_autocommit(self->db)) {
        int rc = sqlite3_exec(self->db, "COMMIT;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            mp_sqlite3_raise_error(self->db, rc);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_connection_commit_obj, mp_sqlite3_connection_commit);

static mp_obj_t mp_sqlite3_connection_rollback(mp_obj_t self_in) {
    mp_sqlite3_connection_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->db == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Cannot operate on a closed database"));
    }
    if (!sqlite3_get_autocommit(self->db)) {
        int rc = sqlite3_exec(self->db, "ROLLBACK;", NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            mp_sqlite3_raise_error(self->db, rc);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_connection_rollback_obj, mp_sqlite3_connection_rollback);

static mp_obj_t mp_sqlite3_connection_close(mp_obj_t self_in) {
    mp_sqlite3_connection_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->db != NULL) {
        sqlite3_close_v2(self->db);
        self->db = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_connection_close_obj, mp_sqlite3_connection_close);

static const mp_rom_map_elem_t mp_sqlite3_connection_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_cursor),        MP_ROM_PTR(&mp_sqlite3_connection_cursor_obj) },
    { MP_ROM_QSTR(MP_QSTR_execute),       MP_ROM_PTR(&mp_sqlite3_connection_execute_obj) },
    { MP_ROM_QSTR(MP_QSTR_executescript), MP_ROM_PTR(&mp_sqlite3_connection_executescript_obj) },
    { MP_ROM_QSTR(MP_QSTR_commit),        MP_ROM_PTR(&mp_sqlite3_connection_commit_obj) },
    { MP_ROM_QSTR(MP_QSTR_rollback),      MP_ROM_PTR(&mp_sqlite3_connection_rollback_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),         MP_ROM_PTR(&mp_sqlite3_connection_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__),       MP_ROM_PTR(&mp_sqlite3_connection_close_obj) },
};
static MP_DEFINE_CONST_DICT(mp_sqlite3_connection_locals_dict, mp_sqlite3_connection_locals_dict_table);

#if defined(MP_DEFINE_CONST_OBJ_TYPE)
static MP_DEFINE_CONST_OBJ_TYPE(
    mp_sqlite3_connection_type,
    MP_QSTR_Connection,
    MP_TYPE_FLAG_NONE,
    locals_dict, &mp_sqlite3_connection_locals_dict
);
#else
static const mp_obj_type_t mp_sqlite3_connection_type = {
    { &mp_type_type },
    .name = MP_QSTR_Connection,
    .locals_dict = (mp_obj_dict_t *)&mp_sqlite3_connection_locals_dict,
};
#endif

// ===========================================================================
// Module-level Functions & Globals
// ===========================================================================

static mp_obj_t mp_sqlite3_connect(mp_obj_t db_path_in) {
    const char *db_path = mp_obj_str_get_str(db_path_in);
    sqlite3 *db = NULL;

    int rc = sqlite3_open_v2(
        db_path,
        &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
        NULL
    );
    if (rc != SQLITE_OK) {
        if (db != NULL) {
            sqlite3_close_v2(db);
        }
        mp_sqlite3_raise_error(NULL, rc);
    }

    // Enable WAL mode where supported on QNX filesystems (/tmp, /data) for real-time write speed
    sqlite3_exec(db, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);

    mp_sqlite3_connection_obj_t *conn = mp_obj_malloc(mp_sqlite3_connection_obj_t, &mp_sqlite3_connection_type);
    conn->db = db;
    conn->in_transaction = false;
    return MP_OBJ_FROM_PTR(conn);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_sqlite3_connect_obj, mp_sqlite3_connect);

static const MP_DEFINE_STR_OBJ(mp_sqlite3_version_str_obj, SQLITE_VERSION);

static const mp_rom_map_elem_t mp_module_sqlite3_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),       MP_ROM_QSTR(MP_QSTR_sqlite3) },
    { MP_ROM_QSTR(MP_QSTR_connect),        MP_ROM_PTR(&mp_sqlite3_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_Connection),     MP_ROM_PTR(&mp_sqlite3_connection_type) },
    { MP_ROM_QSTR(MP_QSTR_Cursor),         MP_ROM_PTR(&mp_sqlite3_cursor_type) },
    { MP_ROM_QSTR(MP_QSTR_sqlite_version), MP_ROM_PTR(&mp_sqlite3_version_str_obj) },
};
static MP_DEFINE_CONST_DICT(mp_module_sqlite3_globals, mp_module_sqlite3_globals_table);

const mp_obj_module_t mp_module_sqlite3 = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_sqlite3_globals,
};

MP_REGISTER_MODULE(MP_QSTR_sqlite3, mp_module_sqlite3);
