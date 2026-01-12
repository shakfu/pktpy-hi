/*
 * test_cpp_wrapper.cpp - Tests for the C++ pktpy_hi.hpp wrapper
 *
 * Tests RAII scope management, move-only Value type, variadic call<>(),
 * and type-safe argument extraction.
 */

#include "pktpy_hi.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name()
#define RUN_TEST(name) do { \
    printf("  %s... ", #name); \
    test_##name(); \
    printf("ok\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_STREQ(a, b) ASSERT(strcmp((a), (b)) == 0)

// ============================================================================
// Scope Tests
// ============================================================================

TEST(scope_basic) {
    // Scope should capture and restore stack position
    py_StackRef before = py_peek(0);
    {
        ph::Scope scope;
        py_push(py_None());  // Push something
        ASSERT(py_peek(0) != before);  // Stack changed
    }
    // After scope ends, stack should be restored
    ASSERT_EQ(py_peek(0), before);
}

TEST(scope_exception_print) {
    // Exception should be caught and cleared
    {
        ph::Scope scope(ph::ExcPolicy::Print);
        py_exec("raise ValueError('test')", "<test>", EXEC_MODE, nullptr);
        ASSERT(scope.failed());
    }
    // Exception should be cleared after scope
    ASSERT(!py_checkexc());
}

TEST(scope_exception_raise) {
    // Exception should be kept for propagation
    {
        ph::Scope scope(ph::ExcPolicy::Raise);
        py_exec("raise ValueError('test')", "<test>", EXEC_MODE, nullptr);
        ASSERT(scope.failed());
    }
    // Exception should still be pending
    ASSERT(py_checkexc());
    py_clearexc(nullptr);  // Clean up
}

TEST(scope_ok_check) {
    {
        ph::Scope scope;
        py_exec("x = 42", "<test>", EXEC_MODE, nullptr);
        ASSERT(scope.ok());
    }
}

// ============================================================================
// Value Tests
// ============================================================================

TEST(value_integer) {
    auto v = ph::Value::integer(42, 0);
    ASSERT(v.valid());
    ASSERT(v.is_int());
    ASSERT_EQ(v.to_int(), 42);
    ASSERT_EQ(v.reg(), 0);
}

TEST(value_float) {
    auto v = ph::Value::floating(3.14, 1);
    ASSERT(v.valid());
    ASSERT(v.is_float());
    ASSERT(v.to_float() > 3.13 && v.to_float() < 3.15);
}

TEST(value_string) {
    auto v = ph::Value::string("hello", 2);
    ASSERT(v.valid());
    ASSERT(v.is_str());
    ASSERT_STREQ(v.to_str(), "hello");
}

TEST(value_boolean) {
    auto v = ph::Value::boolean(true, 3);
    ASSERT(v.valid());
    ASSERT(v.is_bool());
    ASSERT(v.to_bool());
}

TEST(value_move) {
    auto a = ph::Value::integer(100, 0);
    ASSERT(a.valid());

    auto b = std::move(a);
    ASSERT(b.valid());
    ASSERT_EQ(b.to_int(), 100);
    ASSERT(!a.valid());  // a is now empty
}

TEST(value_different_registers) {
    // Key test: multiple values don't clobber each other
    auto a = ph::Value::integer(1, 0);
    auto b = ph::Value::integer(2, 1);
    auto c = ph::Value::integer(3, 2);

    ASSERT_EQ(a.to_int(), 1);
    ASSERT_EQ(b.to_int(), 2);
    ASSERT_EQ(c.to_int(), 3);
}

TEST(value_safe_extraction) {
    auto v = ph::Value::integer(42, 0);
    ASSERT_EQ(v.as_int(0), 42);
    ASSERT_EQ(v.as_float(0.0), 42.0);  // int converts to float
    ASSERT_STREQ(v.as_str("default"), "default");  // wrong type, use default
}

TEST(value_wrap) {
    py_newint(py_r0(), 999);
    auto v = ph::Value::wrap(py_r0());
    ASSERT(v.valid());
    ASSERT_EQ(v.to_int(), 999);
    ASSERT_EQ(v.reg(), -1);  // non-owning
}

// ============================================================================
// Execution Tests
// ============================================================================

TEST(exec_simple) {
    ASSERT(ph::exec("x = 1 + 2"));
    auto x = ph::get_global("x");
    ASSERT(x != nullptr);
    ASSERT_EQ(py_toint(x), 3);
}

TEST(exec_error) {
    ASSERT(!ph::exec("1/0"));  // ZeroDivisionError
    ASSERT(!py_checkexc());    // Should be cleared (Print policy)
}

TEST(eval_simple) {
    auto result = ph::eval("2 ** 10");
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 1024);
}

TEST(eval_error) {
    auto result = ph::eval("undefined_variable");
    ASSERT(!result.ok());
}

// ============================================================================
// Call Tests
// ============================================================================

TEST(call_builtin_no_args) {
    ph::exec("def get_answer(): return 42");
    auto result = ph::call("get_answer");
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 42);
}

TEST(call_builtin_with_args) {
    ph::exec("def add(a, b): return a + b");
    auto a = ph::Value::integer(10, 0);
    auto b = ph::Value::integer(20, 1);
    auto result = ph::call("add", a, b);
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 30);
}

TEST(call_method) {
    ph::exec("class Foo:\n    def greet(self): return 'hello'");
    ph::exec("obj = Foo()");
    auto obj = ph::get_global("obj");
    ASSERT(obj != nullptr);

    auto result = ph::call_method(obj, "greet");
    ASSERT(result.ok());
    ASSERT_STREQ(py_tostr(result.value()), "hello");
}

TEST(call_method_with_arg) {
    ph::exec("class Bar:\n    def double(self, x): return x * 2");
    ph::exec("bar = Bar()");
    auto bar = ph::get_global("bar");

    auto arg = ph::Value::integer(21, 0);
    auto result = ph::call_method(bar, "double", arg);
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 42);
}

TEST(call_register_result) {
    ph::exec("def make_list(): return [1, 2, 3]");
    auto r1 = ph::call_r(4, "make_list");
    auto r2 = ph::call_r(5, "make_list");

    ASSERT(r1.ok());
    ASSERT(r2.ok());
    // Both should be valid and independent
    ASSERT_EQ(py_list_len(r1.value()), 3);
    ASSERT_EQ(py_list_len(r2.value()), 3);
}

TEST(call_error) {
    auto result = ph::call("nonexistent_function");
    ASSERT(!result.ok());
}

// ============================================================================
// Binding Tests
// ============================================================================

static bool native_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);

    auto a = ph::arg<py_i64>(argv, 0);
    auto b = ph::arg<py_i64>(argv, 1);
    if (!a || !b) return false;

    return ph::ret_int(*a + *b);
}

TEST(binding_def) {
    ph::def("native_add(a, b)", native_add);
    auto result = ph::eval("native_add(100, 200)");
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 300);
}

TEST(binding_set_get_global) {
    auto v = ph::Value::integer(12345, 0);
    ph::set_global("test_var", v);

    auto retrieved = ph::get_global("test_var");
    ASSERT(retrieved != nullptr);
    ASSERT_EQ(py_toint(retrieved), 12345);
}

// ============================================================================
// Argument Extraction Tests
// ============================================================================

static bool arg_test_func(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(4);

    auto i = ph::arg<py_i64>(argv, 0);
    auto f = ph::arg<py_f64>(argv, 1);
    auto s = ph::arg<const char*>(argv, 2);
    auto b = ph::arg<bool>(argv, 3);

    if (!i || !f || !s || !b) return false;

    // Return tuple-like string showing we got everything
    char buf[256];
    snprintf(buf, sizeof(buf), "%lld,%.2f,%s,%s",
             (long long)*i, *f, *s, *b ? "true" : "false");
    return ph::ret_str(buf);
}

TEST(arg_extraction) {
    ph::def("arg_test(i, f, s, b)", arg_test_func);
    auto result = ph::eval("arg_test(42, 3.14, 'hello', True)");
    ASSERT(result.ok());
    ASSERT_STREQ(py_tostr(result.value()), "42,3.14,hello,true");
}

TEST(arg_type_error) {
    ph::def("int_only(x)", [](int argc, py_StackRef argv) -> bool {
        PY_CHECK_ARGC(1);
        auto x = ph::arg<py_i64>(argv, 0);
        if (!x) return false;
        return ph::ret_int(*x);
    });

    auto result = ph::eval("int_only('not an int')");
    ASSERT(!result.ok());  // Type error
}

// ============================================================================
// List Tests
// ============================================================================

TEST(list_foreach) {
    ph::exec("lst = [10, 20, 30]");
    auto lst = ph::get_global("lst");

    py_i64 sum = 0;
    bool ok = ph::list_foreach(lst, [&sum](int idx, py_Ref item) {
        (void)idx;
        sum += py_toint(item);
        return true;
    });

    ASSERT(ok);
    ASSERT_EQ(sum, 60);
}

TEST(list_from_ints) {
    ph::list_from_ints(py_r0(), {1, 2, 3, 4, 5});
    ph::set_global("nums", py_r0());

    auto result = ph::eval("sum(nums)");
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 15);
}

TEST(list_from_container) {
    std::vector<int> vec = {100, 200, 300};
    ph::list_from(py_r0(), vec);
    ph::set_global("vec_list", py_r0());

    auto result = ph::eval("sum(vec_list)");
    ASSERT(result.ok());
    ASSERT_EQ(py_toint(result.value()), 600);
}

// ============================================================================
// Result Tests
// ============================================================================

TEST(result_success) {
    auto r = ph::Result<int>::success(42);
    ASSERT(r.ok());
    ASSERT(r);
    ASSERT_EQ(r.value(), 42);
    ASSERT_EQ(*r, 42);
}

TEST(result_failure) {
    auto r = ph::Result<int>::failure();
    ASSERT(!r.ok());
    ASSERT(!r);
    ASSERT_EQ(r.value_or(99), 99);
}

// ============================================================================
// Debug Tests
// ============================================================================

TEST(type_name) {
    auto v = ph::Value::integer(42, 0);
    ASSERT_STREQ(v.type_name(), "int");

    auto s = ph::Value::string("test", 1);
    ASSERT_STREQ(s.type_name(), "str");
}

TEST(repr) {
    auto v = ph::Value::integer(42, 0);
    const char* r = ph::repr(v);
    ASSERT_STREQ(r, "42");
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    py_initialize();

    printf("Running C++ wrapper tests...\n\n");

    printf("Scope tests:\n");
    RUN_TEST(scope_basic);
    RUN_TEST(scope_exception_print);
    RUN_TEST(scope_exception_raise);
    RUN_TEST(scope_ok_check);

    printf("\nValue tests:\n");
    RUN_TEST(value_integer);
    RUN_TEST(value_float);
    RUN_TEST(value_string);
    RUN_TEST(value_boolean);
    RUN_TEST(value_move);
    RUN_TEST(value_different_registers);
    RUN_TEST(value_safe_extraction);
    RUN_TEST(value_wrap);

    printf("\nExecution tests:\n");
    RUN_TEST(exec_simple);
    RUN_TEST(exec_error);
    RUN_TEST(eval_simple);
    RUN_TEST(eval_error);

    printf("\nCall tests:\n");
    RUN_TEST(call_builtin_no_args);
    RUN_TEST(call_builtin_with_args);
    RUN_TEST(call_method);
    RUN_TEST(call_method_with_arg);
    RUN_TEST(call_register_result);
    RUN_TEST(call_error);

    printf("\nBinding tests:\n");
    RUN_TEST(binding_def);
    RUN_TEST(binding_set_get_global);

    printf("\nArgument extraction tests:\n");
    RUN_TEST(arg_extraction);
    RUN_TEST(arg_type_error);

    printf("\nList tests:\n");
    RUN_TEST(list_foreach);
    RUN_TEST(list_from_ints);
    RUN_TEST(list_from_container);

    printf("\nResult tests:\n");
    RUN_TEST(result_success);
    RUN_TEST(result_failure);

    printf("\nDebug tests:\n");
    RUN_TEST(type_name);
    RUN_TEST(repr);

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    py_finalize();

    return tests_failed > 0 ? 1 : 0;
}
