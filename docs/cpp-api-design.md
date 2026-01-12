# pktpy_hi.hpp: C++ Wrapper API Design

A modern C++17 wrapper over pocketpy's C-API using RAII, templates, and move semantics to provide compile-time safety guarantees.

**Namespace**: `ph`

## Design Principles

1. **Compile-time safety**: Move-only types prevent register aliasing bugs at compile time
2. **RAII everywhere**: Destructors handle cleanup, even on exceptions
3. **Zero-cost abstractions**: Templates and inline functions optimize away
4. **Composable**: Works alongside raw pocketpy API and C wrapper
5. **Modern C++**: Uses `std::optional`, forwarding references, `[[nodiscard]]`

## Header Structure

```cpp
#pragma once

#define PK_IS_PUBLIC_INCLUDE
#include "pocketpy.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <cassert>
#include <utility>

namespace ph {
// All wrapper types and functions
}
```

---

## 1. Exception Handling Policy

Controls how exceptions are handled when a scope ends.

```cpp
enum class ExcPolicy {
    Print,   // Print and clear (fire-and-forget) - default
    Raise,   // Keep for propagation (caller must handle)
    Silent   // Clear silently
};
```

---

## 2. RAII Scope Management

The `Scope` class automatically handles stack unwinding and exception cleanup. Unlike the C version where you must remember to call `ph_scope_end()`, the destructor guarantees cleanup.

### Class Definition

```cpp
class Scope {
public:
    explicit Scope(ExcPolicy policy = ExcPolicy::Print);
    ~Scope();  // Automatic cleanup

    // Non-copyable, non-movable (tied to stack frame)
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    [[nodiscard]] bool ok() const;      // Returns true if no exception
    [[nodiscard]] bool failed() const;  // Returns true if exception occurred
};
```

### Usage Example

```cpp
// C version - must remember to call end:
ph_Scope scope = ph_scope_begin();
py_exec("1/0", "<test>", EXEC_MODE, NULL);
ph_scope_end_print(&scope);  // Easy to forget!

// C++ version - automatic cleanup:
{
    ph::Scope scope;
    py_exec("1/0", "<test>", EXEC_MODE, nullptr);
}  // Destructor handles cleanup, even on exceptions
```

---

## 3. Result Type

A simple result type for operations that can fail. Similar to `std::optional` but with clearer semantics.

```cpp
template<typename T = py_GlobalRef>
class Result {
public:
    Result();                              // Creates failed result
    explicit Result(T val);                // Creates success result

    static Result failure();
    static Result success(T val);

    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] bool ok() const;
    [[nodiscard]] T value() const;         // Asserts if failed
    [[nodiscard]] T value_or(T default_val) const;
    [[nodiscard]] T operator*() const;     // Same as value()
};
```

### Usage Example

```cpp
auto result = ph::eval("2 ** 10");
if (result) {
    printf("Result: %lld\n", py_toint(result.value()));
}

// Or with default:
py_i64 val = py_toint(result.value_or(py_None()));
```

---

## 4. Move-Only Value Type

The `Value` class prevents register aliasing bugs at compile time by being move-only. Copying is deleted, so you can't accidentally create two references to the same register.

### The Problem It Solves

```cpp
// Raw pocketpy - runtime bug:
py_newint(py_r0(), 1);
py_GlobalRef a = py_r0();
py_newint(py_r0(), 2);  // a now points to 2, not 1!

// ph::Value - compile-time prevention:
auto a = ph::Value::integer(1, 0);
auto b = a;  // WON'T COMPILE - copy deleted
auto c = std::move(a);  // OK - explicit transfer
// a is now empty, c owns the value
```

### Class Definition

```cpp
class Value {
public:
    Value();  // Default: empty/nil value

    // Factory functions - each specifies which register to use
    static Value integer(py_i64 val, int reg);
    static Value floating(py_f64 val, int reg);
    static Value string(const char* val, int reg);
    static Value boolean(bool val, int reg);
    static Value wrap(py_Ref r);  // Non-owning wrapper

    // Move semantics
    Value(Value&& other) noexcept;
    Value& operator=(Value&& other) noexcept;

    // Copying deleted - prevents aliasing
    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    // Access
    [[nodiscard]] py_GlobalRef ref() const;
    [[nodiscard]] operator py_Ref() const;
    [[nodiscard]] int reg() const;
    [[nodiscard]] bool valid() const;

    // Type checks
    [[nodiscard]] bool is_int() const;
    [[nodiscard]] bool is_float() const;
    [[nodiscard]] bool is_str() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_none() const;
    [[nodiscard]] bool is_nil() const;

    // Direct extraction (caller must verify type)
    [[nodiscard]] py_i64 to_int() const;
    [[nodiscard]] py_f64 to_float() const;
    [[nodiscard]] const char* to_str() const;
    [[nodiscard]] bool to_bool() const;

    // Safe extraction with defaults
    [[nodiscard]] py_i64 as_int(py_i64 def = 0) const;
    [[nodiscard]] py_f64 as_float(py_f64 def = 0.0) const;
    [[nodiscard]] const char* as_str(const char* def = "") const;
    [[nodiscard]] bool as_bool(bool def = false) const;

    // Type info
    [[nodiscard]] const char* type_name() const;
};
```

### Usage Example

```cpp
// Create values in different registers
auto a = ph::Value::integer(10, 0);   // register 0
auto b = ph::Value::integer(20, 1);   // register 1
auto c = ph::Value::string("hi", 2);  // register 2

// All remain valid simultaneously
printf("%lld + %lld = %lld\n", a.to_int(), b.to_int(), a.to_int() + b.to_int());

// Transfer ownership
auto d = std::move(a);  // a is now empty
assert(!a.valid());
assert(d.valid());

// Wrap existing reference (non-owning)
py_ItemRef global = py_getglobal(py_name("x"));
auto wrapped = ph::Value::wrap(global);
```

---

## 5. Execution Helpers

Safe execution with automatic scope management.

```cpp
// Execute code (returns success/failure)
bool exec(const char* source, const char* filename = "<string>",
          ExcPolicy policy = ExcPolicy::Print);

bool exec_in(const char* source, const char* filename, py_Ref module,
             ExcPolicy policy = ExcPolicy::Print);

// Evaluate expression (returns Result with value)
Result<py_GlobalRef> eval(const char* source,
                          ExcPolicy policy = ExcPolicy::Print);

Result<py_GlobalRef> eval_in(const char* source, py_Ref module,
                             ExcPolicy policy = ExcPolicy::Print);
```

### Usage Example

```cpp
// Simple execution
ph::exec("print('Hello!')");

// With error handling
if (!ph::exec("1/0")) {
    // Exception was printed and cleared
}

// Evaluation
auto result = ph::eval("2 ** 10");
if (result) {
    printf("Result: %lld\n", py_toint(*result));
}

// Propagate exception
auto result = ph::eval("might_fail()", ph::ExcPolicy::Raise);
if (!result && py_matchexc(tp_ValueError)) {
    // Handle ValueError specifically
}
```

---

## 6. Function Calls (Variadic Templates)

Variadic templates provide a unified interface for calling Python functions with any number of arguments (0-4). No need for separate `call0`, `call1`, etc.

### Functions

```cpp
// Call global function by name (0-4 arguments)
Result<py_GlobalRef> call(const char* func_name);

template<typename... Args>
Result<py_GlobalRef> call(const char* func_name, Args&&... args);

// Call any callable with explicit argc/argv
Result<py_GlobalRef> call(py_Ref callable);
Result<py_GlobalRef> call(py_Ref callable, int argc, py_Ref argv);

// Call method on object (0-2 arguments)
Result<py_GlobalRef> call_method(py_Ref obj, const char* method_name);

template<typename A0>
Result<py_GlobalRef> call_method(py_Ref obj, const char* method_name, A0&& a0);

template<typename A0, typename A1>
Result<py_GlobalRef> call_method(py_Ref obj, const char* method_name, A0&& a0, A1&& a1);

// Store result in specific register for stability
template<typename... Args>
Result<py_GlobalRef> call_r(int reg, const char* func_name, Args&&... args);
```

### Usage Example

```cpp
// Define a Python function
ph::exec("def add(a, b, c): return a + b + c");

// Call with Value arguments
auto a = ph::Value::integer(1, 0);
auto b = ph::Value::integer(2, 1);
auto c = ph::Value::integer(3, 2);

auto result = ph::call("add", a, b, c);
if (result) {
    printf("Sum: %lld\n", py_toint(*result));  // 6
}

// Call method
ph::exec("obj = [1, 2, 3]");
auto obj = ph::get_global("obj");
auto len_result = ph::call_method(obj, "__len__");

// Store in register for multiple results
auto r1 = ph::call_r(4, "get_first");
auto r2 = ph::call_r(5, "get_second");
// Both r1.value() and r2.value() remain valid
```

---

## 7. Binding Helpers

```cpp
// Bind C function to __main__
void def(const char* sig, py_CFunction f);

// Bind to specific module (creates if needed)
void def_in(const char* module_path, const char* sig, py_CFunction f);

// Global variable access
void set_global(const char* name, py_Ref val);
void set_global(const char* name, const Value& val);
py_ItemRef get_global(const char* name);

// Module access (creates if needed)
py_GlobalRef module(const char* path);
```

---

## 8. Type-Safe Argument Extraction

Templates with `std::optional` provide type-safe argument extraction in native functions, replacing the C macro approach.

### Functions

```cpp
// Extract typed argument (returns nullopt on type error)
template<typename T>
std::optional<T> arg(py_StackRef argv, int i);

// Supported types: py_i64, py_f64, const char*, bool, py_Ref

// Check if argument should use default
bool arg_is_default(py_StackRef argv, int i);
```

### Usage Example

```cpp
// C version with macros:
static bool my_func(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    PH_ARG_INT(0, a);
    PH_ARG_STR(1, b);
    // ...
}

// C++ version with templates:
static bool my_func(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);

    auto a = ph::arg<py_i64>(argv, 0);
    auto b = ph::arg<const char*>(argv, 1);
    if (!a || !b) return false;  // Type error already raised

    printf("a=%lld, b=%s\n", *a, *b);
    return ph::ret_none();
}

// With defaults using value_or:
auto count = ph::arg<py_i64>(argv, 2).value_or(10);
```

---

## 9. Return Helpers

```cpp
bool ret_int(py_i64 val);
bool ret_float(py_f64 val);
bool ret_str(const char* val);
bool ret_bool(bool val);
bool ret_none();
bool ret(py_Ref val);
bool ret(const Value& val);
```

### Usage Example

```cpp
static bool add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    auto a = ph::arg<py_i64>(argv, 0);
    auto b = ph::arg<py_i64>(argv, 1);
    if (!a || !b) return false;
    return ph::ret_int(*a + *b);
}
```

---

## 10. List Helpers

```cpp
// Iterate with lambda
template<typename Fn>
bool list_foreach(py_Ref list, Fn&& callback);

// Build from initializer list
void list_from_ints(py_OutRef out, std::initializer_list<py_i64> vals);
void list_from_floats(py_OutRef out, std::initializer_list<py_f64> vals);
void list_from_strings(py_OutRef out, std::initializer_list<const char*> vals);

// Build from any container (vector, array, etc.)
template<typename Container>
void list_from(py_OutRef out, const Container& vals);
```

### Usage Example

```cpp
// Lambda iteration
ph::exec("nums = [1, 2, 3, 4, 5]");
auto nums = ph::get_global("nums");

py_i64 sum = 0;
ph::list_foreach(nums, [&sum](int idx, py_Ref item) {
    sum += py_toint(item);
    return true;  // continue iteration
});

// Build from initializer list
ph::list_from_ints(py_r0(), {10, 20, 30});

// Build from std::vector
std::vector<int> vec = {1, 2, 3};
ph::list_from(py_r0(), vec);
```

---

## 11. Debug Helpers

```cpp
void print(py_Ref val);              // Print repr to stdout
const char* repr(py_Ref val);        // Get repr string
const char* type_name(py_Ref val);   // Get type name
```

---

## Summary: C vs C++ Comparison

| Feature | C Version | C++ Version |
|---------|-----------|-------------|
| Scope cleanup | Manual `ph_scope_end()` | RAII destructor |
| Register aliasing | Runtime bugs | Compile-time prevention |
| Function calls | `ph_call0/1/2/3` | Single `ph::call()` template |
| Argument extraction | `PH_ARG_INT` macros | `ph::arg<T>()` templates |
| Optional arguments | `PH_ARG_INT_OPT` | `std::optional::value_or()` |
| Result handling | `ph_Result` struct | `ph::Result<T>` class |
| Error policy | Separate `*_raise` functions | `ExcPolicy` enum parameter |

## What This Wrapper Provides

| Category | Types/Functions | Key Benefit |
|----------|-----------------|-------------|
| Exception Policy | `ExcPolicy` | Configurable error handling |
| Scope Management | `Scope` | RAII cleanup, exception-safe |
| Result Type | `Result<T>` | Clear success/failure semantics |
| Value Type | `Value` | Move-only prevents aliasing bugs |
| Execution | `exec`, `eval` | One-liners with policy support |
| Function Calls | `call<>`, `call_method<>` | Variadic templates |
| Binding | `def`, `set_global`, `module` | Same as C version |
| Arg Extraction | `arg<T>()` | Type-safe with `std::optional` |
| Return Helpers | `ret_int`, `ret_none`, etc. | Cleaner than macros |
| List Helpers | `list_foreach`, `list_from<>` | Lambda and container support |
| Debug | `print`, `repr`, `type_name` | Same as C version |

## File Organization

```text
pktpy-hi/
  include/
    pktpy_hi.h          # C wrapper header
    pktpy_hi.hpp        # C++ wrapper header (this API)
  docs/
    c-api-design.md     # C API documentation
    cpp-api-design.md   # This document
  examples/
    basic_usage.c       # C usage examples
    basic_usage.cpp     # C++ usage examples
  tests/
    test_*.c            # C test suites
    test_cpp_wrapper.cpp # C++ test suite
```
