/*
 * This file is part of the MicroPython QNX 8.0 Port project.
 *
 * Native C extension module `qnx_rt` exposing QNX Neutrino RTOS kernel
 * primitives (<sys/neutrino.h>, <sys/syspage.h>, <sched.h>) to MicroPython.
 *
 * Implements:
 *   - qnx_rt.clock_cycles()       -> int (64-bit hardware ClockCycles())
 *   - qnx_rt.cycles_per_sec()     -> int (SYSPAGE_ENTRY(qtime)->cycles_per_sec)
 *   - qnx_rt.get_priority()       -> int (current QNX thread priority, 1..255)
 *   - qnx_rt.set_priority(prio, [policy]) -> int (set SCHED_FIFO / SCHED_RR priority)
 *   - qnx_rt.get_sched_policy()   -> int (SCHED_FIFO=1, SCHED_RR=2, SCHED_OTHER=3)
 *   - qnx_rt.clock_period_ns([new_ns]) -> int (ClockPeriod() kernel tick in nanoseconds)
 *   - qnx_rt.sysinfo()            -> dict (pid, tid, priority, policy, cycles_per_sec)
 *   - Constants: SCHED_FIFO, SCHED_RR, SCHED_OTHER, SCHED_SPORADIC
 *
 * SPDX-License-Identifier: MIT
 */

#include "py/runtime.h"
#include "py/obj.h"
#include "py/mperrno.h"

#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#if defined(__QNX__)
#include <sys/neutrino.h>
#include <sys/syspage.h>
#endif

// ===========================================================================
// 1. Hardware Cycle Counter: ClockCycles() & SYSPAGE cycles_per_sec
// ===========================================================================

static mp_obj_t mp_qnx_rt_clock_cycles(void) {
#if defined(__QNX__)
    uint64_t cycles = ClockCycles();
    return mp_obj_new_int_from_ull(cycles);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    return mp_obj_new_int_from_ull(ns);
#endif
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_qnx_rt_clock_cycles_obj, mp_qnx_rt_clock_cycles);

static mp_obj_t mp_qnx_rt_cycles_per_sec(void) {
#if defined(__QNX__)
    uint64_t cps = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    return mp_obj_new_int_from_ull(cps);
#else
    return mp_obj_new_int_from_ull(1000000000ULL);
#endif
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_qnx_rt_cycles_per_sec_obj, mp_qnx_rt_cycles_per_sec);

// ===========================================================================
// 2. QNX Real-Time Thread Priority & Scheduling Policy
// ===========================================================================

static mp_obj_t mp_qnx_rt_get_priority(void) {
    int policy = 0;
    struct sched_param param;
    if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
        return MP_OBJ_NEW_SMALL_INT(10); // QNX default user priority is 10
    }
    return MP_OBJ_NEW_SMALL_INT(param.sched_priority);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_qnx_rt_get_priority_obj, mp_qnx_rt_get_priority);

static mp_obj_t mp_qnx_rt_get_sched_policy(void) {
    int policy = SCHED_RR;
    struct sched_param param;
    if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
        return MP_OBJ_NEW_SMALL_INT(SCHED_RR);
    }
    return MP_OBJ_NEW_SMALL_INT(policy);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_qnx_rt_get_sched_policy_obj, mp_qnx_rt_get_sched_policy);

static mp_obj_t mp_qnx_rt_set_priority(size_t n_args, const mp_obj_t *args) {
    int new_prio = (int)mp_obj_get_int(args[0]);
    int policy = SCHED_RR;
    struct sched_param param;

    if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
        policy = SCHED_RR;
    }
    if (n_args >= 2) {
        policy = (int)mp_obj_get_int(args[1]);
    }

    param.sched_priority = new_prio;
    int rc = pthread_setschedparam(pthread_self(), policy, &param);
    if (rc != 0) {
        mp_raise_OSError(rc);
    }
    return MP_OBJ_NEW_SMALL_INT(new_prio);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_qnx_rt_set_priority_obj, 1, 2, mp_qnx_rt_set_priority);

// ===========================================================================
// 3. QNX Kernel Tick Period: ClockPeriod(CLOCK_REALTIME, ...)
// ===========================================================================

static mp_obj_t mp_qnx_rt_clock_period_ns(size_t n_args, const mp_obj_t *args) {
#if defined(__QNX__)
    struct _clockperiod new_cp;
    struct _clockperiod old_cp;
    const struct _clockperiod *new_ptr = NULL;

    if (n_args >= 1 && args[0] != mp_const_none) {
        new_cp.nsec = (uint32_t)mp_obj_get_int(args[0]);
        new_cp.fract = 0;
        new_ptr = &new_cp;
    }

    if (ClockPeriod(CLOCK_REALTIME, new_ptr, &old_cp, 0) == -1) {
        mp_raise_OSError(errno);
    }
    return mp_obj_new_int_from_uint(old_cp.nsec);
#else
    (void)n_args;
    (void)args;
    return mp_obj_new_int_from_uint(1000000U); // 1 ms nominal
#endif
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_qnx_rt_clock_period_ns_obj, 0, 1, mp_qnx_rt_clock_period_ns);

// ===========================================================================
// 4. QNX System & Process Info Snapshot: sysinfo()
// ===========================================================================

static mp_obj_t mp_qnx_rt_sysinfo(void) {
    mp_obj_t dict = mp_obj_new_dict(6);

    int policy = SCHED_RR;
    struct sched_param param;
    param.sched_priority = 10;
    pthread_getschedparam(pthread_self(), &policy, &param);

    const char *policy_name = "SCHED_OTHER";
    if (policy == SCHED_FIFO) {
        policy_name = "SCHED_FIFO";
    } else if (policy == SCHED_RR) {
        policy_name = "SCHED_RR";
    }
#if defined(SCHED_SPORADIC)
    else if (policy == SCHED_SPORADIC) {
        policy_name = "SCHED_SPORADIC";
    }
#endif

    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_pid), MP_OBJ_NEW_SMALL_INT(getpid()));
#if defined(__QNX__)
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_tid), MP_OBJ_NEW_SMALL_INT(gettid()));
#else
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_tid), MP_OBJ_NEW_SMALL_INT(1));
#endif
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_priority), MP_OBJ_NEW_SMALL_INT(param.sched_priority));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_policy), mp_obj_new_str(policy_name, strlen(policy_name)));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_cycles_per_sec), mp_qnx_rt_cycles_per_sec());
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_clock_cycles), mp_qnx_rt_clock_cycles());

    return dict;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_qnx_rt_sysinfo_obj, mp_qnx_rt_sysinfo);

// ===========================================================================
// Module Registration
// ===========================================================================

#ifndef SCHED_SPORADIC
#define SCHED_SPORADIC 4
#endif

static const mp_rom_map_elem_t mp_module_qnx_rt_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),         MP_ROM_QSTR(MP_QSTR_qnx_rt) },
    { MP_ROM_QSTR(MP_QSTR_clock_cycles),     MP_ROM_PTR(&mp_qnx_rt_clock_cycles_obj) },
    { MP_ROM_QSTR(MP_QSTR_cycles_per_sec),   MP_ROM_PTR(&mp_qnx_rt_cycles_per_sec_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_priority),     MP_ROM_PTR(&mp_qnx_rt_get_priority_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_priority),     MP_ROM_PTR(&mp_qnx_rt_set_priority_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_sched_policy), MP_ROM_PTR(&mp_qnx_rt_get_sched_policy_obj) },
    { MP_ROM_QSTR(MP_QSTR_clock_period_ns),  MP_ROM_PTR(&mp_qnx_rt_clock_period_ns_obj) },
    { MP_ROM_QSTR(MP_QSTR_sysinfo),          MP_ROM_PTR(&mp_qnx_rt_sysinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_SCHED_FIFO),       MP_ROM_INT(SCHED_FIFO) },
    { MP_ROM_QSTR(MP_QSTR_SCHED_RR),         MP_ROM_INT(SCHED_RR) },
    { MP_ROM_QSTR(MP_QSTR_SCHED_OTHER),      MP_ROM_INT(SCHED_OTHER) },
    { MP_ROM_QSTR(MP_QSTR_SCHED_SPORADIC),   MP_ROM_INT(SCHED_SPORADIC) },
};
static MP_DEFINE_CONST_DICT(mp_module_qnx_rt_globals, mp_module_qnx_rt_globals_table);

const mp_obj_module_t mp_module_qnx_rt = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_qnx_rt_globals,
};

MP_REGISTER_MODULE(MP_QSTR_qnx_rt, mp_module_qnx_rt);
