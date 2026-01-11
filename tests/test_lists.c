/*
 * test_lists.c - Tests for ph_list_* helper functions
 *
 * Demonstrates:
 * - Building lists from C arrays
 * - Iterating lists with callbacks
 * - List manipulation patterns
 */

#include "test_common.h"

TEST(list_from_ints) {
    py_i64 values[] = {10, 20, 30, 40, 50};
    ph_list_from_ints(py_r0(), values, 5);

    ASSERT(py_islist(py_r0()));
    ASSERT_EQ(py_list_len(py_r0()), 5);

    ASSERT_EQ(py_toint(py_list_getitem(py_r0(), 0)), 10);
    ASSERT_EQ(py_toint(py_list_getitem(py_r0(), 2)), 30);
    ASSERT_EQ(py_toint(py_list_getitem(py_r0(), 4)), 50);
}

TEST(list_from_ints_empty) {
    ph_list_from_ints(py_r0(), NULL, 0);
    ASSERT(py_islist(py_r0()));
    ASSERT_EQ(py_list_len(py_r0()), 0);
}

TEST(list_from_floats) {
    py_f64 values[] = {1.5, 2.5, 3.5};
    ph_list_from_floats(py_r0(), values, 3);

    ASSERT(py_islist(py_r0()));
    ASSERT_EQ(py_list_len(py_r0()), 3);

    py_f64 diff = py_tofloat(py_list_getitem(py_r0(), 1)) - 2.5;
    ASSERT(diff > -0.0001 && diff < 0.0001);
}

TEST(list_from_strs) {
    const char* values[] = {"apple", "banana", "cherry"};
    ph_list_from_strs(py_r0(), values, 3);

    ASSERT(py_islist(py_r0()));
    ASSERT_EQ(py_list_len(py_r0()), 3);

    ASSERT_STR_EQ(py_tostr(py_list_getitem(py_r0(), 0)), "apple");
    ASSERT_STR_EQ(py_tostr(py_list_getitem(py_r0(), 1)), "banana");
    ASSERT_STR_EQ(py_tostr(py_list_getitem(py_r0(), 2)), "cherry");
}

// Callback context for sum
typedef struct {
    py_i64 sum;
} SumContext;

static bool sum_callback(int index, py_Ref item, void* ctx) {
    (void)index;
    SumContext* sc = (SumContext*)ctx;
    sc->sum += py_toint(item);
    return true;  // Continue iteration
}

TEST(list_foreach_sum) {
    py_i64 values[] = {1, 2, 3, 4, 5};
    ph_list_from_ints(py_r0(), values, 5);

    SumContext ctx = {0};
    bool ok = ph_list_foreach(py_r0(), sum_callback, &ctx);

    ASSERT(ok);
    ASSERT_EQ(ctx.sum, 15);  // 1+2+3+4+5 = 15
}

// Callback that stops early
static bool find_negative_callback(int index, py_Ref item, void* ctx) {
    (void)index;
    int* found_index = (int*)ctx;
    if (py_toint(item) < 0) {
        *found_index = index;
        return false;  // Stop iteration
    }
    return true;
}

TEST(list_foreach_early_exit) {
    py_i64 values[] = {5, 10, -3, 20, 25};
    ph_list_from_ints(py_r0(), values, 5);

    int found = -1;
    bool completed = ph_list_foreach(py_r0(), find_negative_callback, &found);

    ASSERT(!completed);  // Did not complete (stopped early)
    ASSERT_EQ(found, 2);  // Found at index 2
}

// Callback that collects strings
typedef struct {
    char buffer[256];
} JoinContext;

static bool join_callback(int index, py_Ref item, void* ctx) {
    JoinContext* jc = (JoinContext*)ctx;
    if (index > 0) {
        strcat(jc->buffer, ", ");
    }
    strcat(jc->buffer, py_tostr(item));
    return true;
}

TEST(list_foreach_join) {
    const char* values[] = {"a", "b", "c"};
    ph_list_from_strs(py_r0(), values, 3);

    JoinContext ctx = {""};
    bool ok = ph_list_foreach(py_r0(), join_callback, &ctx);

    ASSERT(ok);
    ASSERT_STR_EQ(ctx.buffer, "a, b, c");
}

TEST(list_foreach_empty) {
    ph_list_from_ints(py_r0(), NULL, 0);

    SumContext ctx = {0};
    bool ok = ph_list_foreach(py_r0(), sum_callback, &ctx);

    ASSERT(ok);
    ASSERT_EQ(ctx.sum, 0);
}

TEST(list_in_python) {
    // Build list in C, use in Python
    py_i64 values[] = {2, 4, 6, 8, 10};
    ph_list_from_ints(py_r0(), values, 5);
    ph_setglobal("c_list", py_r0());

    bool ok = ph_eval("sum(c_list)");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 30);

    ok = ph_eval("max(c_list)");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 10);

    ok = ph_eval("len(c_list)");
    ASSERT(ok);
    ASSERT_EQ(py_toint(py_retval()), 5);
}

TEST(list_mixed_creation) {
    // Use low-level API to create mixed-type list
    py_newlist(py_r0());
    py_list_append(py_r0(), ph_int_r(1, 42));
    py_list_append(py_r0(), ph_str_r(2, "hello"));
    py_list_append(py_r0(), ph_float_r(3, 3.14));
    py_list_append(py_r0(), ph_bool_r(4, true));

    ASSERT_EQ(py_list_len(py_r0()), 4);
    ASSERT(py_isint(py_list_getitem(py_r0(), 0)));
    ASSERT(py_isstr(py_list_getitem(py_r0(), 1)));
    ASSERT(py_isfloat(py_list_getitem(py_r0(), 2)));
    ASSERT(py_isbool(py_list_getitem(py_r0(), 3)));
}

TEST_SUITE_BEGIN("List Helpers")
    RUN_TEST(list_from_ints);
    RUN_TEST(list_from_ints_empty);
    RUN_TEST(list_from_floats);
    RUN_TEST(list_from_strs);
    RUN_TEST(list_foreach_sum);
    RUN_TEST(list_foreach_early_exit);
    RUN_TEST(list_foreach_join);
    RUN_TEST(list_foreach_empty);
    RUN_TEST(list_in_python);
    RUN_TEST(list_mixed_creation);
TEST_SUITE_END()
