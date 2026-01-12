/*
 * test_values.c - Tests for ph_ value creation functions
 *
 * Demonstrates:
 * - Creating int, float, str, bool values
 * - Using different registers
 * - Return-by-value pattern vs py_OutRef
 */

#include "test_common.h"

TEST(create_int) {
    py_GlobalRef val = ph_tmp_int(42);
    ASSERT(py_isint(val));
    ASSERT_EQ(py_toint(val), 42);
}

TEST(create_int_negative) {
    py_GlobalRef val = ph_tmp_int(-12345);
    ASSERT(py_isint(val));
    ASSERT_EQ(py_toint(val), -12345);
}

TEST(create_float) {
    py_GlobalRef val = ph_tmp_float(3.14159);
    ASSERT(py_isfloat(val));
    // Float comparison with tolerance
    py_f64 diff = py_tofloat(val) - 3.14159;
    ASSERT(diff > -0.00001 && diff < 0.00001);
}

TEST(create_str) {
    py_GlobalRef val = ph_tmp_str("hello");
    ASSERT(py_isstr(val));
    ASSERT_STR_EQ(py_tostr(val), "hello");
}

TEST(create_str_empty) {
    py_GlobalRef val = ph_tmp_str("");
    ASSERT(py_isstr(val));
    ASSERT_STR_EQ(py_tostr(val), "");
}

TEST(create_bool_true) {
    py_GlobalRef val = ph_tmp_bool(true);
    ASSERT(py_isbool(val));
    ASSERT(py_tobool(val) == true);
}

TEST(create_bool_false) {
    py_GlobalRef val = ph_tmp_bool(false);
    ASSERT(py_isbool(val));
    ASSERT(py_tobool(val) == false);
}

TEST(create_with_register) {
    // Create values in different registers
    py_GlobalRef a = ph_int_r(0, 10);
    py_GlobalRef b = ph_int_r(1, 20);
    py_GlobalRef c = ph_int_r(2, 30);

    // All should be valid and distinct
    ASSERT_EQ(py_toint(a), 10);
    ASSERT_EQ(py_toint(b), 20);
    ASSERT_EQ(py_toint(c), 30);

    // They should be in different registers
    ASSERT(a != b);
    ASSERT(b != c);
}

TEST(create_str_with_register) {
    py_GlobalRef s1 = ph_str_r(0, "first");
    py_GlobalRef s2 = ph_str_r(1, "second");

    ASSERT_STR_EQ(py_tostr(s1), "first");
    ASSERT_STR_EQ(py_tostr(s2), "second");
}

TEST(create_float_with_register) {
    py_GlobalRef f1 = ph_float_r(0, 1.5);
    py_GlobalRef f2 = ph_float_r(1, 2.5);

    py_f64 diff1 = py_tofloat(f1) - 1.5;
    py_f64 diff2 = py_tofloat(f2) - 2.5;
    ASSERT(diff1 > -0.00001 && diff1 < 0.00001);
    ASSERT(diff2 > -0.00001 && diff2 < 0.00001);
}

TEST(setglobal_with_value) {
    // Combine value creation with setting globals
    ph_setglobal("my_num", ph_tmp_int(999));
    ph_setglobal("my_text", ph_tmp_str("test string"));

    bool ok = ph_eval("my_num + 1");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 1000);

    ok = ph_eval("my_text.upper()");
    ASSERT(ok);
    ASSERT_STR_EQ(py_tostr(py_retval()), "TEST STRING");
}

TEST(overwrite_register) {
    // Creating a new value in r0 overwrites previous
    ph_tmp_int(100);
    py_i64 first = py_toint(py_r0());

    ph_tmp_int(200);
    py_i64 second = py_toint(py_r0());

    ASSERT_EQ(first, 100);
    ASSERT_EQ(second, 200);
}

TEST_SUITE_BEGIN("Value Creation")
    RUN_TEST(create_int);
    RUN_TEST(create_int_negative);
    RUN_TEST(create_float);
    RUN_TEST(create_str);
    RUN_TEST(create_str_empty);
    RUN_TEST(create_bool_true);
    RUN_TEST(create_bool_false);
    RUN_TEST(create_with_register);
    RUN_TEST(create_str_with_register);
    RUN_TEST(create_float_with_register);
    RUN_TEST(setglobal_with_value);
    RUN_TEST(overwrite_register);
TEST_SUITE_END()
