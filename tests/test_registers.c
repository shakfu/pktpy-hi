/*
 * test_registers.c - Tests for register bounds checking
 *
 * Verifies that:
 * - PH_MAX_REG is correctly defined
 * - _r variants return failure for out-of-bounds registers
 * - Valid register indices work correctly
 */

#include "test_common.h"

TEST(max_reg_constant) {
    // Verify constant is defined and has expected value
    ASSERT_EQ(PH_MAX_REG, 8);
}

TEST(valid_register_indices) {
    // All registers 0-7 should work
    for (int i = 0; i < PH_MAX_REG; i++) {
        py_GlobalRef ref = ph_int_r(i, 100 + i);
        ASSERT(ref != NULL);
        ASSERT_EQ(py_toint(ref), 100 + i);
    }
}

TEST(call0_r_invalid_register_negative) {
    ph_exec("def get_value(): return 42", "<test>");

    // Negative register should fail gracefully
    ph_Result r = ph_call0_r(-1, "get_value");
    ASSERT(!r.ok);
    ASSERT(r.val == NULL);
}

TEST(call0_r_invalid_register_too_large) {
    ph_exec("def get_value(): return 42", "<test>");

    // Register >= PH_MAX_REG should fail gracefully
    ph_Result r = ph_call0_r(8, "get_value");
    ASSERT(!r.ok);
    ASSERT(r.val == NULL);

    r = ph_call0_r(100, "get_value");
    ASSERT(!r.ok);
    ASSERT(r.val == NULL);
}

TEST(call1_r_invalid_register) {
    ph_exec("def double(x): return x * 2", "<test>");

    ph_Result r = ph_call1_r(-1, "double", ph_tmp_int(5));
    ASSERT(!r.ok);

    r = ph_call1_r(8, "double", ph_tmp_int(5));
    ASSERT(!r.ok);
}

TEST(call2_r_invalid_register) {
    ph_exec("def add(a, b): return a + b", "<test>");

    ph_int_r(0, 10);
    ph_int_r(1, 20);

    ph_Result r = ph_call2_r(-1, "add", py_r0());
    ASSERT(!r.ok);

    r = ph_call2_r(8, "add", py_r0());
    ASSERT(!r.ok);
}

TEST(call3_r_invalid_register) {
    ph_exec("def sum3(a, b, c): return a + b + c", "<test>");

    ph_int_r(0, 1);
    ph_int_r(1, 2);
    ph_int_r(2, 3);

    ph_Result r = ph_call3_r(-1, "sum3", py_r0());
    ASSERT(!r.ok);

    r = ph_call3_r(8, "sum3", py_r0());
    ASSERT(!r.ok);
}

TEST(call_r_invalid_register) {
    ph_exec("def identity(x): return x", "<test>");
    py_ItemRef fn = ph_getglobal("identity");
    ASSERT(fn != NULL);

    ph_Result r = ph_call_r(-1, fn, 1, ph_tmp_int(42));
    ASSERT(!r.ok);

    r = ph_call_r(8, fn, 1, ph_tmp_int(42));
    ASSERT(!r.ok);
}

TEST(callmethod0_r_invalid_register) {
    ph_exec("my_list = [1, 2, 3]", "<test>");
    py_ItemRef list = ph_getglobal("my_list");
    ASSERT(list != NULL);

    ph_Result r = ph_callmethod0_r(-1, list, "copy");
    ASSERT(!r.ok);

    r = ph_callmethod0_r(8, list, "copy");
    ASSERT(!r.ok);
}

TEST(callmethod1_r_invalid_register) {
    ph_exec("items = []", "<test>");
    py_ItemRef items = ph_getglobal("items");
    ASSERT(items != NULL);

    ph_Result r = ph_callmethod1_r(-1, items, "append", ph_tmp_int(42));
    ASSERT(!r.ok);

    r = ph_callmethod1_r(8, items, "append", ph_tmp_int(42));
    ASSERT(!r.ok);
}

TEST(call_r_valid_registers_work) {
    // Verify that valid registers still work after adding bounds checks
    ph_exec("def get_num(): return 999", "<test>");

    for (int i = 0; i < PH_MAX_REG; i++) {
        ph_Result r = ph_call0_r(i, "get_num");
        ASSERT(r.ok);
        ASSERT_EQ(py_toint(r.val), 999);
        ASSERT(r.val == py_getreg(i));
    }
}

TEST(callmethod_r_valid_registers_work) {
    ph_exec("text = 'hello'", "<test>");
    py_ItemRef text = ph_getglobal("text");

    for (int i = 0; i < PH_MAX_REG; i++) {
        ph_Result r = ph_callmethod0_r(i, text, "upper");
        ASSERT(r.ok);
        ASSERT_STR_EQ(py_tostr(r.val), "HELLO");
        ASSERT(r.val == py_getreg(i));
    }
}

TEST_SUITE_BEGIN("Register Bounds Checking")
    RUN_TEST(max_reg_constant);
    RUN_TEST(valid_register_indices);
    RUN_TEST(call0_r_invalid_register_negative);
    RUN_TEST(call0_r_invalid_register_too_large);
    RUN_TEST(call1_r_invalid_register);
    RUN_TEST(call2_r_invalid_register);
    RUN_TEST(call3_r_invalid_register);
    RUN_TEST(call_r_invalid_register);
    RUN_TEST(callmethod0_r_invalid_register);
    RUN_TEST(callmethod1_r_invalid_register);
    RUN_TEST(call_r_valid_registers_work);
    RUN_TEST(callmethod_r_valid_registers_work);
TEST_SUITE_END()
