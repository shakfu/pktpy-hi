# pktpy-hi

An idea for a higher-level header api wrapper for the [pocketpy](https://github.com/pocketpy/pocketpy) embedded Python interpreter.

**Version:** 0.2.0

**Caveat**: this idea was developed with the extensive aid of AI agents.

## Overview

This project provides two header-only wrappers for pocketpy:

- **`pktpy_hi.h`** - C11 wrapper using macros and conventions
- **`pktpy_hi.hpp`** - C++17 wrapper using RAII, templates, and move semantics

Both reduce boilerplate while maintaining full interoperability with the low-level pocketpy API. All three APIs (C wrapper, C++ wrapper, raw pocketpy) can be used together in the same program.

## Rationale

The pocketpy C-API is a low-level, stack-based interface with ~140 functions. While powerful and flexible, it has several pain points:

| Problem | Impact | Wrapper Solution |
|---------|--------|------------------|
| Manual stack management | Easy to corrupt state or forget cleanup | `ph_Scope` with automatic unwinding |
| `py_OutRef` pattern | Non-idiomatic, requires pre-allocated slots | Return-by-value helpers (`ph_tmp_int()`, etc.) |
| Error handling boilerplate | Easy to forget `py_checkexc()` / `py_clearexc()` | Auto-check in `ph_exec()`, `ph_call*()` |
| Verbose argument extraction | 3-4 lines per argument in native functions | `PH_ARG_INT(0, x)` one-liner macros |
| Two binding paradigms | Cognitive load choosing decl vs argc style | Unified `ph_def()` approach |

**Design constraints:**

- **Thin wrapper**: Does not hide the stack model; users can drop to `py_*` when needed
- **Zero overhead**: All functions are `static inline`
- **No new abstractions**: Only simplifies existing patterns, adds no new concepts
- **Header-only**: Single file, no build complexity

## Features

- **Automatic scope management** - Stack unwinding and exception cleanup
- **Safe execution helpers** - One-liner `ph_exec()` / `ph_eval()` with error handling
- **Return-by-value creation** - `ph_tmp_int()`, `ph_tmp_str()` instead of `py_OutRef` pattern
- **Safe function calls** - `ph_call*()` with `ph_Result` struct
- **Argument macros** - `PH_ARG_INT`, `PH_RETURN_INT` for native functions
- **Zero overhead** - All functions are `static inline`

## Quick Start

```c
#include "pktpy_hi.h"

int main(void) {
    py_initialize();

    // Simple execution with automatic error handling
    ph_exec("print('Hello from pktpy-hi!')", "<test>");

    // Evaluate and get result
    if (ph_eval("2 ** 10")) {
        printf("Result: %lld\n", py_toint(py_retval()));
    }

    // Set globals easily
    ph_setglobal("x", ph_tmp_int(42));
    ph_exec("print(f'x = {x}')", "<test>");

    py_finalize();
    return 0;
}
```

## Building

```bash
make          # Build all targets
make test     # Run tests
make clean    # Clean build directory
```

### Requirements

- CMake 3.14+
- C11 compiler for C wrapper (GCC, Clang, or MSVC)
- C++17 compiler for C++ wrapper (GCC 7+, Clang 5+, MSVC 2017+)
- pocketpy 2.1.6 (included in `pocketpy-2.1.6/`)

## API Categories

| Category | Functions | Purpose |
|----------|-----------|---------|
| Scope | `ph_scope_begin`, `ph_scope_end`, `ph_scope_end_print`, `ph_scope_end_raise`, `ph_scope_failed` | Automatic stack cleanup |
| Execution | `ph_exec`, `ph_eval`, `*_in`, `*_raise` variants | Safe code execution |
| Values | `ph_tmp_int`, `ph_tmp_str`, `ph_tmp_float`, `ph_tmp_bool` | Temporary value creation |
| Values (stable) | `ph_int_r`, `ph_str_r`, `ph_float_r`, `ph_bool_r` | Register-backed values |
| Calls | `ph_call0/1/2/3`, `ph_callmethod0/1/2/3` | Function calling |
| Calls (variants) | `*_raise`, `*_r`, `*_r_raise` | Exception propagation / stable storage |
| Extraction | `ph_as_int/float/str/bool`, `ph_is_truthy/_raise`, `ph_is_none`, `ph_is_nil` | Safe value extraction |
| Binding | `ph_def`, `ph_def_in`, `ph_setglobal`, `ph_getglobal`, `ph_module` | C function binding |
| Args | `PH_ARG_INT`, `PH_ARG_STR`, `PH_ARG_REF`, etc. | Argument extraction (with bounds check) |
| Returns | `PH_RETURN_INT`, `PH_RETURN_NONE`, etc. | Return value macros |
| Lists | `ph_list_foreach`, `ph_list_from_ints/floats/strs/bools` | List helpers |
| Debug | `ph_print`, `ph_repr`, `ph_typename` | Debugging utilities |

## Important: Register and Result Lifetime

The wrapper uses pocketpy's global registers for temporary storage. Understanding their lifetime is critical to avoid subtle bugs.

### Value Creation (`ph_tmp_int`, `ph_tmp_str`, etc.)

The `ph_tmp_*` functions all use register `r0`, so each call **overwrites** the previous value. The `tmp` prefix signals that these return temporary values:

```c
// WRONG - b overwrites a
py_GlobalRef a = ph_tmp_int(1);
py_GlobalRef b = ph_tmp_int(2);  // a now points to 2!

// CORRECT - use value immediately (object ref is copied)
ph_setglobal("x", ph_tmp_int(1));
ph_setglobal("y", ph_tmp_int(2));

// CORRECT - use explicit registers for multiple values
py_GlobalRef a = ph_int_r(0, 1);  // store in r0
py_GlobalRef b = ph_int_r(1, 2);  // store in r1 (independent)
```

### Function Call Results (`ph_call*`)

The base `ph_call*` functions return `py_retval()`, which is overwritten by every Python call:

```c
// WRONG - r2.val overwrites r1.val
ph_Result r1 = ph_call0("get_a");
ph_Result r2 = ph_call0("get_b");  // r1.val is now invalid!

// CORRECT - use _r variants for stable storage
ph_Result r1 = ph_call0_r(4, "get_a");  // store in r4
ph_Result r2 = ph_call0_r(5, "get_b");  // store in r5 (independent)
```

### Call Function Variants

Each call function has multiple variants for different use cases:

| Variant | Exception Handling | Result Storage | Use Case |
|---------|-------------------|----------------|----------|
| `ph_call0()` | Print and clear | `py_retval()` (volatile) | Fire-and-forget |
| `ph_call0_raise()` | Keep for caller | `py_retval()` (volatile) | Propagate to Python |
| `ph_call0_r(reg, ...)` | Print and clear | Register (stable) | Need stable result |
| `ph_call0_r_raise(reg, ...)` | Keep for caller | Register (stable) | Both stable + propagate |

The same variants exist for `ph_call1/2/3` and `ph_callmethod0/1/2/3`.

### Available Registers

Registers `r0` through `r7` are available. Convention:

- `r0`: Used by all `ph_tmp_*()` functions (each call overwrites)
- `r1-r7`: Available for user storage via `ph_*_r()` functions

## Native Function Example

The `PH_ARG_*` macros include automatic bounds checking - they verify the argument index is within `argc` before accessing it, raising `TypeError` if not.

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

// After (with pktpy_hi):
static bool my_add(int argc, py_StackRef argv) {
    PH_ARG_INT(0, a);  // Includes bounds check + type check
    PH_ARG_INT(1, b);
    PH_RETURN_INT(a + b);
}
```

Available argument macros:

- `PH_ARG_INT(i, var)` - Required int argument
- `PH_ARG_FLOAT(i, var)` - Required float (accepts int or float)
- `PH_ARG_STR(i, var)` - Required string
- `PH_ARG_BOOL(i, var)` - Required bool
- `PH_ARG_REF(i, var)` - Required raw `py_Ref` (no type check)
- `PH_ARG_*_OPT(i, var, default)` - Optional variants with default values

## C++ Wrapper

The C++ wrapper (`pktpy_hi.hpp`) addresses the same pain points using language features instead of conventions:

| C Version Problem | C++ Solution |
|-------------------|--------------|
| Must remember `ph_scope_end()` | RAII `ph::Scope` - destructor handles cleanup |
| Register aliasing bugs at runtime | Move-only `ph::Value` - compile-time prevention |
| Separate `ph_call0/1/2/3` functions | Overloaded `ph::call()` with type deduction |
| Macro-based argument extraction | Template-based `ph::arg<T>()` with `std::optional` |

### C++ Quick Start

```cpp
#include "pktpy_hi.hpp"

int main() {
    py_initialize();

    // RAII scope - cleanup is automatic
    {
        ph::Scope scope;
        ph::exec("print('Hello from C++!')");
    }  // stack restored here, no explicit end() needed

    // Move-only values prevent aliasing bugs
    auto a = ph::Value::integer(10, 0);  // register 0
    auto b = ph::Value::integer(20, 1);  // register 1
    // auto c = a;  // WON'T COMPILE - prevents aliasing

    // Type-safe function calls
    ph::exec("def add(x, y): return x + y");
    auto result = ph::call("add", a, b);
    if (result) {
        printf("Result: %lld\n", py_toint(result.value()));
    }

    py_finalize();
    return 0;
}
```

### C++ Native Function

```cpp
static bool my_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);

    // Type-safe extraction with std::optional
    auto a = ph::arg<py_i64>(argv, 0);
    auto b = ph::arg<py_i64>(argv, 1);
    if (!a || !b) return false;

    return ph::ret_int(*a + *b);
}
```

## Interoperability

Both APIs work together seamlessly:

```c
// Mix ph_* and py_* functions freely
py_newlist(py_r0());
py_list_append(py_r0(), ph_int_r(1, 100));
py_list_append(py_r0(), ph_int_r(2, 200));
ph_setglobal("my_list", py_r0());
ph_exec("print(my_list)", "<test>");
```

## Project Structure

```text
pktpy-hi/
  include/
    pktpy_hi.h          # C wrapper header (header-only)
    pktpy_hi.hpp        # C++ wrapper header (header-only)
  pocketpy-2.1.6/       # Upstream pocketpy
  examples/
    basic_usage.c       # C usage examples
    basic_usage.cpp     # C++ usage examples
  tests/
    test_*.c            # C test suites
    test_cpp_wrapper.cpp # C++ test suite
  docs/
    c-api-design.md     # C API documentation
    cpp-api-design.md   # C++ API documentation
  CMakeLists.txt        # CMake build
  Makefile              # Frontend makefile
```

## Documentation

- [C API Design](docs/c-api-design.md) - C wrapper API documentation
- [C++ API Design](docs/cpp-api-design.md) - C++ wrapper API documentation
