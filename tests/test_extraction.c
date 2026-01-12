/*
 * test_extraction.c - Tests for ph_as_* value extraction functions
 *
 * Demonstrates:
 * - Safe value extraction with defaults
 * - Type coercion behavior
 * - Truthy checking
 */

#include "test_common.h"

TEST(as_int_valid) {
    py_GlobalRef val = ph_tmp_int(42);
    py_i64 result = ph_as_int(val, -1);
    ASSERT_EQ(result, 42);
}

TEST(as_int_default) {
    // Non-int returns default
    py_GlobalRef val = ph_tmp_str("not an int");
    py_i64 result = ph_as_int(val, -999);
    ASSERT_EQ(result, -999);
}

TEST(as_int_from_float) {
    // Float should return default (no implicit conversion)
    py_GlobalRef val = ph_tmp_float(3.14);
    py_i64 result = ph_as_int(val, -1);
    ASSERT_EQ(result, -1);  // Returns default, not 3
}

TEST(as_float_valid) {
    py_GlobalRef val = ph_tmp_float(2.718);
    py_f64 result = ph_as_float(val, -1.0);
    py_f64 diff = result - 2.718;
    ASSERT(diff > -0.0001 && diff < 0.0001);
}

TEST(as_float_from_int) {
    // Int should be coerced to float
    py_GlobalRef val = ph_tmp_int(42);
    py_f64 result = ph_as_float(val, -1.0);
    py_f64 diff = result - 42.0;
    ASSERT(diff > -0.0001 && diff < 0.0001);
}

TEST(as_float_default) {
    py_GlobalRef val = ph_tmp_str("not a number");
    py_f64 result = ph_as_float(val, -999.0);
    py_f64 diff = result - (-999.0);
    ASSERT(diff > -0.0001 && diff < 0.0001);
}

TEST(as_str_valid) {
    py_GlobalRef val = ph_tmp_str("hello");
    const char* result = ph_as_str(val, "default");
    ASSERT_STR_EQ(result, "hello");
}

TEST(as_str_default) {
    py_GlobalRef val = ph_tmp_int(42);
    const char* result = ph_as_str(val, "default");
    ASSERT_STR_EQ(result, "default");
}

TEST(as_str_empty) {
    py_GlobalRef val = ph_tmp_str("");
    const char* result = ph_as_str(val, "default");
    ASSERT_STR_EQ(result, "");  // Empty string is still valid
}

TEST(as_bool_true) {
    py_GlobalRef val = ph_tmp_bool(true);
    bool result = ph_as_bool(val, false);
    ASSERT(result == true);
}

TEST(as_bool_false) {
    py_GlobalRef val = ph_tmp_bool(false);
    bool result = ph_as_bool(val, true);
    ASSERT(result == false);
}

TEST(as_bool_default) {
    py_GlobalRef val = ph_tmp_int(1);  // Not a bool type
    bool result = ph_as_bool(val, true);
    ASSERT(result == true);  // Returns default
}

TEST(is_truthy_int) {
    ASSERT(ph_is_truthy(ph_tmp_int(1)) == true);
    ASSERT(ph_is_truthy(ph_tmp_int(0)) == false);
    ASSERT(ph_is_truthy(ph_tmp_int(-1)) == true);
}

TEST(is_truthy_str) {
    // Use separate registers to avoid r0 being overwritten
    py_GlobalRef hello = ph_str_r(1, "hello");
    py_GlobalRef empty = ph_str_r(2, "");

    ASSERT(ph_is_truthy(hello) == true);
    ASSERT(ph_is_truthy(empty) == false);
}

TEST(is_truthy_bool) {
    ASSERT(ph_is_truthy(ph_tmp_bool(true)) == true);
    ASSERT(ph_is_truthy(ph_tmp_bool(false)) == false);
}

TEST(is_truthy_list) {
    ph_exec("empty_list = []", "<test>");
    ph_exec("full_list = [1, 2, 3]", "<test>");

    py_ItemRef empty = ph_getglobal("empty_list");
    py_ItemRef full = ph_getglobal("full_list");

    ASSERT(ph_is_truthy(empty) == false);
    ASSERT(ph_is_truthy(full) == true);
}

TEST(is_truthy_raise_success) {
    // ph_is_truthy_raise returns 1 for truthy, 0 for falsy
    int result = ph_is_truthy_raise(ph_tmp_int(42));
    ASSERT(result > 0);

    result = ph_is_truthy_raise(ph_tmp_int(0));
    ASSERT(result == 0);

    result = ph_is_truthy_raise(ph_str_r(1, "hello"));
    ASSERT(result > 0);

    result = ph_is_truthy_raise(ph_str_r(2, ""));
    ASSERT(result == 0);
}

TEST(is_truthy_raise_exception) {
    // Create an object with a __bool__ that raises
    ph_exec(
        "class BadBool:\n"
        "    def __bool__(self):\n"
        "        raise ValueError('bool failed')\n"
        "bad_obj = BadBool()",
        "<test>"
    );

    py_ItemRef bad_obj = ph_getglobal("bad_obj");
    ASSERT(bad_obj != NULL);

    int result = ph_is_truthy_raise(bad_obj);
    ASSERT(result < 0);  // -1 indicates error
    ASSERT(py_checkexc());  // Exception should be set
    ASSERT(py_matchexc(tp_ValueError));

    py_clearexc(NULL);
}

TEST(is_none) {
    ph_exec("none_val = None", "<test>");
    ph_exec("some_val = 42", "<test>");

    py_ItemRef none_val = ph_getglobal("none_val");
    py_ItemRef some_val = ph_getglobal("some_val");

    ASSERT(ph_is_none(none_val) == true);
    ASSERT(ph_is_none(some_val) == false);
}

TEST(is_nil) {
    // nil is used for unset/invalid values
    py_ItemRef undefined = ph_getglobal("undefined_var_12345");
    ASSERT(undefined == NULL);

    // Valid value is not nil
    ph_setglobal("defined_var", ph_tmp_int(1));
    py_ItemRef defined = ph_getglobal("defined_var");
    ASSERT(defined != NULL);
    ASSERT(ph_is_nil(defined) == false);
}

TEST(extraction_chain) {
    // Demonstrate extraction in a realistic scenario
    ph_exec(
        "config = {\n"
        "    'port': 8080,\n"
        "    'host': 'localhost',\n"
        "    'debug': True,\n"
        "    'timeout': 30.5\n"
        "}\n",
        "<test>"
    );

    py_ItemRef config = ph_getglobal("config");
    ASSERT(config != NULL);

    // Extract with defaults
    ph_Scope scope = ph_scope_begin();

    py_dict_getitem_by_str(config, "port");
    py_i64 port = ph_as_int(py_retval(), 80);
    ASSERT_EQ(port, 8080);

    py_dict_getitem_by_str(config, "host");
    const char* host = ph_as_str(py_retval(), "0.0.0.0");
    ASSERT_STR_EQ(host, "localhost");

    py_dict_getitem_by_str(config, "debug");
    bool debug = ph_as_bool(py_retval(), false);
    ASSERT(debug == true);

    py_dict_getitem_by_str(config, "timeout");
    py_f64 timeout = ph_as_float(py_retval(), 60.0);
    py_f64 diff = timeout - 30.5;
    ASSERT(diff > -0.0001 && diff < 0.0001);

    // Non-existent key uses default
    int found = py_dict_getitem_by_str(config, "max_connections");
    py_i64 max_conn = (found == 1) ? ph_as_int(py_retval(), 100) : 100;
    ASSERT_EQ(max_conn, 100);

    ph_scope_end(&scope);
}

TEST_SUITE_BEGIN("Value Extraction")
    RUN_TEST(as_int_valid);
    RUN_TEST(as_int_default);
    RUN_TEST(as_int_from_float);
    RUN_TEST(as_float_valid);
    RUN_TEST(as_float_from_int);
    RUN_TEST(as_float_default);
    RUN_TEST(as_str_valid);
    RUN_TEST(as_str_default);
    RUN_TEST(as_str_empty);
    RUN_TEST(as_bool_true);
    RUN_TEST(as_bool_false);
    RUN_TEST(as_bool_default);
    RUN_TEST(is_truthy_int);
    RUN_TEST(is_truthy_str);
    RUN_TEST(is_truthy_bool);
    RUN_TEST(is_truthy_list);
    RUN_TEST(is_truthy_raise_success);
    RUN_TEST(is_truthy_raise_exception);
    RUN_TEST(is_none);
    RUN_TEST(is_nil);
    RUN_TEST(extraction_chain);
TEST_SUITE_END()
