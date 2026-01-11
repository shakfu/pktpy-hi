/*
 * test_exec.c - Tests for ph_exec and ph_eval
 *
 * Demonstrates:
 * - Simple execution with automatic error handling
 * - Evaluation with result retrieval
 * - Module-specific execution
 */

#include "test_common.h"

TEST(exec_simple) {
    // Basic execution should succeed
    bool ok = ph_exec("result = 2 + 3", "<test>");
    ASSERT(ok);

    py_ItemRef result = py_getglobal(py_name("result"));
    ASSERT(result != NULL);
    ASSERT_EQ(py_toint(result), 5);
}

TEST(exec_multiline) {
    // Multiline code should work
    bool ok = ph_exec(
        "def square(x):\n"
        "    return x * x\n"
        "squared = square(7)\n",
        "<test>"
    );
    ASSERT(ok);

    py_ItemRef squared = py_getglobal(py_name("squared"));
    ASSERT(squared != NULL);
    ASSERT_EQ(py_toint(squared), 49);
}

TEST(exec_syntax_error) {
    // Syntax errors should be caught
    bool ok = ph_exec("def bad syntax", "<test>");
    ASSERT(!ok);
    ASSERT(!py_checkexc());  // Exception should be cleared
}

TEST(exec_runtime_error) {
    // Runtime errors should be caught
    bool ok = ph_exec("x = undefined_variable", "<test>");
    ASSERT(!ok);
    ASSERT(!py_checkexc());
}

TEST(eval_simple) {
    // Evaluation should return result in py_retval()
    bool ok = ph_eval("3 * 4");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 12);
}

TEST(eval_expression) {
    // Complex expression evaluation
    ph_exec("base = 10", "<test>");
    bool ok = ph_eval("base ** 2 + 5");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 105);
}

TEST(eval_string) {
    // String evaluation
    bool ok = ph_eval("'hello' + ' ' + 'world'");
    ASSERT(ok);
    ASSERT_STR_EQ(py_tostr(py_retval()), "hello world");
}

TEST(eval_error) {
    // Evaluation error should be handled
    bool ok = ph_eval("1 / 0");
    ASSERT(!ok);
    ASSERT(!py_checkexc());
}

TEST(exec_in_module) {
    // Execution in a specific module
    py_GlobalRef mod = py_newmodule("testmod");
    bool ok = ph_exec_in("mod_var = 42", "<test>", mod);
    ASSERT(ok);

    // Variable should be in module, not __main__
    py_ItemRef main_var = py_getglobal(py_name("mod_var"));
    ASSERT(main_var == NULL);

    py_ItemRef mod_var = py_getdict(mod, py_name("mod_var"));
    ASSERT(mod_var != NULL);
    ASSERT_EQ(py_toint(mod_var), 42);
}

TEST(eval_in_module) {
    // Evaluation in a specific module
    py_GlobalRef mod = py_newmodule("evalmod");
    ph_exec_in("x = 100", "<test>", mod);

    bool ok = ph_eval_in("x * 2", mod);
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 200);
}

TEST(exec_raise_keeps_exception) {
    // ph_exec_raise should NOT clear the exception
    bool ok = ph_exec_raise("1 / 0", "<test>");
    ASSERT(!ok);
    ASSERT(py_checkexc());  // Exception should still be set

    // Clean up for subsequent tests
    py_clearexc(NULL);
}

TEST(exec_raise_success) {
    // ph_exec_raise should work normally on success
    bool ok = ph_exec_raise("raise_test = 123", "<test>");
    ASSERT(ok);
    ASSERT(!py_checkexc());

    py_ItemRef result = py_getglobal(py_name("raise_test"));
    ASSERT(result != NULL);
    ASSERT_EQ(py_toint(result), 123);
}

TEST(eval_raise_keeps_exception) {
    // ph_eval_raise should NOT clear the exception
    bool ok = ph_eval_raise("undefined_var");
    ASSERT(!ok);
    ASSERT(py_checkexc());  // Exception should still be set

    // Clean up
    py_clearexc(NULL);
}

TEST(eval_raise_success) {
    // ph_eval_raise should work normally on success
    bool ok = ph_eval_raise("10 * 10");
    ASSERT(ok);
    ASSERT(!py_checkexc());
    ASSERT_EQ(py_toint(py_retval()), 100);
}

TEST_SUITE_BEGIN("Execution Helpers")
    RUN_TEST(exec_simple);
    RUN_TEST(exec_multiline);
    RUN_TEST(exec_syntax_error);
    RUN_TEST(exec_runtime_error);
    RUN_TEST(eval_simple);
    RUN_TEST(eval_expression);
    RUN_TEST(eval_string);
    RUN_TEST(eval_error);
    RUN_TEST(exec_in_module);
    RUN_TEST(eval_in_module);
    RUN_TEST(exec_raise_keeps_exception);
    RUN_TEST(exec_raise_success);
    RUN_TEST(eval_raise_keeps_exception);
    RUN_TEST(eval_raise_success);
TEST_SUITE_END()
