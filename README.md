# pktpy-hi

A higher-level C wrapper for the [pocketpy](https://github.com/pocketpy/pocketpy) embedded Python interpreter.

## Overview

`pktpy_hi.h` is a thin, header-only wrapper that reduces boilerplate while maintaining full interoperability with the low-level pocketpy API. Both APIs can be used together in the same program.

## Features

- **Automatic scope management** - Stack unwinding and exception cleanup
- **Safe execution helpers** - One-liner `ph_exec()` / `ph_eval()` with error handling
- **Return-by-value creation** - `ph_int()`, `ph_str()` instead of `py_OutRef` pattern
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
    ph_setglobal("x", ph_int(42));
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
- C11 compiler (GCC, Clang, or MSVC)
- pocketpy 2.1.6 (included in `pocketpy-2.1.6/`)

## API Categories

| Category | Functions | Purpose |
|----------|-----------|---------|
| Scope | `ph_scope_begin`, `ph_scope_end` | Automatic stack cleanup |
| Execution | `ph_exec`, `ph_eval` | Safe code execution |
| Values | `ph_int`, `ph_str`, `ph_float`, `ph_bool` | Value creation |
| Calls | `ph_call0/1/2/3`, `ph_callmethod0/1` | Function calling |
| Extraction | `ph_as_int`, `ph_as_str`, `ph_is_truthy` | Safe value extraction |
| Binding | `ph_def`, `ph_setglobal`, `ph_getglobal` | C function binding |
| Args | `PH_ARG_INT`, `PH_ARG_STR`, etc. | Argument extraction macros |
| Returns | `PH_RETURN_INT`, `PH_RETURN_NONE`, etc. | Return value macros |
| Lists | `ph_list_foreach`, `ph_list_from_ints` | List helpers |
| Debug | `ph_print`, `ph_repr`, `ph_typename` | Debugging utilities |

## Native Function Example

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
    PY_CHECK_ARGC(2);
    PH_ARG_INT(0, a);
    PH_ARG_INT(1, b);
    PH_RETURN_INT(a + b);
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

```
pktpy-hi/
  include/
    pktpy_hi.h          # The wrapper header (header-only)
  pocketpy-2.1.6/       # Upstream pocketpy
  examples/
    basic_usage.c       # Usage examples
  tests/
    test_*.c            # Test suites
  docs/
    initial-analysis.md # Design rationale
    api-design.md       # API documentation
  CMakeLists.txt        # CMake build
  Makefile              # Frontend makefile
```

## Documentation

- [API Design](docs/api-design.md) - Detailed API documentation
- [Initial Analysis](docs/initial-analysis.md) - Design rationale

## License

MIT (same as pocketpy)
