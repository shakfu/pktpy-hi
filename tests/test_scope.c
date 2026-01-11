/*
 * test_scope.c - Tests for ph_Scope management
 *
 * Demonstrates:
 * - Automatic stack unwinding on success
 * - Automatic exception cleanup on failure
 * - Nested scopes
 */

#include "test_common.h"

TEST(scope_success) {
    // Scope should return true when no exception occurs
    ph_Scope scope = ph_scope_begin();
    py_exec("x = 1 + 2", "<test>", EXEC_MODE, NULL);
    bool ok = ph_scope_end(&scope);

    ASSERT(ok);
    ASSERT(!ph_scope_failed(&scope));
}

TEST(scope_exception) {
    // Scope should return false and clear exception
    ph_Scope scope = ph_scope_begin();
    py_exec("1 / 0", "<test>", EXEC_MODE, NULL);
    bool ok = ph_scope_end(&scope);

    ASSERT(!ok);
    ASSERT(ph_scope_failed(&scope));
    // Exception should be cleared
    ASSERT(!py_checkexc());
}

TEST(scope_nested_success) {
    // Nested scopes should work correctly
    ph_Scope outer = ph_scope_begin();

    py_exec("a = 10", "<test>", EXEC_MODE, NULL);

    {
        ph_Scope inner = ph_scope_begin();
        py_exec("b = 20", "<test>", EXEC_MODE, NULL);
        ASSERT(ph_scope_end(&inner));
    }

    py_exec("c = a + b", "<test>", EXEC_MODE, NULL);
    ASSERT(ph_scope_end(&outer));

    // Verify values
    py_ItemRef c = py_getglobal(py_name("c"));
    ASSERT(c != NULL);
    ASSERT_EQ(py_toint(c), 30);
}

TEST(scope_nested_inner_fail) {
    // Inner scope failure should be contained
    ph_Scope outer = ph_scope_begin();

    py_exec("x = 100", "<test>", EXEC_MODE, NULL);

    {
        ph_Scope inner = ph_scope_begin();
        py_exec("1 / 0", "<test>", EXEC_MODE, NULL);
        ASSERT(!ph_scope_end(&inner));
        // Exception cleared by inner scope
    }

    // Outer scope should still work
    py_exec("y = x * 2", "<test>", EXEC_MODE, NULL);
    ASSERT(ph_scope_end(&outer));

    py_ItemRef y = py_getglobal(py_name("y"));
    ASSERT(y != NULL);
    ASSERT_EQ(py_toint(y), 200);
}

TEST(scope_end_print) {
    // ph_scope_end_print should print and clear exception
    ph_Scope scope = ph_scope_begin();
    py_exec("undefined_var", "<test>", EXEC_MODE, NULL);

    // Redirect would be needed to verify print, but we just check it doesn't crash
    bool ok = ph_scope_end_print(&scope);
    ASSERT(!ok);
    ASSERT(!py_checkexc());
}

TEST_SUITE_BEGIN("Scope Management")
    RUN_TEST(scope_success);
    RUN_TEST(scope_exception);
    RUN_TEST(scope_nested_success);
    RUN_TEST(scope_nested_inner_fail);
    RUN_TEST(scope_end_print);
TEST_SUITE_END()
