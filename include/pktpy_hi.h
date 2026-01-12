/*
 * pktpy_hi.h - Higher-level wrapper for pocketpy C-API
 *
 * A thin, header-only wrapper that reduces boilerplate while maintaining
 * full interoperability with the low-level pocketpy API.
 *
 * Prefix: ph_ (pocketpy high-level)
 *
 * IMPORTANT LIFETIME RULES:
 * -------------------------
 * Many functions return pointers to volatile storage. Read the documentation
 * for each function carefully. General rules:
 *
 * 1. ph_tmp_*() functions (ph_tmp_int, ph_tmp_str, etc.) use register r0.
 *    Each call OVERWRITES the previous value. Use immediately or use ph_*_r()
 *    variants for stable storage.
 *
 * 2. ph_Result.val from base ph_call*() functions points to py_retval(),
 *    which is overwritten by every Python call. Use _r variants for stability.
 *
 * 3. String pointers from ph_repr(), ph_as_str() are valid only until the
 *    next Python operation. Use immediately (e.g., in printf).
 */

#pragma once

/* Version information */
#define PH_VERSION_MAJOR 0
#define PH_VERSION_MINOR 2
#define PH_VERSION_PATCH 0
#define PH_VERSION_STRING "0.2.0"

/* Requires pocketpy 2.x */
#define PH_POCKETPY_VERSION_MIN 2

#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"
#include <stdbool.h>
#include <stddef.h>  /* for ptrdiff_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 0. Constants and Internal Helpers
 * ============================================================================
 */

/*
 * Maximum number of registers available (r0-r7).
 * This matches pocketpy's 8 global registers accessed via py_getreg(0..7).
 * Convention: r0-r3 for temporaries, r4-r7 for user storage.
 */
#define PH_MAX_REG 8

/* Internal: validate register index, returns true if valid */
static inline bool ph__check_reg(int reg) {
    return reg >= 0 && reg < PH_MAX_REG;
}

/* ============================================================================
 * 1. Scope Management
 * ============================================================================
 * Automatic stack unwinding and exception cleanup.
 */

typedef struct {
    py_StackRef unwind_point;
    bool has_exception;
} ph_Scope;

// Begin a new scope - captures current stack position
static inline ph_Scope ph_scope_begin(void) {
    ph_Scope s;
    s.unwind_point = py_peek(0);
    s.has_exception = false;
    return s;
}

/* End scope and handle any pending exception.
 * Returns true if execution was successful, false if exception occurred. */
static inline bool ph_scope_end(ph_Scope* scope) {
    if (py_checkexc()) {
        scope->has_exception = true;
        py_clearexc(scope->unwind_point);
        return false;
    }
    /* Restore stack on success (use ptrdiff_t to avoid overflow) */
    ptrdiff_t depth = py_peek(0) - scope->unwind_point;
    if (depth > 0) {
        py_shrink((int)depth);
    }
    return true;
}

/* End scope, print exception if any, return success status. */
static inline bool ph_scope_end_print(ph_Scope* scope) {
    if (py_checkexc()) {
        scope->has_exception = true;
        py_printexc();
        py_clearexc(scope->unwind_point);
        return false;
    }
    /* Restore stack on success */
    ptrdiff_t depth = py_peek(0) - scope->unwind_point;
    if (depth > 0) {
        py_shrink((int)depth);
    }
    return true;
}

/* End scope, restore stack, but keep exception for propagation.
 * Returns true if successful, false if exception (exception NOT cleared). */
static inline bool ph_scope_end_raise(ph_Scope* scope) {
    /* Restore stack regardless of exception status */
    ptrdiff_t depth = py_peek(0) - scope->unwind_point;
    if (depth > 0) {
        py_shrink((int)depth);
    }
    if (py_checkexc()) {
        scope->has_exception = true;
        return false;
    }
    return true;
}

// Check if scope had an exception (after ph_scope_end)
static inline bool ph_scope_failed(const ph_Scope* scope) {
    return scope->has_exception;
}

/* ============================================================================
 * 2. Safe Execution Helpers
 * ============================================================================
 * Combine scope management with common execution patterns.
 *
 * Two variants are provided:
 * - ph_exec/ph_eval: Print and clear exceptions (fire-and-forget)
 * - ph_exec_raise/ph_eval_raise: Keep exceptions for propagation
 */

// Execute Python code with automatic exception handling
// Returns true on success, false on exception (printed and cleared)
static inline bool ph_exec(const char* source, const char* filename) {
    ph_Scope scope = ph_scope_begin();
    py_exec(source, filename, EXEC_MODE, NULL);
    return ph_scope_end_print(&scope);
}

// Execute in a specific module
static inline bool ph_exec_in(const char* source, const char* filename, py_Ref module) {
    ph_Scope scope = ph_scope_begin();
    py_exec(source, filename, EXEC_MODE, module);
    return ph_scope_end_print(&scope);
}

// Evaluate expression, result in py_retval()
// Returns true on success
static inline bool ph_eval(const char* source) {
    ph_Scope scope = ph_scope_begin();
    py_eval(source, NULL);
    return ph_scope_end_print(&scope);
}

// Evaluate expression in a specific module, result in py_retval()
static inline bool ph_eval_in(const char* source, py_Ref module) {
    ph_Scope scope = ph_scope_begin();
    py_eval(source, module);
    return ph_scope_end_print(&scope);
}

// Execute Python code, propagating any exception
// Returns true on success, false on exception (exception NOT cleared, use py_checkexc())
static inline bool ph_exec_raise(const char* source, const char* filename) {
    ph_Scope scope = ph_scope_begin();
    py_exec(source, filename, EXEC_MODE, NULL);
    return ph_scope_end_raise(&scope);
}

// Execute in a specific module, propagating any exception
static inline bool ph_exec_in_raise(const char* source, const char* filename, py_Ref module) {
    ph_Scope scope = ph_scope_begin();
    py_exec(source, filename, EXEC_MODE, module);
    return ph_scope_end_raise(&scope);
}

// Evaluate expression, propagating any exception
// Result in py_retval() on success
static inline bool ph_eval_raise(const char* source) {
    ph_Scope scope = ph_scope_begin();
    py_eval(source, NULL);
    return ph_scope_end_raise(&scope);
}

// Evaluate in a specific module, propagating any exception
static inline bool ph_eval_in_raise(const char* source, py_Ref module) {
    ph_Scope scope = ph_scope_begin();
    py_eval(source, module);
    return ph_scope_end_raise(&scope);
}

/* ============================================================================
 * 3. Value Creation (Return-by-Value Style)
 * ============================================================================
 * Use registers internally to avoid py_OutRef awkwardness.
 *
 * IMPORTANT: Register Lifetime Rules
 * -----------------------------------
 * - ph_tmp_int(), ph_tmp_str(), etc. all use register r0
 * - Each call OVERWRITES the previous value in r0
 * - The returned pointer is only valid until the next ph_tmp_* or py_* call
 *
 * WRONG - second call invalidates first result:
 *   py_GlobalRef a = ph_tmp_int(1);
 *   py_GlobalRef b = ph_tmp_int(2);  // a now points to 2!
 *
 * CORRECT - use immediately (object ref is copied):
 *   ph_setglobal("x", ph_tmp_int(1));
 *   ph_setglobal("y", ph_tmp_int(2));
 *
 * CORRECT - use explicit registers for multiple values:
 *   py_GlobalRef a = ph_int_r(0, 1);
 *   py_GlobalRef b = ph_int_r(1, 2);  // a and b are independent
 *
 * Available registers: 0-7 (r0-r7)
 * Convention: r0-r3 for temporaries, r4-r7 for user storage
 */

/*
 * Create temporary values using r0 - USE IMMEDIATELY OR VALUE IS LOST.
 * The "tmp" prefix signals that storing the returned pointer is dangerous.
 * Safe patterns: ph_setglobal("x", ph_tmp_int(1));
 *                ph_call1("f", ph_tmp_int(1));
 * Dangerous:     py_GlobalRef a = ph_tmp_int(1);  // Don't do this!
 */
static inline py_GlobalRef ph_tmp_int(py_i64 val) {
    py_newint(py_r0(), val);
    return py_r0();
}

static inline py_GlobalRef ph_tmp_float(py_f64 val) {
    py_newfloat(py_r0(), val);
    return py_r0();
}

static inline py_GlobalRef ph_tmp_str(const char* val) {
    py_newstr(py_r0(), val);
    return py_r0();
}

static inline py_GlobalRef ph_tmp_bool(bool val) {
    py_newbool(py_r0(), val);
    return py_r0();
}

/*
 * Create values in a specific register (0-7) for stable storage.
 * Use these when you need to keep multiple values simultaneously.
 * Returns NULL if register index is invalid.
 */
static inline py_GlobalRef ph_int_r(int reg, py_i64 val) {
    if (!ph__check_reg(reg)) return NULL;
    py_newint(py_getreg(reg), val);
    return py_getreg(reg);
}

static inline py_GlobalRef ph_float_r(int reg, py_f64 val) {
    if (!ph__check_reg(reg)) return NULL;
    py_newfloat(py_getreg(reg), val);
    return py_getreg(reg);
}

static inline py_GlobalRef ph_str_r(int reg, const char* val) {
    if (!ph__check_reg(reg)) return NULL;
    py_newstr(py_getreg(reg), val);
    return py_getreg(reg);
}

static inline py_GlobalRef ph_bool_r(int reg, bool val) {
    if (!ph__check_reg(reg)) return NULL;
    py_newbool(py_getreg(reg), val);
    return py_getreg(reg);
}

/* ============================================================================
 * 4. Safe Function Calls
 * ============================================================================
 * Simplified function calling with result structs.
 *
 * Three variants are provided:
 * - ph_call*: Print and clear exceptions (like ph_exec), result in volatile py_retval()
 * - ph_call*_raise: Keep exceptions for propagation (caller must handle)
 * - ph_call*_r: Copy result to specified register (0-7) for stable storage
 *
 * WARNING: The base ph_call* variants return py_retval() which is overwritten
 * by every subsequent Python call. Use ph_call*_r() when you need to keep
 * multiple results or pass results to other Python calls.
 */

typedef struct {
    bool ok;           // true if call succeeded
    py_GlobalRef val;  // result (only valid if ok==true)
} ph_Result;

// Call a global function by name with no arguments
static inline ph_Result ph_call0(const char* func_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 0, NULL);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a global function with 1 argument
static inline ph_Result ph_call1(const char* func_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 1, arg0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a global function with 2 arguments (args must be contiguous)
static inline ph_Result ph_call2(const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 2, args);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a global function with 3 arguments (args must be contiguous)
static inline ph_Result ph_call3(const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 3, args);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call any callable reference with argc arguments
static inline ph_Result ph_call(py_Ref callable, int argc, py_Ref argv) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    r.ok = py_call(callable, argc, argv);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a method on an object with no arguments
static inline ph_Result ph_callmethod0(py_Ref obj, const char* method_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_vectorcall(0, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a method on an object with 1 argument
static inline ph_Result ph_callmethod1(py_Ref obj, const char* method_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    py_push(arg0);
    r.ok = py_vectorcall(1, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a method on an object with 2 arguments
static inline ph_Result ph_callmethod2(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    r.ok = py_vectorcall(2, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// Call a method on an object with 3 arguments
static inline ph_Result ph_callmethod3(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    py_push(arg2);
    r.ok = py_vectorcall(3, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_print(&scope);
    return r;
}

// --- Propagating variants (exceptions NOT cleared) ---

// Call a global function by name with no arguments, propagating exceptions
static inline ph_Result ph_call0_raise(const char* func_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 0, NULL);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a global function with 1 argument, propagating exceptions
static inline ph_Result ph_call1_raise(const char* func_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 1, arg0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a global function with 2 arguments, propagating exceptions
static inline ph_Result ph_call2_raise(const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 2, args);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a global function with 3 arguments, propagating exceptions
static inline ph_Result ph_call3_raise(const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 3, args);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call any callable reference with argc arguments, propagating exceptions
static inline ph_Result ph_call_raise(py_Ref callable, int argc, py_Ref argv) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    r.ok = py_call(callable, argc, argv);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a method on an object with no arguments, propagating exceptions
static inline ph_Result ph_callmethod0_raise(py_Ref obj, const char* method_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_vectorcall(0, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a method on an object with 1 argument, propagating exceptions
static inline ph_Result ph_callmethod1_raise(py_Ref obj, const char* method_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    py_push(arg0);
    r.ok = py_vectorcall(1, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a method on an object with 2 arguments, propagating exceptions
static inline ph_Result ph_callmethod2_raise(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    r.ok = py_vectorcall(2, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// Call a method on an object with 3 arguments, propagating exceptions
static inline ph_Result ph_callmethod3_raise(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    py_push(arg2);
    r.ok = py_vectorcall(3, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end_raise(&scope);
    return r;
}

// --- Register-backed variants (result copied to stable register) ---
// All _r variants validate register index and return ok=false if invalid.

// Call a global function, copy result to register
static inline ph_Result ph_call0_r(int reg, const char* func_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 0, NULL);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call with 1 argument, copy result to register
static inline ph_Result ph_call1_r(int reg, const char* func_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 1, arg0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call with 2 arguments, copy result to register
static inline ph_Result ph_call2_r(int reg, const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 2, args);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call with 3 arguments, copy result to register
static inline ph_Result ph_call3_r(int reg, const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_call(fn, 3, args);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call any callable, copy result to register
static inline ph_Result ph_call_r(int reg, py_Ref callable, int argc, py_Ref argv) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    r.ok = py_call(callable, argc, argv);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call method with no arguments, copy result to register
static inline ph_Result ph_callmethod0_r(int reg, py_Ref obj, const char* method_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    r.ok = py_vectorcall(0, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call method with 1 argument, copy result to register
static inline ph_Result ph_callmethod1_r(int reg, py_Ref obj, const char* method_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;  // Invalid register

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    py_push(arg0);
    r.ok = py_vectorcall(1, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call method with 2 arguments, copy result to register
static inline ph_Result ph_callmethod2_r(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    r.ok = py_vectorcall(2, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// Call method with 3 arguments, copy result to register
static inline ph_Result ph_callmethod3_r(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_print(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    py_push(arg2);
    r.ok = py_vectorcall(3, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_print(&scope);
    return r;
}

// --- Register-backed variants with exception propagation (stable storage + raise) ---
// All _r_raise variants validate register index and return ok=false if invalid.

// Call a global function, copy result to register, propagate exception
static inline ph_Result ph_call0_r_raise(int reg, const char* func_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 0, NULL);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call with 1 argument, copy result to register, propagate exception
static inline ph_Result ph_call1_r_raise(int reg, const char* func_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 1, arg0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call with 2 arguments, copy result to register, propagate exception
static inline ph_Result ph_call2_r_raise(int reg, const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 2, args);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call with 3 arguments, copy result to register, propagate exception
static inline ph_Result ph_call3_r_raise(int reg, const char* func_name, py_Ref args) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_call(fn, 3, args);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call any callable, copy result to register, propagate exception
static inline ph_Result ph_call_r_raise(int reg, py_Ref callable, int argc, py_Ref argv) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    r.ok = py_call(callable, argc, argv);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call method with no arguments, copy result to register, propagate exception
static inline ph_Result ph_callmethod0_r_raise(int reg, py_Ref obj, const char* method_name) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    r.ok = py_vectorcall(0, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call method with 1 argument, copy result to register, propagate exception
static inline ph_Result ph_callmethod1_r_raise(int reg, py_Ref obj, const char* method_name, py_Ref arg0) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    py_push(arg0);
    r.ok = py_vectorcall(1, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call method with 2 arguments, copy result to register, propagate exception
static inline ph_Result ph_callmethod2_r_raise(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    r.ok = py_vectorcall(2, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

// Call method with 3 arguments, copy result to register, propagate exception
static inline ph_Result ph_callmethod3_r_raise(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2) {
    ph_Result r;
    r.ok = false;
    r.val = NULL;

    if (!ph__check_reg(reg)) return r;

    ph_Scope scope = ph_scope_begin();
    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end_raise(&scope);
        return r;
    }

    py_push(arg0);
    py_push(arg1);
    py_push(arg2);
    r.ok = py_vectorcall(3, 0);
    if (r.ok) {
        py_assign(py_getreg(reg), py_retval());
        r.val = py_getreg(reg);
    }
    ph_scope_end_raise(&scope);
    return r;
}

/* ============================================================================
 * 5. Value Extraction Helpers
 * ============================================================================
 * Safe extraction with default values (no exceptions on type mismatch).
 */

// Get int or default
static inline py_i64 ph_as_int(py_Ref val, py_i64 default_val) {
    if (py_isint(val)) return py_toint(val);
    return default_val;
}

// Get float or default (accepts int or float)
static inline py_f64 ph_as_float(py_Ref val, py_f64 default_val) {
    if (py_isfloat(val)) return py_tofloat(val);
    if (py_isint(val)) return (py_f64)py_toint(val);
    return default_val;
}

// Get string or default
static inline const char* ph_as_str(py_Ref val, const char* default_val) {
    if (py_isstr(val)) return py_tostr(val);
    return default_val;
}

// Get bool or default
static inline bool ph_as_bool(py_Ref val, bool default_val) {
    if (py_isbool(val)) return py_tobool(val);
    return default_val;
}

// Check if value is truthy (handles exceptions internally)
// Note: py_bool returns 1 for true, 0 for false, -1 for error
// For some types (strings, lists) it may return length as truthy indicator
static inline bool ph_is_truthy(py_Ref val) {
    int result = py_bool(val);
    if (result < 0) {
        py_clearexc(NULL);
        return false;
    }
    return result > 0;
}

// Check if value is truthy (propagates exceptions)
// Returns: 1 for truthy, 0 for falsy, -1 for error (exception set, use py_checkexc())
static inline int ph_is_truthy_raise(py_Ref val) {
    return py_bool(val);
}

// Check if value is None
static inline bool ph_is_none(py_Ref val) {
    return py_isnone(val);
}

// Check if value is nil (invalid/unset)
static inline bool ph_is_nil(py_Ref val) {
    return py_isnil(val);
}

/* ============================================================================
 * 6. Simplified Binding
 * ============================================================================
 * Unified binding with clearer semantics.
 */

// Bind a C function to the main module
static inline void ph_def(const char* sig, py_CFunction f) {
    py_GlobalRef main_mod = py_getmodule("__main__");
    py_bind(main_mod, sig, f);
}

// Bind a C function to a named module (creates module if needed)
static inline void ph_def_in(const char* module_path, const char* sig, py_CFunction f) {
    py_GlobalRef mod = py_getmodule(module_path);
    if (!mod) {
        mod = py_newmodule(module_path);
    }
    py_bind(mod, sig, f);
}

// Set a global variable in __main__
static inline void ph_setglobal(const char* name, py_Ref val) {
    py_setglobal(py_name(name), val);
}

// Get a global variable from __main__ (returns NULL if not found)
static inline py_ItemRef ph_getglobal(const char* name) {
    return py_getglobal(py_name(name));
}

// Get or create a module
static inline py_GlobalRef ph_module(const char* path) {
    py_GlobalRef mod = py_getmodule(path);
    if (!mod) {
        mod = py_newmodule(path);
    }
    return mod;
}

/* ============================================================================
 * 7. Argument Helpers for Native Functions
 * ============================================================================
 * Macros to simplify argument extraction in py_CFunction implementations.
 *
 * NOTE: These macros assume 'argc' and 'argv' are in scope (standard for
 * py_CFunction). Required argument macros now include bounds checking.
 */

/*
 * Get required argument as int with bounds and type checking.
 * Raises TypeError if argc is insufficient or type doesn't match.
 */
#define PH_ARG_INT(i, var) \
    py_i64 var; \
    do { \
        if ((i) >= argc) { \
            py_exception(tp_TypeError, "missing required argument at index %d", (i)); \
            return false; \
        } \
        if (!py_castint(py_arg(i), &var)) return false; \
    } while(0)

/* Get required argument as float (accepts int or float) */
#define PH_ARG_FLOAT(i, var) \
    py_f64 var; \
    do { \
        if ((i) >= argc) { \
            py_exception(tp_TypeError, "missing required argument at index %d", (i)); \
            return false; \
        } \
        if (!py_castfloat(py_arg(i), &var)) return false; \
    } while(0)

/* Get required argument as string */
#define PH_ARG_STR(i, var) \
    const char* var; \
    do { \
        if ((i) >= argc) { \
            py_exception(tp_TypeError, "missing required argument at index %d", (i)); \
            return false; \
        } \
        if (!py_checkstr(py_arg(i))) return false; \
        var = py_tostr(py_arg(i)); \
    } while(0)

/* Get required argument as bool */
#define PH_ARG_BOOL(i, var) \
    bool var; \
    do { \
        if ((i) >= argc) { \
            py_exception(tp_TypeError, "missing required argument at index %d", (i)); \
            return false; \
        } \
        if (!py_checkbool(py_arg(i))) return false; \
        var = py_tobool(py_arg(i)); \
    } while(0)

/* Get raw py_Ref argument (no type checking) */
#define PH_ARG_REF(i, var) \
    py_Ref var; \
    do { \
        if ((i) >= argc) { \
            py_exception(tp_TypeError, "missing required argument at index %d", (i)); \
            return false; \
        } \
        var = py_arg(i); \
    } while(0)

/* Helper to check if argument should use default (nil or None) */
#define PH_ARG_IS_DEFAULT(i) (py_isnil(py_arg(i)) || py_isnone(py_arg(i)))

/* Optional int argument with default value */
#define PH_ARG_INT_OPT(i, var, default_val) \
    py_i64 var = (default_val); \
    do { \
        if (argc > (i) && !PH_ARG_IS_DEFAULT(i)) { \
            if (!py_castint(py_arg(i), &var)) return false; \
        } \
    } while(0)

/* Optional float argument with default value */
#define PH_ARG_FLOAT_OPT(i, var, default_val) \
    py_f64 var = (default_val); \
    do { \
        if (argc > (i) && !PH_ARG_IS_DEFAULT(i)) { \
            if (!py_castfloat(py_arg(i), &var)) return false; \
        } \
    } while(0)

/* Optional string argument with default value */
#define PH_ARG_STR_OPT(i, var, default_val) \
    const char* var = (default_val); \
    do { \
        if (argc > (i) && !PH_ARG_IS_DEFAULT(i)) { \
            if (!py_checkstr(py_arg(i))) return false; \
            var = py_tostr(py_arg(i)); \
        } \
    } while(0)

/* Optional bool argument with default value */
#define PH_ARG_BOOL_OPT(i, var, default_val) \
    bool var = (default_val); \
    do { \
        if (argc > (i) && !PH_ARG_IS_DEFAULT(i)) { \
            if (!py_checkbool(py_arg(i))) return false; \
            var = py_tobool(py_arg(i)); \
        } \
    } while(0)

/* Return helpers - set retval and return true */
#define PH_RETURN_INT(val) \
    do { py_newint(py_retval(), (val)); return true; } while(0)

#define PH_RETURN_FLOAT(val) \
    do { py_newfloat(py_retval(), (val)); return true; } while(0)

#define PH_RETURN_STR(val) \
    do { py_newstr(py_retval(), (val)); return true; } while(0)

#define PH_RETURN_BOOL(val) \
    do { py_newbool(py_retval(), (val)); return true; } while(0)

#define PH_RETURN_NONE \
    do { py_newnone(py_retval()); return true; } while(0)

#define PH_RETURN(ref) \
    do { py_assign(py_retval(), (ref)); return true; } while(0)

/* ============================================================================
 * 8. List Helpers
 * ============================================================================
 */

typedef bool (*ph_ListCallback)(int index, py_Ref item, void* ctx);

// Iterate over a list with a callback
static inline bool ph_list_foreach(py_Ref list, ph_ListCallback cb, void* ctx) {
    int len = py_list_len(list);
    for (int i = 0; i < len; i++) {
        py_ItemRef item = py_list_getitem(list, i);
        if (!cb(i, item, ctx)) return false;
    }
    return true;
}

// Build a list from C array of ints
static inline void ph_list_from_ints(py_OutRef out, const py_i64* vals, int count) {
    py_newlistn(out, count);
    for (int i = 0; i < count; i++) {
        py_newint(py_list_getitem(out, i), vals[i]);
    }
}

// Build a list from C array of floats
static inline void ph_list_from_floats(py_OutRef out, const py_f64* vals, int count) {
    py_newlistn(out, count);
    for (int i = 0; i < count; i++) {
        py_newfloat(py_list_getitem(out, i), vals[i]);
    }
}

// Build a list from C array of strings
static inline void ph_list_from_strs(py_OutRef out, const char** vals, int count) {
    py_newlistn(out, count);
    for (int i = 0; i < count; i++) {
        py_newstr(py_list_getitem(out, i), vals[i]);
    }
}

// Build a list from C array of bools
static inline void ph_list_from_bools(py_OutRef out, const bool* vals, int count) {
    py_newlistn(out, count);
    for (int i = 0; i < count; i++) {
        py_newbool(py_list_getitem(out, i), vals[i]);
    }
}

/* ============================================================================
 * 9. Debug/Development Helpers
 * ============================================================================
 */

// Print a Python value's repr (for debugging)
static inline void ph_print(py_Ref val) {
    ph_Scope scope = ph_scope_begin();
    if (py_repr(val)) {
        py_Callbacks* cb = py_callbacks();
        if (cb->print) {
            cb->print(py_tostr(py_retval()));
            cb->print("\n");
        }
    }
    ph_scope_end(&scope);
}

// Get string representation (returns fallback on error)
static inline const char* ph_repr(py_Ref val) {
    if (!py_repr(val)) {
        py_clearexc(NULL);
        return "<repr failed>";
    }
    return py_tostr(py_retval());
}

// Get type name of a value
static inline const char* ph_typename(py_Ref val) {
    return py_tpname(py_typeof(val));
}

#ifdef __cplusplus
}
#endif
