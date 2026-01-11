/*
 * test_debug.c - Tests for debug/development helper functions
 *
 * Tests ph_print, ph_repr, and ph_typename on various value types.
 * Verifies no exception leakage or stack corruption after calls.
 */

#include "test_common.h"
#include <string.h>

/* ============================================================================
 * ph_typename tests
 * ============================================================================ */

TEST(typename_int) {
    py_GlobalRef val = ph_int(42);
    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "int");
    ASSERT(!py_checkexc());
}

TEST(typename_float) {
    py_GlobalRef val = ph_float(3.14);
    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "float");
    ASSERT(!py_checkexc());
}

TEST(typename_str) {
    py_GlobalRef val = ph_str("hello");
    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "str");
    ASSERT(!py_checkexc());
}

TEST(typename_bool) {
    py_GlobalRef val = ph_bool(true);
    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "bool");
    ASSERT(!py_checkexc());
}

TEST(typename_list) {
    ph_exec("test_list = [1, 2, 3]", "<test>");
    py_ItemRef val = ph_getglobal("test_list");
    ASSERT(val != NULL);

    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "list");
    ASSERT(!py_checkexc());
}

TEST(typename_dict) {
    ph_exec("test_dict = {'a': 1}", "<test>");
    py_ItemRef val = ph_getglobal("test_dict");
    ASSERT(val != NULL);

    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "dict");
    ASSERT(!py_checkexc());
}

TEST(typename_none) {
    ph_exec("test_none = None", "<test>");
    py_ItemRef val = ph_getglobal("test_none");
    ASSERT(val != NULL);

    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "NoneType");
    ASSERT(!py_checkexc());
}

TEST(typename_custom_class) {
    ph_exec("class MyClass: pass", "<test>");
    ph_exec("test_obj = MyClass()", "<test>");
    py_ItemRef val = ph_getglobal("test_obj");
    ASSERT(val != NULL);

    const char* name = ph_typename(val);
    ASSERT_STR_EQ(name, "MyClass");
    ASSERT(!py_checkexc());
}

/* ============================================================================
 * ph_repr tests
 * ============================================================================ */

TEST(repr_int) {
    py_GlobalRef val = ph_int(42);
    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "42");
    ASSERT(!py_checkexc());
}

TEST(repr_negative_int) {
    py_GlobalRef val = ph_int(-123);
    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "-123");
    ASSERT(!py_checkexc());
}

TEST(repr_float) {
    py_GlobalRef val = ph_float(3.5);
    const char* repr = ph_repr(val);
    // Float repr may vary, just check it's not the fallback
    ASSERT(strcmp(repr, "<repr failed>") != 0);
    ASSERT(!py_checkexc());
}

TEST(repr_str) {
    py_GlobalRef val = ph_str("hello");
    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "'hello'");
    ASSERT(!py_checkexc());
}

TEST(repr_str_with_quotes) {
    py_GlobalRef val = ph_str("it's");
    const char* repr = ph_repr(val);
    // Should escape or use double quotes
    ASSERT(strcmp(repr, "<repr failed>") != 0);
    ASSERT(!py_checkexc());
}

TEST(repr_bool_true) {
    py_GlobalRef val = ph_bool(true);
    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "True");
    ASSERT(!py_checkexc());
}

TEST(repr_bool_false) {
    py_GlobalRef val = ph_bool(false);
    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "False");
    ASSERT(!py_checkexc());
}

TEST(repr_list) {
    ph_exec("repr_list = [1, 2, 3]", "<test>");
    py_ItemRef val = ph_getglobal("repr_list");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "[1, 2, 3]");
    ASSERT(!py_checkexc());
}

TEST(repr_empty_list) {
    ph_exec("empty_list = []", "<test>");
    py_ItemRef val = ph_getglobal("empty_list");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "[]");
    ASSERT(!py_checkexc());
}

TEST(repr_dict) {
    ph_exec("repr_dict = {'x': 1}", "<test>");
    py_ItemRef val = ph_getglobal("repr_dict");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "{'x': 1}");
    ASSERT(!py_checkexc());
}

TEST(repr_none) {
    ph_exec("repr_none = None", "<test>");
    py_ItemRef val = ph_getglobal("repr_none");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "None");
    ASSERT(!py_checkexc());
}

TEST(repr_custom_object) {
    // Object with default repr
    ph_exec("class SimpleClass: pass", "<test>");
    ph_exec("simple_obj = SimpleClass()", "<test>");
    py_ItemRef val = ph_getglobal("simple_obj");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    // Default repr contains class name and address
    ASSERT(strstr(repr, "SimpleClass") != NULL);
    ASSERT(!py_checkexc());
}

TEST(repr_custom_repr_method) {
    // Object with custom __repr__
    ph_exec("class CustomRepr:\n    def __repr__(self): return 'CustomRepr()'", "<test>");
    ph_exec("custom_obj = CustomRepr()", "<test>");
    py_ItemRef val = ph_getglobal("custom_obj");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    ASSERT_STR_EQ(repr, "CustomRepr()");
    ASSERT(!py_checkexc());
}

TEST(repr_failing_repr_method) {
    // Object whose __repr__ raises an exception
    ph_exec("class BadRepr:\n    def __repr__(self): raise ValueError('bad repr')", "<test>");
    ph_exec("bad_obj = BadRepr()", "<test>");
    py_ItemRef val = ph_getglobal("bad_obj");
    ASSERT(val != NULL);

    const char* repr = ph_repr(val);
    // Should return fallback, not crash
    ASSERT_STR_EQ(repr, "<repr failed>");
    // Exception should be cleared
    ASSERT(!py_checkexc());
}

/* ============================================================================
 * ph_print tests
 * ============================================================================
 * ph_print writes to the VM's print callback. We can't easily capture the
 * output, but we can verify it doesn't crash, leak exceptions, or corrupt
 * the stack.
 */

TEST(print_int) {
    py_StackRef stack_before = py_peek(0);
    py_GlobalRef val = ph_int(42);

    ph_print(val);

    // Verify no exception
    ASSERT(!py_checkexc());
    // Verify stack is clean (ph_print uses a scope internally)
    py_StackRef stack_after = py_peek(0);
    ASSERT(stack_before == stack_after);
}

TEST(print_str) {
    py_StackRef stack_before = py_peek(0);
    py_GlobalRef val = ph_str("hello world");

    ph_print(val);

    ASSERT(!py_checkexc());
    py_StackRef stack_after = py_peek(0);
    ASSERT(stack_before == stack_after);
}

TEST(print_list) {
    py_StackRef stack_before = py_peek(0);
    ph_exec("print_list = [1, 2, 3]", "<test>");
    py_ItemRef val = ph_getglobal("print_list");
    ASSERT(val != NULL);

    ph_print(val);

    ASSERT(!py_checkexc());
    py_StackRef stack_after = py_peek(0);
    ASSERT(stack_before == stack_after);
}

TEST(print_custom_object) {
    py_StackRef stack_before = py_peek(0);
    ph_exec("class PrintTest:\n    def __repr__(self): return 'PrintTest()'", "<test>");
    ph_exec("print_obj = PrintTest()", "<test>");
    py_ItemRef val = ph_getglobal("print_obj");
    ASSERT(val != NULL);

    ph_print(val);

    ASSERT(!py_checkexc());
    py_StackRef stack_after = py_peek(0);
    ASSERT(stack_before == stack_after);
}

TEST(print_failing_repr) {
    // ph_print should handle repr failures gracefully
    py_StackRef stack_before = py_peek(0);
    ph_exec("class BadPrint:\n    def __repr__(self): raise RuntimeError('oops')", "<test>");
    ph_exec("bad_print = BadPrint()", "<test>");
    py_ItemRef val = ph_getglobal("bad_print");
    ASSERT(val != NULL);

    ph_print(val);

    // Should not leave an exception
    ASSERT(!py_checkexc());
    // Stack should be clean
    py_StackRef stack_after = py_peek(0);
    ASSERT(stack_before == stack_after);
}

TEST(print_none) {
    py_StackRef stack_before = py_peek(0);
    ph_exec("print_none = None", "<test>");
    py_ItemRef val = ph_getglobal("print_none");
    ASSERT(val != NULL);

    ph_print(val);

    ASSERT(!py_checkexc());
    py_StackRef stack_after = py_peek(0);
    ASSERT(stack_before == stack_after);
}

/* ============================================================================
 * Combined/edge case tests
 * ============================================================================ */

TEST(multiple_repr_calls) {
    // Verify multiple calls don't accumulate state
    for (int i = 0; i < 10; i++) {
        py_GlobalRef val = ph_int(i);
        const char* repr = ph_repr(val);
        ASSERT(strcmp(repr, "<repr failed>") != 0);
        ASSERT(!py_checkexc());
    }
}

TEST(multiple_typename_calls) {
    // Verify multiple calls work correctly
    ASSERT_STR_EQ(ph_typename(ph_int(1)), "int");
    ASSERT_STR_EQ(ph_typename(ph_str("x")), "str");
    ASSERT_STR_EQ(ph_typename(ph_float(1.0)), "float");
    ASSERT_STR_EQ(ph_typename(ph_bool(true)), "bool");
    ASSERT(!py_checkexc());
}

TEST(debug_helpers_no_stack_leak) {
    // Comprehensive test that debug helpers don't leak stack entries
    py_StackRef stack_start = py_peek(0);

    // Call all helpers multiple times
    for (int i = 0; i < 5; i++) {
        py_GlobalRef val = ph_int(i * 10);

        ph_typename(val);
        ph_repr(val);
        ph_print(val);
    }

    py_StackRef stack_end = py_peek(0);
    ASSERT(stack_start == stack_end);
    ASSERT(!py_checkexc());
}

TEST_SUITE_BEGIN("Debug Helpers")
    // ph_typename tests
    RUN_TEST(typename_int);
    RUN_TEST(typename_float);
    RUN_TEST(typename_str);
    RUN_TEST(typename_bool);
    RUN_TEST(typename_list);
    RUN_TEST(typename_dict);
    RUN_TEST(typename_none);
    RUN_TEST(typename_custom_class);

    // ph_repr tests
    RUN_TEST(repr_int);
    RUN_TEST(repr_negative_int);
    RUN_TEST(repr_float);
    RUN_TEST(repr_str);
    RUN_TEST(repr_str_with_quotes);
    RUN_TEST(repr_bool_true);
    RUN_TEST(repr_bool_false);
    RUN_TEST(repr_list);
    RUN_TEST(repr_empty_list);
    RUN_TEST(repr_dict);
    RUN_TEST(repr_none);
    RUN_TEST(repr_custom_object);
    RUN_TEST(repr_custom_repr_method);
    RUN_TEST(repr_failing_repr_method);

    // ph_print tests
    RUN_TEST(print_int);
    RUN_TEST(print_str);
    RUN_TEST(print_list);
    RUN_TEST(print_custom_object);
    RUN_TEST(print_failing_repr);
    RUN_TEST(print_none);

    // Combined tests
    RUN_TEST(multiple_repr_calls);
    RUN_TEST(multiple_typename_calls);
    RUN_TEST(debug_helpers_no_stack_leak);
TEST_SUITE_END()
