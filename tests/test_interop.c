/*
 * test_interop.c - Tests for interoperability with low-level pocketpy API
 *
 * Demonstrates:
 * - Mixing ph_* with py_* functions
 * - Using scopes with raw py_exec
 * - Combining high-level value creation with low-level operations
 */

#include "test_common.h"

TEST(mix_exec_styles) {
    // Use ph_exec for safe execution
    bool ok = ph_exec("x = 10", "<test>");
    ASSERT(ok);

    // Use raw py_exec within a scope
    ph_Scope scope = ph_scope_begin();
    py_exec("y = x * 2", "<test>", EXEC_MODE, NULL);
    ok = ph_scope_end(&scope);
    ASSERT(ok);

    // Verify both worked
    py_ItemRef y = py_getglobal(py_name("y"));
    ASSERT(y != NULL);
    ASSERT_EQ(py_toint(y), 20);
}

TEST(mix_value_creation) {
    // Use ph_int for quick creation
    py_GlobalRef a = ph_tmp_int(100);

    // Use py_newint for explicit output
    py_newint(py_r1(), 200);
    py_GlobalRef b = py_r1();

    // Both should work identically
    ASSERT_EQ(py_toint(a), 100);
    ASSERT_EQ(py_toint(b), 200);

    // Can use together in operations
    ph_setglobal("a", a);
    py_setglobal(py_name("b"), b);

    bool ok = ph_eval("a + b");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 300);
}

TEST(scope_with_raw_api) {
    // Use ph_Scope to protect raw API calls
    ph_Scope scope = ph_scope_begin();

    // Raw stack manipulation
    py_push(ph_tmp_int(5));
    py_push(ph_tmp_int(10));

    // Get values from stack
    py_i64 top = py_toint(py_peek(-1));
    py_i64 second = py_toint(py_peek(-2));

    // Clean up stack
    py_shrink(2);

    bool ok = ph_scope_end(&scope);
    ASSERT(ok);
    ASSERT_EQ(top, 10);
    ASSERT_EQ(second, 5);
}

TEST(ph_result_with_raw_call) {
    // Define function with raw API
    ph_Scope scope = ph_scope_begin();
    py_exec(
        "def compute(a, b, c):\n"
        "    return (a + b) * c\n",
        "<test>", EXEC_MODE, NULL
    );
    ph_scope_end(&scope);

    // Get function with raw API
    py_ItemRef fn = py_getglobal(py_name("compute"));
    ASSERT(fn != NULL);

    // Prepare args with ph_ helpers
    ph_int_r(0, 2);
    ph_int_r(1, 3);
    ph_int_r(2, 4);

    // Call with ph_call
    ph_Result r = ph_call(fn, 3, py_r0());
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 20);  // (2+3)*4 = 20
}

TEST(dict_with_ph_values) {
    // Create dict with raw API
    py_newdict(py_r0());

    // Set values using ph_ helpers
    py_dict_setitem_by_str(py_r0(), "name", ph_str_r(1, "Alice"));
    py_dict_setitem_by_str(py_r0(), "age", ph_int_r(2, 30));
    py_dict_setitem_by_str(py_r0(), "score", ph_float_r(3, 95.5));

    // Verify with Python
    ph_setglobal("person", py_r0());

    bool ok = ph_eval("person['name']");
    ASSERT(ok);
    ASSERT_STR_EQ(py_tostr(py_retval()), "Alice");

    ok = ph_eval("person['age']");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 30);
}

TEST(type_creation_interop) {
    // Create a custom type using raw API
    py_Type my_type = py_newtype("MyClass", tp_object, NULL, NULL);
    ASSERT(my_type != 0);

    // Create instance
    py_newobject(py_r0(), my_type, -1, 0);  // -1 for __dict__

    // Set attributes using ph_ helpers
    py_setdict(py_r0(), py_name("value"), ph_int_r(1, 42));
    py_setdict(py_r0(), py_name("label"), ph_str_r(2, "test"));

    // Make it available in Python
    ph_setglobal("my_obj", py_r0());

    // Access from Python
    bool ok = ph_eval("my_obj.value");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 42);

    ok = ph_eval("my_obj.label");
    ASSERT(ok);
    ASSERT_STR_EQ(py_tostr(py_retval()), "test");
}

// Native function using both APIs
static bool hybrid_func(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(1);

    // Use PH_ macro for arg extraction
    PH_ARG_INT(0, n);

    // Use raw API for computation
    py_i64 result = 0;
    for (py_i64 i = 1; i <= n; i++) {
        result += i;
    }

    // Use PH_ macro for return
    PH_RETURN_INT(result);
}

TEST(hybrid_native_function) {
    // Bind using ph_ helper
    ph_def("sum_to_n(n)", hybrid_func);

    // Call using ph_ helper
    ph_Result r = ph_call1("sum_to_n", ph_tmp_int(10));
    ASSERT(r.ok);
    ASSERT_EQ(py_toint(r.val), 55);  // 1+2+...+10 = 55
}

TEST(error_handling_interop) {
    // ph_Scope can wrap raw API error-prone code
    ph_Scope outer = ph_scope_begin();

    // Successful raw operation
    py_exec("valid = 1", "<test>", EXEC_MODE, NULL);

    // Nested scope for risky operation
    {
        ph_Scope inner = ph_scope_begin();
        py_exec("1/0", "<test>", EXEC_MODE, NULL);
        ASSERT(!ph_scope_end(&inner));
    }

    // Continue with more operations
    py_exec("another = 2", "<test>", EXEC_MODE, NULL);

    ASSERT(ph_scope_end(&outer));

    // Both valid and another should exist
    ASSERT(py_getglobal(py_name("valid")) != NULL);
    ASSERT(py_getglobal(py_name("another")) != NULL);
}

TEST(register_reuse) {
    // Demonstrate register management between APIs
    // r0-r3 used by ph_ functions, r4-r7 available for user

    py_GlobalRef a = ph_tmp_int(1);       // Uses r0
    py_GlobalRef b = ph_tmp_str("test");  // Overwrites r0!

    // a is now invalid because r0 was reused
    // This is expected behavior - document it

    // For multiple values, use explicit registers
    ph_int_r(0, 10);
    ph_int_r(1, 20);
    ph_int_r(2, 30);

    // Or use raw API with user registers
    py_newint(py_r4(), 100);
    py_newint(py_r5(), 200);

    // All are valid simultaneously
    ASSERT_EQ(py_toint(py_r0()), 10);
    ASSERT_EQ(py_toint(py_r1()), 20);
    ASSERT_EQ(py_toint(py_r2()), 30);
    ASSERT_EQ(py_toint(py_r4()), 100);
    ASSERT_EQ(py_toint(py_r5()), 200);

    (void)a;
    (void)b;
}

TEST(module_interop) {
    // Create module with raw API
    py_GlobalRef mod = py_newmodule("hybrid_mod");

    // Add function with ph_ helper
    py_bind(mod, "helper(x)", hybrid_func);

    // Add constant with ph_ helper
    py_setdict(mod, py_name("CONSTANT"), ph_tmp_int(42));

    // Use from Python
    bool ok = ph_exec("import hybrid_mod", "<test>");
    ASSERT(ok);

    ok = ph_eval("hybrid_mod.helper(5)");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 15);

    ok = ph_eval("hybrid_mod.CONSTANT");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 42);
}

TEST_SUITE_BEGIN("API Interoperability")
    RUN_TEST(mix_exec_styles);
    RUN_TEST(mix_value_creation);
    RUN_TEST(scope_with_raw_api);
    RUN_TEST(ph_result_with_raw_call);
    RUN_TEST(dict_with_ph_values);
    RUN_TEST(type_creation_interop);
    RUN_TEST(hybrid_native_function);
    RUN_TEST(error_handling_interop);
    RUN_TEST(register_reuse);
    RUN_TEST(module_interop);
TEST_SUITE_END()
