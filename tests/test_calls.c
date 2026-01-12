/*
 * test_calls.c - Tests for ph_call* functions
 *
 * Demonstrates:
 * - Calling Python functions from C
 * - Handling call results with ph_Result
 * - Calling methods on objects
 */

#include "test_common.h"

TEST(call0_builtin) {
    // Call a no-arg function
    ph_exec("def get_value(): return 42", "<test>");

    ph_Result r = ph_call0("get_value");
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 42);
}

TEST(call1_simple) {
    // Call with one argument
    ph_exec("def double(x): return x * 2", "<test>");

    ph_Result r = ph_call1("double", ph_tmp_int(21));
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 42);
}

TEST(call1_string) {
    // Call with string argument
    ph_exec("def greet(name): return 'Hello, ' + name", "<test>");

    ph_Result r = ph_call1("greet", ph_tmp_str("World"));
    ASSERT(r.ok);
    ASSERT_STR_EQ(py_tostr(r.val), "Hello, World");
}

TEST(call2_add) {
    // Call with two arguments (contiguous)
    ph_exec("def add(a, b): return a + b", "<test>");

    // Arguments must be contiguous - use registers
    ph_int_r(0, 10);
    ph_int_r(1, 20);

    ph_Result r = ph_call2("add", py_r0());
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 30);
}

TEST(call3_sum) {
    // Call with three arguments
    ph_exec("def sum3(a, b, c): return a + b + c", "<test>");

    ph_int_r(0, 1);
    ph_int_r(1, 2);
    ph_int_r(2, 3);

    ph_Result r = ph_call3("sum3", py_r0());
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 6);
}

TEST(call_undefined) {
    // Calling undefined function should fail gracefully
    ph_Result r = ph_call0("nonexistent_function");
    ASSERT(!r.ok);
    ASSERT(!py_checkexc());  // Exception should be cleared
}

TEST(call_exception) {
    // Function that raises exception
    ph_exec("def fail(): raise ValueError('oops')", "<test>");

    ph_Result r = ph_call0("fail");
    ASSERT(!r.ok);
    ASSERT(!py_checkexc());
}

TEST(call_callable_ref) {
    // Call using a callable reference directly
    ph_exec("def multiply(a, b): return a * b", "<test>");

    py_ItemRef fn = ph_getglobal("multiply");
    ASSERT(fn != NULL);

    ph_int_r(0, 6);
    ph_int_r(1, 7);

    ph_Result r = ph_call(fn, 2, py_r0());
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 42);
}

TEST(callmethod0_simple) {
    // Call method with no arguments
    ph_exec("my_list = [1, 2, 3]", "<test>");
    py_ItemRef list = ph_getglobal("my_list");
    ASSERT(list != NULL);

    ph_Result r = ph_callmethod0(list, "copy");
    ASSERT(r.ok);
    ASSERT(py_islist(r.val));
    ASSERT_EQ(py_list_len(r.val), 3);
}

TEST(callmethod1_append) {
    // Call method with one argument
    ph_exec("items = []", "<test>");
    py_ItemRef items = ph_getglobal("items");
    ASSERT(items != NULL);

    ph_Result r = ph_callmethod1(items, "append", ph_tmp_int(42));
    ASSERT(r.ok);

    // Verify item was appended
    ASSERT_EQ(py_list_len(items), 1);
    ASSERT_EQ(py_toint(py_list_getitem(items, 0)), 42);
}

TEST(callmethod_string) {
    // Call string method
    ph_exec("text = 'hello world'", "<test>");
    py_ItemRef text = ph_getglobal("text");

    ph_Result r = ph_callmethod0(text, "upper");
    ASSERT(r.ok);
    ASSERT_STR_EQ(py_tostr(r.val), "HELLO WORLD");
}

TEST(callmethod_undefined) {
    // Calling undefined method should fail
    ph_exec("obj = 42", "<test>");
    py_ItemRef obj = ph_getglobal("obj");

    ph_Result r = ph_callmethod0(obj, "nonexistent_method");
    ASSERT(!r.ok);
    ASSERT(!py_checkexc());
}

TEST(call0_raise_keeps_exception) {
    // ph_call0_raise should NOT clear exceptions
    ph_exec("def raise_error(): raise ValueError('test')", "<test>");

    ph_Result r = ph_call0_raise("raise_error");
    ASSERT(!r.ok);
    ASSERT(py_checkexc());  // Exception should still be set

    // Can inspect the exception type
    ASSERT(py_matchexc(tp_ValueError));

    // Clean up
    py_clearexc(NULL);
}

TEST(call0_raise_undefined_keeps_exception) {
    // Missing function should also keep exception
    ph_Result r = ph_call0_raise("nonexistent_func");
    ASSERT(!r.ok);
    ASSERT(py_checkexc());  // NameError should still be set

    // Can inspect the exception
    ASSERT(py_matchexc(tp_NameError));

    py_clearexc(NULL);
}

TEST(call1_raise_success) {
    // Successful calls should work normally
    ph_exec("def increment(x): return x + 1", "<test>");

    ph_Result r = ph_call1_raise("increment", ph_tmp_int(41));
    ASSERT(r.ok);
    ASSERT(!py_checkexc());
    ASSERT_EQ(py_toint(r.val), 42);
}

TEST(callmethod_raise_keeps_exception) {
    // Method calls should also propagate exceptions
    ph_exec("obj = 42", "<test>");
    py_ItemRef obj = ph_getglobal("obj");

    ph_Result r = ph_callmethod0_raise(obj, "no_such_method");
    ASSERT(!r.ok);
    ASSERT(py_checkexc());  // AttributeError should still be set

    py_clearexc(NULL);
}

TEST(call_r_preserves_across_calls) {
    // Key test: _r variants preserve results across multiple calls
    ph_exec("def get_a(): return 100", "<test>");
    ph_exec("def get_b(): return 200", "<test>");

    // With base ph_call0, r1.val would be invalidated by second call
    // With _r variants, each result is in its own register
    ph_Result r1 = ph_call0_r(4, "get_a");  // Store in r4
    ph_Result r2 = ph_call0_r(5, "get_b");  // Store in r5

    ASSERT(r1.ok);
    ASSERT(r2.ok);

    // Both results should still be valid
    ASSERT_EQ(py_toint(r1.val), 100);
    ASSERT_EQ(py_toint(r2.val), 200);

    // Verify they're in the expected registers
    ASSERT(r1.val == py_r4());
    ASSERT(r2.val == py_r5());
}

TEST(call1_r_simple) {
    ph_exec("def square(x): return x * x", "<test>");

    ph_Result r = ph_call1_r(6, "square", ph_tmp_int(7));
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 49);
    ASSERT(r.val == py_getreg(6));
}

TEST(callmethod_r_simple) {
    ph_exec("text = 'hello'", "<test>");
    py_ItemRef text = ph_getglobal("text");

    ph_Result r = ph_callmethod0_r(7, text, "upper");
    ASSERT(r.ok);
    ASSERT_STR_EQ(py_tostr(r.val), "HELLO");
    ASSERT(r.val == py_r7());
}

TEST(call_r_result_usable_as_arg) {
    // Result from _r can be safely passed to another call
    ph_exec("def double(x): return x * 2", "<test>");
    ph_exec("def add_ten(x): return x + 10", "<test>");

    ph_Result r1 = ph_call1_r(4, "double", ph_tmp_int(5));  // 10
    ASSERT(r1.ok);

    // r1.val is stable, can use it as argument to another call
    ph_Result r2 = ph_call1_r(5, "add_ten", r1.val);    // 20
    ASSERT(r2.ok);

    // Both are still valid
    ASSERT_EQ(py_toint(r1.val), 10);
    ASSERT_EQ(py_toint(r2.val), 20);
}

TEST(callmethod2_simple) {
    // Test callmethod with 2 arguments
    ph_exec("text = 'hello world'", "<test>");
    py_ItemRef text = ph_getglobal("text");

    ph_Result r = ph_callmethod2(text, "replace",
        ph_str_r(0, "world"),
        ph_str_r(1, "universe"));
    ASSERT(r.ok);
    ASSERT_STR_EQ(py_tostr(r.val), "hello universe");
}

TEST(callmethod3_simple) {
    // Test callmethod with 3 arguments
    // Use a custom Python class since str.replace in pocketpy may have different signature
    ph_exec(
        "class Container:\n"
        "    def __init__(self, val):\n"
        "        self.val = val\n"
        "    def compute(self, a, b, c):\n"
        "        return self.val + a + b + c\n"
        "container = Container(100)",
        "<test>"
    );
    py_ItemRef container = ph_getglobal("container");

    ph_Result r = ph_callmethod3(container, "compute",
        ph_int_r(0, 10),
        ph_int_r(1, 20),
        ph_int_r(2, 30));
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 160);  // 100 + 10 + 20 + 30
}

TEST(call_r_raise_success) {
    // _r_raise variant combines stable storage with exception propagation
    ph_exec("def get_value(): return 42", "<test>");

    ph_Result r = ph_call0_r_raise(4, "get_value");
    ASSERT(r.ok);
    ASSERT(!py_checkexc());
    ASSERT_EQ(py_toint(r.val), 42);
    ASSERT(r.val == py_r4());
}

TEST(call_r_raise_exception) {
    // _r_raise should keep exception set
    ph_exec("def fail(): raise RuntimeError('test')", "<test>");

    ph_Result r = ph_call0_r_raise(4, "fail");
    ASSERT(!r.ok);
    ASSERT(py_checkexc());  // Exception should be set
    ASSERT(py_matchexc(tp_RuntimeError));

    py_clearexc(NULL);
}

TEST(callmethod_r_raise_success) {
    ph_exec("items = ['a', 'b', 'c']", "<test>");
    py_ItemRef items = ph_getglobal("items");

    ph_Result r = ph_callmethod0_r_raise(5, items, "copy");
    ASSERT(r.ok);
    ASSERT(!py_checkexc());
    ASSERT(py_islist(r.val));
    ASSERT(r.val == py_r5());
}

TEST(callmethod_r_raise_exception) {
    ph_exec("obj = 42", "<test>");
    py_ItemRef obj = ph_getglobal("obj");

    ph_Result r = ph_callmethod0_r_raise(5, obj, "no_method");
    ASSERT(!r.ok);
    ASSERT(py_checkexc());  // AttributeError should be set

    py_clearexc(NULL);
}

TEST_SUITE_BEGIN("Function Calls")
    RUN_TEST(call0_builtin);
    RUN_TEST(call1_simple);
    RUN_TEST(call1_string);
    RUN_TEST(call2_add);
    RUN_TEST(call3_sum);
    RUN_TEST(call_undefined);
    RUN_TEST(call_exception);
    RUN_TEST(call_callable_ref);
    RUN_TEST(callmethod0_simple);
    RUN_TEST(callmethod1_append);
    RUN_TEST(callmethod2_simple);
    RUN_TEST(callmethod3_simple);
    RUN_TEST(callmethod_string);
    RUN_TEST(callmethod_undefined);
    RUN_TEST(call0_raise_keeps_exception);
    RUN_TEST(call0_raise_undefined_keeps_exception);
    RUN_TEST(call1_raise_success);
    RUN_TEST(callmethod_raise_keeps_exception);
    RUN_TEST(call_r_preserves_across_calls);
    RUN_TEST(call1_r_simple);
    RUN_TEST(callmethod_r_simple);
    RUN_TEST(call_r_result_usable_as_arg);
    RUN_TEST(call_r_raise_success);
    RUN_TEST(call_r_raise_exception);
    RUN_TEST(callmethod_r_raise_success);
    RUN_TEST(callmethod_r_raise_exception);
TEST_SUITE_END()
