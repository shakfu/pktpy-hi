# pktpy_hi.h: Higher-Level Wrapper API Design

A thin, header-only wrapper over pocketpy's C-API that reduces boilerplate while maintaining full interoperability with the low-level API.

**Prefix**: `ph_` (pocketpy high-level)

## Design Principles

1. **Zero overhead**: All functions are `static inline`
2. **Composable**: Both APIs usable together; drop to low-level when needed
3. **Focused**: Address only the highest-friction pain points
4. **Idiomatic C**: Prefer return values over output parameters where possible
5. **Safe by default**: Automatic cleanup and exception handling

## Header Structure

```c
// pktpy_hi.h - Higher-level pocketpy wrapper
#pragma once

#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
```

---

## 1. Scope Management

The most error-prone aspect of pocketpy is manual stack unwinding. This provides automatic cleanup.

### Types

```c
// Represents an execution scope with automatic cleanup
typedef struct {
    py_StackRef unwind_point;
    bool has_exception;
} ph_Scope;
```

### Functions

```c
// Begin a new scope - captures current stack position
static inline ph_Scope ph_scope_begin(void) {
    return (ph_Scope){ .unwind_point = py_peek(0), .has_exception = false };
}

// End scope and handle any pending exception
// Returns true if execution was successful, false if exception occurred
static inline bool ph_scope_end(ph_Scope* scope) {
    if (py_checkexc()) {
        scope->has_exception = true;
        py_clearexc(scope->unwind_point);
        return false;
    }
    return true;
}

// End scope, print exception if any, return success status
static inline bool ph_scope_end_print(ph_Scope* scope) {
    if (py_checkexc()) {
        scope->has_exception = true;
        py_printexc();
        py_clearexc(scope->unwind_point);
        return false;
    }
    return true;
}

// Check if scope had an exception (after ph_scope_end)
static inline bool ph_scope_failed(const ph_Scope* scope) {
    return scope->has_exception;
}
```

### Usage Example

```c
// Before (raw pocketpy):
py_StackRef p0 = py_peek(0);
bool ok = py_exec("1/0", "<eval>", EXEC_MODE, NULL);
if (!ok) {
    py_printexc();
    py_clearexc(p0);
}

// After (with wrapper):
ph_Scope scope = ph_scope_begin();
py_exec("1/0", "<eval>", EXEC_MODE, NULL);
ph_scope_end_print(&scope);
```

---

## 2. Safe Execution Helpers

Combine scope management with common execution patterns.

```c
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
```

---

## 3. Value Creation (Return-by-Value Style)

The `py_OutRef` pattern is awkward. Provide alternatives that use registers internally.

```c
// Create values and return references (uses py_r0 internally)
// These are convenience functions for quick value creation

static inline py_GlobalRef ph_int(py_i64 val) {
    py_newint(py_r0(), val);
    return py_r0();
}

static inline py_GlobalRef ph_float(py_f64 val) {
    py_newfloat(py_r0(), val);
    return py_r0();
}

static inline py_GlobalRef ph_str(const char* val) {
    py_newstr(py_r0(), val);
    return py_r0();
}

static inline py_GlobalRef ph_bool(bool val) {
    py_newbool(py_r0(), val);
    return py_r0();
}

// Create into a specific register (r0-r7)
static inline py_GlobalRef ph_int_r(int reg, py_i64 val) {
    py_newint(py_getreg(reg), val);
    return py_getreg(reg);
}

static inline py_GlobalRef ph_str_r(int reg, const char* val) {
    py_newstr(py_getreg(reg), val);
    return py_getreg(reg);
}
```

---

## 4. Safe Function Calls

Calling Python functions from C is verbose. Simplify common cases.

```c
// Result type for function calls
typedef struct {
    bool ok;           // true if call succeeded
    py_GlobalRef val;  // result (only valid if ok==true)
} ph_Result;

// Call a global function by name with no arguments
static inline ph_Result ph_call0(const char* func_name) {
    ph_Result r = { .ok = false, .val = NULL };
    ph_Scope scope = ph_scope_begin();

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end(&scope);
        return r;
    }

    r.ok = py_call(fn, 0, NULL);
    if (r.ok) r.val = py_retval();
    ph_scope_end(&scope);
    return r;
}

// Call with 1 argument
static inline ph_Result ph_call1(const char* func_name, py_Ref arg0) {
    ph_Result r = { .ok = false, .val = NULL };
    ph_Scope scope = ph_scope_begin();

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end(&scope);
        return r;
    }

    r.ok = py_call(fn, 1, arg0);
    if (r.ok) r.val = py_retval();
    ph_scope_end(&scope);
    return r;
}

// Call with 2 arguments (args must be contiguous in memory)
static inline ph_Result ph_call2(const char* func_name, py_Ref args) {
    ph_Result r = { .ok = false, .val = NULL };
    ph_Scope scope = ph_scope_begin();

    py_ItemRef fn = py_getglobal(py_name(func_name));
    if (!fn) {
        py_exception(tp_NameError, "name '%s' is not defined", func_name);
        ph_scope_end(&scope);
        return r;
    }

    r.ok = py_call(fn, 2, args);
    if (r.ok) r.val = py_retval();
    ph_scope_end(&scope);
    return r;
}

// Call any callable reference with argc arguments
static inline ph_Result ph_call(py_Ref callable, int argc, py_Ref argv) {
    ph_Result r = { .ok = false, .val = NULL };
    ph_Scope scope = ph_scope_begin();

    r.ok = py_call(callable, argc, argv);
    if (r.ok) r.val = py_retval();
    ph_scope_end(&scope);
    return r;
}

// Call a method on an object
static inline ph_Result ph_callmethod0(py_Ref obj, const char* method_name) {
    ph_Result r = { .ok = false, .val = NULL };
    ph_Scope scope = ph_scope_begin();

    py_push(obj);
    if (!py_pushmethod(py_name(method_name))) {
        py_pop();  // remove obj
        py_exception(tp_AttributeError, "object has no method '%s'", method_name);
        ph_scope_end(&scope);
        return r;
    }

    // Stack: [method, obj]
    r.ok = py_vectorcall(0, 0);
    if (r.ok) r.val = py_retval();
    ph_scope_end(&scope);
    return r;
}
```

---

## 5. Value Extraction Helpers

Safe extraction with default values.

```c
// Get int or default (no exception on type mismatch)
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

// Check if value is truthy (handles exceptions)
static inline bool ph_is_truthy(py_Ref val) {
    int result = py_bool(val);
    if (result < 0) {
        py_clearexc(NULL);
        return false;
    }
    return result == 1;
}
```

---

## 6. Simplified Binding

Unified binding approach with clearer semantics.

```c
// Bind a C function to the main module
static inline void ph_def(const char* sig, py_CFunction f) {
    py_GlobalRef main_mod = py_getmodule("__main__");
    py_bind(main_mod, sig, f);
}

// Bind a C function to a named module
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
```

---

## 7. Argument Helpers for Native Functions

Macros to simplify argument extraction in `py_CFunction` implementations.

```c
// Get argument as int with bounds checking
#define PH_ARG_INT(i, var) \
    py_i64 var; \
    do { \
        if (!py_castint(py_arg(i), &var)) return false; \
    } while(0)

// Get argument as float (accepts int or float)
#define PH_ARG_FLOAT(i, var) \
    py_f64 var; \
    do { \
        if (!py_castfloat(py_arg(i), &var)) return false; \
    } while(0)

// Get argument as string
#define PH_ARG_STR(i, var) \
    const char* var; \
    do { \
        if (!py_checkstr(py_arg(i))) return false; \
        var = py_tostr(py_arg(i)); \
    } while(0)

// Get argument as bool
#define PH_ARG_BOOL(i, var) \
    bool var; \
    do { \
        if (!py_checkbool(py_arg(i))) return false; \
        var = py_tobool(py_arg(i)); \
    } while(0)

// Optional argument with default (int)
#define PH_ARG_INT_OPT(i, var, default_val) \
    py_i64 var = (default_val); \
    do { \
        if (argc > (i) && !py_isnil(py_arg(i))) { \
            if (!py_castint(py_arg(i), &var)) return false; \
        } \
    } while(0)

// Return an int result
#define PH_RETURN_INT(val) \
    do { py_newint(py_retval(), (val)); return true; } while(0)

// Return a float result
#define PH_RETURN_FLOAT(val) \
    do { py_newfloat(py_retval(), (val)); return true; } while(0)

// Return a string result
#define PH_RETURN_STR(val) \
    do { py_newstr(py_retval(), (val)); return true; } while(0)

// Return None
#define PH_RETURN_NONE \
    do { py_newnone(py_retval()); return true; } while(0)

// Return a bool result
#define PH_RETURN_BOOL(val) \
    do { py_newbool(py_retval(), (val)); return true; } while(0)
```

### Usage Example

```c
// Before (raw pocketpy):
static bool my_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    py_i64 a, b;
    if (!py_castint(py_arg(0), &a)) return false;
    if (!py_castint(py_arg(1), &b)) return false;
    py_newint(py_retval(), a + b);
    return true;
}

// After (with wrapper):
static bool my_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    PH_ARG_INT(0, a);
    PH_ARG_INT(1, b);
    PH_RETURN_INT(a + b);
}
```

---

## 8. List/Dict Iteration Helpers

```c
// Iterate over a list with a callback
typedef bool (*ph_ListCallback)(int index, py_Ref item, void* ctx);

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

// Build a list from C array of strings
static inline void ph_list_from_strs(py_OutRef out, const char** vals, int count) {
    py_newlistn(out, count);
    for (int i = 0; i < count; i++) {
        py_newstr(py_list_getitem(out, i), vals[i]);
    }
}
```

---

## 9. Debug/Development Helpers

```c
// Print a Python value (for debugging)
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

// Get string representation (caller must not free)
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
```

---

## Complete Header Footer

```c
#ifdef __cplusplus
}
#endif

// End of pktpy_hi.h
```

---

## Summary: What This Wrapper Provides

| Category | Functions | Key Benefit |
|----------|-----------|-------------|
| Scope Management | `ph_scope_begin/end` | Automatic stack cleanup |
| Safe Execution | `ph_exec`, `ph_eval` | One-liner execution with error handling |
| Value Creation | `ph_int`, `ph_str`, etc. | Return-by-value instead of out-params |
| Function Calls | `ph_call0/1/2`, `ph_callmethod0` | Safe calls with result struct |
| Value Extraction | `ph_as_int`, `ph_as_float` | Safe extraction with defaults |
| Binding | `ph_def`, `ph_setglobal` | Simplified function binding |
| Arg Macros | `PH_ARG_INT`, `PH_RETURN_INT` | Reduce native function boilerplate |
| Iteration | `ph_list_foreach` | Callback-based iteration |
| Debug | `ph_print`, `ph_repr` | Quick debugging helpers |

## What This Wrapper Does NOT Do

- Does not hide the stack model (users can still use `py_push`/`py_pop`)
- Does not manage object lifetimes beyond scope cleanup
- Does not provide a C++ API (this is pure C)
- Does not add features not in the underlying API
- Does not replace the low-level API (both work together)

## File Organization

```
pktpy-hi/
  include/
    pktpy_hi.h          # The wrapper header (header-only)
  docs/
    initial-analysis.md # This analysis
    api-design.md       # This design document
  examples/
    basic_usage.c       # Example using the wrapper
  tests/
    test_scope.c        # Test scope management
    test_calls.c        # Test function calls
```
