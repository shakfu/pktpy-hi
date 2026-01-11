/*
 * test_binding.c - Tests for ph_def, ph_setglobal, PH_ARG_*, PH_RETURN_*
 *
 * Demonstrates:
 * - Binding C functions to Python
 * - Using argument extraction macros
 * - Using return macros
 * - Optional arguments
 */

#include "test_common.h"

// Simple add function
static bool cfunc_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    PH_ARG_INT(0, a);
    PH_ARG_INT(1, b);
    PH_RETURN_INT(a + b);
}

// Function returning float
static bool cfunc_divide(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    PH_ARG_FLOAT(0, a);
    PH_ARG_FLOAT(1, b);
    if (b == 0.0) {
        return ZeroDivisionError("division by zero");
    }
    PH_RETURN_FLOAT(a / b);
}

// Function with string
static bool cfunc_strlen(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(1);
    PH_ARG_STR(0, s);
    PH_RETURN_INT((py_i64)strlen(s));
}

// Function returning bool
static bool cfunc_is_positive(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(1);
    PH_ARG_INT(0, n);
    PH_RETURN_BOOL(n > 0);
}

// Function returning None
static bool cfunc_noop(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(0);
    (void)argv;
    PH_RETURN_NONE;
}

// Function with optional argument
static bool cfunc_greet(int argc, py_StackRef argv) {
    if (argc < 1 || argc > 2) {
        return TypeError("greet() takes 1-2 arguments");
    }
    PH_ARG_STR(0, name);
    PH_ARG_STR_OPT(1, greeting, "Hello");

    // Build result string
    char buf[256];
    snprintf(buf, sizeof(buf), "%s, %s!", greeting, name);
    PH_RETURN_STR(buf);
}

// Function with optional int
static bool cfunc_power(int argc, py_StackRef argv) {
    if (argc < 1 || argc > 2) {
        return TypeError("power() takes 1-2 arguments");
    }
    PH_ARG_INT(0, base);
    PH_ARG_INT_OPT(1, exp, 2);  // Default exponent is 2

    py_i64 result = 1;
    for (py_i64 i = 0; i < exp; i++) {
        result *= base;
    }
    PH_RETURN_INT(result);
}

TEST(bind_add) {
    ph_def("c_add(a, b)", cfunc_add);

    bool ok = ph_eval("c_add(10, 20)");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 30);
}

TEST(bind_divide) {
    ph_def("c_divide(a, b)", cfunc_divide);

    bool ok = ph_eval("c_divide(10, 4)");
    ASSERT(ok);
    py_f64 diff = py_tofloat(py_retval()) - 2.5;
    ASSERT(diff > -0.00001 && diff < 0.00001);
}

TEST(bind_divide_by_zero) {
    ph_def("c_divide2(a, b)", cfunc_divide);

    bool ok = ph_eval("c_divide2(1, 0)");
    ASSERT(!ok);  // Should raise ZeroDivisionError
    ASSERT(!py_checkexc());
}

TEST(bind_strlen) {
    ph_def("c_strlen(s)", cfunc_strlen);

    bool ok = ph_eval("c_strlen('hello world')");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 11);
}

TEST(bind_is_positive) {
    ph_def("c_is_positive(n)", cfunc_is_positive);

    bool ok = ph_eval("c_is_positive(5)");
    ASSERT(ok);
    ASSERT(py_tobool(py_retval()) == true);

    ok = ph_eval("c_is_positive(-3)");
    ASSERT(ok);
    ASSERT(py_tobool(py_retval()) == false);
}

TEST(bind_noop) {
    ph_def("c_noop()", cfunc_noop);

    bool ok = ph_eval("c_noop()");
    ASSERT(ok);
    ASSERT(py_isnone(py_retval()));
}

TEST(bind_optional_string) {
    ph_def("c_greet(name, greeting=None)", cfunc_greet);

    bool ok = ph_eval("c_greet('World')");
    ASSERT(ok);
    ASSERT_STR_EQ(py_tostr(py_retval()), "Hello, World!");

    ok = ph_eval("c_greet('World', 'Hi')");
    ASSERT(ok);
    ASSERT_STR_EQ(py_tostr(py_retval()), "Hi, World!");
}

TEST(bind_optional_int) {
    ph_def("c_power(base, exp=None)", cfunc_power);

    bool ok = ph_eval("c_power(3)");  // 3^2 = 9
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 9);

    ok = ph_eval("c_power(2, 10)");  // 2^10 = 1024
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 1024);
}

TEST(bind_wrong_argc) {
    ph_def("c_add2(a, b)", cfunc_add);

    bool ok = ph_eval("c_add2(1)");  // Missing argument
    ASSERT(!ok);
    ASSERT(!py_checkexc());
}

TEST(bind_wrong_type) {
    ph_def("c_add3(a, b)", cfunc_add);

    bool ok = ph_eval("c_add3('a', 'b')");  // Wrong types
    ASSERT(!ok);
    ASSERT(!py_checkexc());
}

TEST(setglobal_getglobal) {
    ph_setglobal("test_var", ph_int(12345));

    py_ItemRef var = ph_getglobal("test_var");
    ASSERT(var != NULL);
    ASSERT_EQ(py_toint(var), 12345);
}

TEST(getglobal_undefined) {
    py_ItemRef var = ph_getglobal("undefined_variable_xyz");
    ASSERT(var == NULL);
}

TEST(def_in_module) {
    ph_def_in("mymod", "mod_func(x)", cfunc_is_positive);

    bool ok = ph_exec("import mymod", "<test>");
    ASSERT(ok);

    ok = ph_eval("mymod.mod_func(10)");
    ASSERT(ok);
    ASSERT(py_tobool(py_retval()) == true);
}

TEST_SUITE_BEGIN("Function Binding")
    RUN_TEST(bind_add);
    RUN_TEST(bind_divide);
    RUN_TEST(bind_divide_by_zero);
    RUN_TEST(bind_strlen);
    RUN_TEST(bind_is_positive);
    RUN_TEST(bind_noop);
    RUN_TEST(bind_optional_string);
    RUN_TEST(bind_optional_int);
    RUN_TEST(bind_wrong_argc);
    RUN_TEST(bind_wrong_type);
    RUN_TEST(setglobal_getglobal);
    RUN_TEST(getglobal_undefined);
    RUN_TEST(def_in_module);
TEST_SUITE_END()
