# pktpy_hi.h: Higher-Level Wrapper API Design

A thin, header-only wrapper over pocketpy's C-API that reduces boilerplate while maintaining full interoperability with the low-level API.

**Prefix**: `ph_` (pocketpy high-level)

## Design Principles

1. **Zero overhead**: All functions are `static inline`
2. **Composable**: Both APIs usable together; drop to low-level when needed
3. **Focused**: Address only the highest-friction pain points
4. **Idiomatic C**: Prefer return values over output parameters where possible
5. **Safe by default**: Automatic cleanup and exception handling (print diagnostics)

## Header Structure

```c
// pktpy_hi.h - Higher-level pocketpy wrapper
#pragma once

/* Version information */
#define PH_VERSION_MAJOR 0
#define PH_VERSION_MINOR 2
#define PH_VERSION_PATCH 0
#define PH_VERSION_STRING "0.2.0"

#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"
#include <stdbool.h>
#include <stddef.h>  /* for ptrdiff_t */

#ifdef __cplusplus
extern "C" {
#endif
```

---

## 0. Constants and Internal Helpers

```c
// Maximum number of registers available (r0-r7)
#define PH_MAX_REG 8

// Internal: validate register index, returns true if valid
static inline bool ph__check_reg(int reg) {
    return reg >= 0 && reg < PH_MAX_REG;
}
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

Three variants are provided for ending a scope:

- `ph_scope_end`: Clear exception silently, restore stack
- `ph_scope_end_print`: Print exception, clear, restore stack (recommended default)
- `ph_scope_end_raise`: Restore stack but keep exception for propagation

```c
// Begin a new scope - captures current stack position
static inline ph_Scope ph_scope_begin(void);

// End scope and handle any pending exception
// Clears exception silently, restores stack on success
// Returns true if execution was successful, false if exception occurred
static inline bool ph_scope_end(ph_Scope* scope);

// End scope, print exception if any, restore stack, return success status
// This is the recommended default for fire-and-forget operations
static inline bool ph_scope_end_print(ph_Scope* scope);

// End scope, restore stack, but keep exception for propagation
// Returns true if successful, false if exception (exception NOT cleared)
// Use this when caller needs to inspect or re-raise the exception
static inline bool ph_scope_end_raise(ph_Scope* scope);

// Check if scope had an exception (after ph_scope_end)
static inline bool ph_scope_failed(const ph_Scope* scope);
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

Two variants are provided:

- `ph_exec`/`ph_eval`: Print and clear exceptions (fire-and-forget)
- `ph_exec_raise`/`ph_eval_raise`: Keep exceptions for propagation

```c
// Execute Python code with automatic exception handling
// Returns true on success, false on exception (printed and cleared)
static inline bool ph_exec(const char* source, const char* filename);

// Execute in a specific module
static inline bool ph_exec_in(const char* source, const char* filename, py_Ref module);

// Evaluate expression, result in py_retval()
// Returns true on success
static inline bool ph_eval(const char* source);

// Evaluate expression in a specific module, result in py_retval()
static inline bool ph_eval_in(const char* source, py_Ref module);

// --- Propagating variants (exceptions NOT cleared) ---

// Execute Python code, propagating any exception
// Returns true on success, false on exception (use py_checkexc() to inspect)
static inline bool ph_exec_raise(const char* source, const char* filename);

// Execute in a specific module, propagating any exception
static inline bool ph_exec_in_raise(const char* source, const char* filename, py_Ref module);

// Evaluate expression, propagating any exception
// Result in py_retval() on success
static inline bool ph_eval_raise(const char* source);

// Evaluate in a specific module, propagating any exception
static inline bool ph_eval_in_raise(const char* source, py_Ref module);
```

---

## 3. Value Creation (Return-by-Value Style)

The `py_OutRef` pattern is awkward. Provide alternatives that use registers internally.

**IMPORTANT: Register Lifetime Rules**

- `ph_tmp_int()`, `ph_tmp_str()`, `ph_tmp_float()`, `ph_tmp_bool()` all use register r0
- Each call OVERWRITES the previous value in r0
- The returned pointer is only valid until the next `ph_*` or `py_*` call

```c
// WRONG - second call invalidates first result:
py_GlobalRef a = ph_tmp_int(1);
py_GlobalRef b = ph_tmp_int(2);  // a now points to 2!

// CORRECT - use immediately:
ph_setglobal("x", ph_tmp_int(1));
ph_setglobal("y", ph_tmp_int(2));

// CORRECT - use explicit registers for multiple values:
py_GlobalRef a = ph_int_r(0, 1);
py_GlobalRef b = ph_int_r(1, 2);  // a and b are independent
```

### Functions

```c
// Create values using py_r0() - convenience for quick value creation
// WARNING: Returns r0 which is overwritten by subsequent ph_* calls
static inline py_GlobalRef ph_tmp_int(py_i64 val);
static inline py_GlobalRef ph_tmp_float(py_f64 val);
static inline py_GlobalRef ph_tmp_str(const char* val);
static inline py_GlobalRef ph_tmp_bool(bool val);

// Create into a specific register (0-7)
// Returns NULL if register index is invalid
static inline py_GlobalRef ph_int_r(int reg, py_i64 val);
static inline py_GlobalRef ph_float_r(int reg, py_f64 val);
static inline py_GlobalRef ph_str_r(int reg, const char* val);
static inline py_GlobalRef ph_bool_r(int reg, bool val);
```

---

## 4. Safe Function Calls

Calling Python functions from C is verbose. Simplify common cases.

Three variants are provided:

- `ph_call*`: Print and clear exceptions (like `ph_exec`), result in volatile `py_retval()`
- `ph_call*_raise`: Keep exceptions for propagation (caller must handle)
- `ph_call*_r`: Copy result to specified register (0-7) for stable storage

**WARNING**: The base `ph_call*` variants return `py_retval()` which is overwritten by every subsequent Python call. Use `ph_call*_r()` when you need to keep multiple results or pass results to other Python calls.

### Types

```c
// Result type for function calls
typedef struct {
    bool ok;           // true if call succeeded
    py_GlobalRef val;  // result (only valid if ok==true)
} ph_Result;
```

### Base Variants (print and clear exceptions)

```c
// Call a global function by name with no arguments
static inline ph_Result ph_call0(const char* func_name);

// Call a global function with 1 argument
static inline ph_Result ph_call1(const char* func_name, py_Ref arg0);

// Call a global function with 2 arguments (args must be contiguous)
static inline ph_Result ph_call2(const char* func_name, py_Ref args);

// Call a global function with 3 arguments (args must be contiguous)
static inline ph_Result ph_call3(const char* func_name, py_Ref args);

// Call any callable reference with argc arguments
static inline ph_Result ph_call(py_Ref callable, int argc, py_Ref argv);

// Call a method on an object with no arguments
static inline ph_Result ph_callmethod0(py_Ref obj, const char* method_name);

// Call a method on an object with 1 argument
static inline ph_Result ph_callmethod1(py_Ref obj, const char* method_name, py_Ref arg0);

// Call a method on an object with 2 arguments
static inline ph_Result ph_callmethod2(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1);

// Call a method on an object with 3 arguments
static inline ph_Result ph_callmethod3(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2);
```

### Propagating Variants (keep exceptions)

```c
// Call a global function by name with no arguments, propagating exceptions
static inline ph_Result ph_call0_raise(const char* func_name);
static inline ph_Result ph_call1_raise(const char* func_name, py_Ref arg0);
static inline ph_Result ph_call2_raise(const char* func_name, py_Ref args);
static inline ph_Result ph_call3_raise(const char* func_name, py_Ref args);
static inline ph_Result ph_call_raise(py_Ref callable, int argc, py_Ref argv);
static inline ph_Result ph_callmethod0_raise(py_Ref obj, const char* method_name);
static inline ph_Result ph_callmethod1_raise(py_Ref obj, const char* method_name, py_Ref arg0);
static inline ph_Result ph_callmethod2_raise(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1);
static inline ph_Result ph_callmethod3_raise(py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2);
```

### Register-Backed Variants (stable result storage)

All `_r` variants validate register index and return `ok=false` if invalid.

```c
// Call a global function, copy result to register
static inline ph_Result ph_call0_r(int reg, const char* func_name);
static inline ph_Result ph_call1_r(int reg, const char* func_name, py_Ref arg0);
static inline ph_Result ph_call2_r(int reg, const char* func_name, py_Ref args);
static inline ph_Result ph_call3_r(int reg, const char* func_name, py_Ref args);
static inline ph_Result ph_call_r(int reg, py_Ref callable, int argc, py_Ref argv);
static inline ph_Result ph_callmethod0_r(int reg, py_Ref obj, const char* method_name);
static inline ph_Result ph_callmethod1_r(int reg, py_Ref obj, const char* method_name, py_Ref arg0);
static inline ph_Result ph_callmethod2_r(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1);
static inline ph_Result ph_callmethod3_r(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2);
```

### Register-Backed + Propagating Variants (stable storage + keep exceptions)

All `_r_raise` variants combine stable register storage with exception propagation.

```c
static inline ph_Result ph_call0_r_raise(int reg, const char* func_name);
static inline ph_Result ph_call1_r_raise(int reg, const char* func_name, py_Ref arg0);
static inline ph_Result ph_call2_r_raise(int reg, const char* func_name, py_Ref args);
static inline ph_Result ph_call3_r_raise(int reg, const char* func_name, py_Ref args);
static inline ph_Result ph_call_r_raise(int reg, py_Ref callable, int argc, py_Ref argv);
static inline ph_Result ph_callmethod0_r_raise(int reg, py_Ref obj, const char* method_name);
static inline ph_Result ph_callmethod1_r_raise(int reg, py_Ref obj, const char* method_name, py_Ref arg0);
static inline ph_Result ph_callmethod2_r_raise(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1);
static inline ph_Result ph_callmethod3_r_raise(int reg, py_Ref obj, const char* method_name, py_Ref arg0, py_Ref arg1, py_Ref arg2);
```

### Usage Example

```c
// Fire-and-forget (exceptions printed automatically)
ph_Result r = ph_call0("my_function");
if (r.ok) {
    printf("Result: %lld\n", py_toint(r.val));
}

// Need to handle exception programmatically
ph_Result r = ph_call0_raise("my_function");
if (!r.ok && py_matchexc(tp_ValueError)) {
    // Handle ValueError specifically
    py_clearexc(NULL);
}

// Need multiple stable results
ph_Result r1 = ph_call0_r(4, "get_a");  // Store in r4
ph_Result r2 = ph_call0_r(5, "get_b");  // Store in r5
// Both r1.val and r2.val remain valid
```

---

## 5. Value Extraction Helpers

Safe extraction with default values (no exceptions on type mismatch).

```c
// Get int or default
static inline py_i64 ph_as_int(py_Ref val, py_i64 default_val);

// Get float or default (accepts int or float)
static inline py_f64 ph_as_float(py_Ref val, py_f64 default_val);

// Get string or default
static inline const char* ph_as_str(py_Ref val, const char* default_val);

// Get bool or default
static inline bool ph_as_bool(py_Ref val, bool default_val);

// Check if value is truthy (handles exceptions internally)
static inline bool ph_is_truthy(py_Ref val);

// Check if value is truthy (propagates exceptions)
// Returns: 1 for truthy, 0 for falsy, -1 for error (exception set)
static inline int ph_is_truthy_raise(py_Ref val);

// Check if value is None
static inline bool ph_is_none(py_Ref val);

// Check if value is nil (invalid/unset)
static inline bool ph_is_nil(py_Ref val);
```

---

## 6. Simplified Binding

Unified binding approach with clearer semantics.

```c
// Bind a C function to the main module
static inline void ph_def(const char* sig, py_CFunction f);

// Bind a C function to a named module (creates module if needed)
static inline void ph_def_in(const char* module_path, const char* sig, py_CFunction f);

// Set a global variable in __main__
static inline void ph_setglobal(const char* name, py_Ref val);

// Get a global variable from __main__ (returns NULL if not found)
static inline py_ItemRef ph_getglobal(const char* name);

// Get or create a module
static inline py_GlobalRef ph_module(const char* path);
```

---

## 7. Argument Helpers for Native Functions

Macros to simplify argument extraction in `py_CFunction` implementations.

### Required Arguments

```c
// Get argument as int with type checking
#define PH_ARG_INT(i, var)

// Get argument as float (accepts int or float)
#define PH_ARG_FLOAT(i, var)

// Get argument as string
#define PH_ARG_STR(i, var)

// Get argument as bool
#define PH_ARG_BOOL(i, var)

// Get raw py_Ref argument (no type checking)
#define PH_ARG_REF(i, var)
```

### Optional Arguments

```c
// Helper to check if argument should use default (nil or None)
#define PH_ARG_IS_DEFAULT(i)

// Optional int argument with default value
#define PH_ARG_INT_OPT(i, var, default_val)

// Optional float argument with default value
#define PH_ARG_FLOAT_OPT(i, var, default_val)

// Optional string argument with default value
#define PH_ARG_STR_OPT(i, var, default_val)

// Optional bool argument with default value
#define PH_ARG_BOOL_OPT(i, var, default_val)
```

### Return Helpers

```c
// Return an int result
#define PH_RETURN_INT(val)

// Return a float result
#define PH_RETURN_FLOAT(val)

// Return a string result
#define PH_RETURN_STR(val)

// Return a bool result
#define PH_RETURN_BOOL(val)

// Return None
#define PH_RETURN_NONE

// Return an existing reference
#define PH_RETURN(ref)
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

// With optional argument:
static bool greet(int argc, py_StackRef argv) {
    PH_ARG_STR(0, name);
    PH_ARG_STR_OPT(1, greeting, "Hello");
    // greeting defaults to "Hello" if not provided or None
    printf("%s, %s!\n", greeting, name);
    PH_RETURN_NONE;
}
```

---

## 8. List Helpers

```c
typedef bool (*ph_ListCallback)(int index, py_Ref item, void* ctx);

// Iterate over a list with a callback
static inline bool ph_list_foreach(py_Ref list, ph_ListCallback cb, void* ctx);

// Build a list from C array of ints
static inline void ph_list_from_ints(py_OutRef out, const py_i64* vals, int count);

// Build a list from C array of floats
static inline void ph_list_from_floats(py_OutRef out, const py_f64* vals, int count);

// Build a list from C array of strings
static inline void ph_list_from_strs(py_OutRef out, const char** vals, int count);

// Build a list from C array of bools
static inline void ph_list_from_bools(py_OutRef out, const bool* vals, int count);
```

---

## 9. Debug/Development Helpers

```c
// Print a Python value's repr (for debugging)
static inline void ph_print(py_Ref val);

// Get string representation (returns fallback on error)
static inline const char* ph_repr(py_Ref val);

// Get type name of a value
static inline const char* ph_typename(py_Ref val);
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
| Constants | `PH_MAX_REG`, `PH_VERSION_*` | Register bounds, version info |
| Scope Management | `ph_scope_begin/end/end_print/end_raise` | Automatic stack cleanup |
| Safe Execution | `ph_exec`, `ph_eval`, `*_in`, `*_raise` variants | One-liner execution with error handling |
| Value Creation | `ph_tmp_int/float/str/bool`, `ph_*_r` variants | Return-by-value instead of out-params |
| Function Calls | `ph_call0/1/2/3`, `ph_callmethod0/1/2/3`, `*_raise`, `*_r`, `*_r_raise` | Safe calls with result struct |
| Value Extraction | `ph_as_int/float/str/bool`, `ph_is_truthy/_raise`, `ph_is_none/nil` | Safe extraction with defaults |
| Binding | `ph_def`, `ph_def_in`, `ph_setglobal`, `ph_getglobal`, `ph_module` | Simplified function binding |
| Arg Macros | `PH_ARG_INT/FLOAT/STR/BOOL/REF`, `PH_ARG_*_OPT`, `PH_RETURN_*` | Reduce native function boilerplate |
| List Helpers | `ph_list_foreach`, `ph_list_from_ints/floats/strs/bools` | List creation and iteration |
| Debug | `ph_print`, `ph_repr`, `ph_typename` | Quick debugging helpers |

## What This Wrapper Does NOT Do

- Does not hide the stack model (users can still use `py_push`/`py_pop`)
- Does not manage object lifetimes beyond scope cleanup
- Does not add features not in the underlying API
- Does not replace the low-level API (both work together)

**Note:** A separate C++ wrapper (`pktpy_hi.hpp`) is also provided with RAII, templates, and move semantics.

## File Organization

```text
pktpy-hi/
  include/
    pktpy_hi.h          # C wrapper header (header-only)
    pktpy_hi.hpp        # C++ wrapper header (header-only)
  docs/
    c-api-design.md     # This document (C API)
    cpp-api-design.md   # C++ API documentation
  examples/
    basic_usage.c       # C usage examples
    basic_usage.cpp     # C++ usage examples
  tests/
    test_scope.c        # Test scope management
    test_calls.c        # Test function calls
    test_registers.c    # Test register bounds checking
    test_cpp_wrapper.cpp # C++ wrapper tests
    ...
```
