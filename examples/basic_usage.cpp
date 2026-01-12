/*
 * basic_usage.cpp - Demonstrates C++ pktpy_hi.hpp wrapper usage
 *
 * Compile: c++ -std=c++17 -o basic_usage_cpp basic_usage.cpp \
 *          -I../include -I../pocketpy-2.1.6 ../pocketpy-2.1.6/pocketpy.c -lm
 */

#include "pktpy_hi.hpp"
#include <cstdio>

// Example native function using type-safe argument extraction
static bool my_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);

    // Type-safe extraction with std::optional
    auto a = ph::arg<py_i64>(argv, 0);
    auto b = ph::arg<py_i64>(argv, 1);
    if (!a || !b) return false;

    return ph::ret_int(*a + *b);
}

// Example with string argument
static bool my_greet(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(1);

    auto name = ph::arg<const char*>(argv, 0);
    if (!name) return false;

    printf("Hello, %s!\n", *name);
    return ph::ret_none();
}

// Example showing optional arguments with defaults
static bool my_power(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);

    auto base = ph::arg<py_i64>(argv, 0);
    if (!base) return false;

    // Second arg with default value
    py_i64 exp = ph::arg<py_i64>(argv, 1).value_or(2);

    py_i64 result = 1;
    for (py_i64 i = 0; i < exp; i++) {
        result *= *base;
    }
    return ph::ret_int(result);
}

int main() {
    py_initialize();

    // --- RAII Scope Management ---
    // The key advantage: cleanup is automatic, even on early returns
    printf("=== RAII Scope Management ===\n");
    {
        ph::Scope scope;  // Captures stack position
        py_exec("x = 1 + 2", "<test>", EXEC_MODE, nullptr);
        if (scope.ok()) {
            printf("Execution succeeded\n");
        }
    }  // Stack automatically restored here - no explicit end call needed

    // --- Safe Execution ---
    printf("\n=== Safe Execution ===\n");
    ph::exec("print('Hello from ph::exec!')");

    // Exception handling - automatically printed and cleared
    printf("\n=== Exception Handling ===\n");
    if (!ph::exec("result = 1 / 0")) {
        printf("(Exception was caught and printed above)\n");
    }

    // --- Move-Only Value Type ---
    // Prevents the register aliasing bug at compile time
    printf("\n=== Move-Only Value Type ===\n");

    // Each Value owns a specific register
    auto val_a = ph::Value::integer(42, 0);  // register 0
    auto val_b = ph::Value::string("hello", 1);  // register 1

    ph::set_global("my_int", val_a);
    ph::set_global("my_str", val_b);
    ph::exec("print(f'my_int = {my_int}, my_str = {my_str}')");

    // Multiple independent values - no aliasing possible
    auto x = ph::Value::integer(10, 2);
    auto y = ph::Value::integer(20, 3);
    ph::set_global("x", x);
    ph::set_global("y", y);
    ph::exec("print(f'x + y = {x + y}')");

    // Move semantics - explicit ownership transfer
    auto z = std::move(x);  // x is now empty, z owns the value
    printf("After move: z.valid()=%d, x.valid()=%d\n", z.valid(), x.valid());

    // --- Function Binding ---
    printf("\n=== Function Binding ===\n");
    ph::def("my_add(a, b)", my_add);
    ph::def("greet(name)", my_greet);
    ph::def("power(base, exp)", my_power);

    ph::exec("print(f'my_add(3, 4) = {my_add(3, 4)}')");
    ph::exec("greet('World')");
    ph::exec("print(f'power(2, 8) = {power(2, 8)}')");

    // --- Calling Python from C++ ---
    printf("\n=== Calling Python from C++ ===\n");
    ph::exec("def double(x): return x * 2");

    // Using the call() function with Value arguments
    auto arg = ph::Value::integer(21, 0);
    auto result = ph::call("double", arg);
    if (result) {
        printf("double(21) = %lld\n", py_toint(result.value()));
    }

    // Call with multiple arguments
    ph::exec("def add3(a, b, c): return a + b + c");
    auto r1 = ph::Value::integer(10, 0);
    auto r2 = ph::Value::integer(20, 1);
    auto r3 = ph::Value::integer(30, 2);
    auto sum_result = ph::call("add3", r1, r2, r3);
    if (sum_result) {
        printf("add3(10, 20, 30) = %lld\n", py_toint(sum_result.value()));
    }

    // --- Result Type ---
    printf("\n=== Result Type ===\n");
    auto eval_result = ph::eval("2 ** 10");
    if (eval_result) {
        printf("2 ** 10 = %lld\n", py_toint(eval_result.value()));
    }

    // Using value_or for defaults
    auto bad_result = ph::eval("undefined_var");  // Will fail
    printf("bad_result.ok() = %d\n", bad_result.ok());

    // --- Value Extraction with Type Safety ---
    printf("\n=== Value Extraction ===\n");
    auto my_val = ph::Value::integer(12345, 0);

    // Type-checked extraction with defaults
    printf("as_int: %lld\n", my_val.as_int(0));
    printf("as_float: %.2f\n", my_val.as_float(0.0));
    printf("as_str: %s\n", my_val.as_str("(not a string)"));

    // Type checking
    printf("is_int: %d, is_str: %d\n", my_val.is_int(), my_val.is_str());
    printf("type_name: %s\n", my_val.type_name());

    // --- Method Calls ---
    printf("\n=== Method Calls ===\n");
    ph::exec("class Counter:\n"
             "    def __init__(self):\n"
             "        self.value = 0\n"
             "    def increment(self):\n"
             "        self.value += 1\n"
             "        return self.value\n"
             "    def add(self, n):\n"
             "        self.value += n\n"
             "        return self.value\n"
             "counter = Counter()");

    auto counter = ph::get_global("counter");
    if (counter) {
        auto r = ph::call_method(counter, "increment");
        if (r) printf("counter.increment() = %lld\n", py_toint(r.value()));

        auto add_arg = ph::Value::integer(5, 0);
        r = ph::call_method(counter, "add", add_arg);
        if (r) printf("counter.add(5) = %lld\n", py_toint(r.value()));
    }

    // --- List Helpers ---
    printf("\n=== List Helpers ===\n");
    ph::list_from_ints(py_r0(), {1, 2, 3, 4, 5});
    ph::set_global("nums", py_r0());
    ph::exec("print(f'nums = {nums}, sum = {sum(nums)}')");

    // Iterate with lambda
    ph::exec("items = [10, 20, 30]");
    auto items = ph::get_global("items");
    if (items) {
        printf("Iterating items: ");
        ph::list_foreach(items, [](int idx, py_Ref item) {
            printf("[%d]=%lld ", idx, py_toint(item));
            return true;
        });
        printf("\n");
    }

    // --- Debug Helpers ---
    printf("\n=== Debug Helpers ===\n");
    ph::exec("debug_obj = {'key': [1, 2, 3]}");
    auto debug_obj = ph::get_global("debug_obj");
    if (debug_obj) {
        printf("repr: %s\n", ph::repr(debug_obj));
        printf("type: %s\n", ph::type_name(debug_obj));
    }

    // --- Interoperability ---
    // Can freely mix ph:: functions with py_* C API
    printf("\n=== Interoperability with C API ===\n");
    py_newlist(py_r0());
    py_list_append(py_r0(), ph::Value::integer(100, 1).ref());
    py_list_append(py_r0(), ph::Value::integer(200, 2).ref());
    py_list_append(py_r0(), ph::Value::integer(300, 3).ref());
    ph::set_global("mixed_list", py_r0());
    ph::exec("print(f'mixed_list = {mixed_list}')");

    printf("\nAll examples completed.\n");

    py_finalize();
    return 0;
}
