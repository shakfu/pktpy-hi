/*
 * basic_usage.c - Demonstrates ph.h wrapper usage
 *
 * Compile: cc -o basic_usage basic_usage.c -I../include -I../pocketpy-2.1.6 \
 *          ../pocketpy-2.1.6/pocketpy.c -lm
 */

#include "pktpy_hi.h"
#include <stdio.h>

// Example native function using PH_ macros
static bool my_add(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    PH_ARG_INT(0, a);
    PH_ARG_INT(1, b);
    PH_RETURN_INT(a + b);
}

// Example with optional argument
static bool my_greet(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(1);
    PH_ARG_STR(0, name);
    printf("Hello, %s!\n", name);
    PH_RETURN_NONE;
}

int main(void) {
    // Initialize pocketpy
    py_initialize();

    // --- Scope management example ---
    printf("=== Scope Management ===\n");
    {
        ph_Scope scope = ph_scope_begin();
        py_exec("x = 1 + 2", "<test>", EXEC_MODE, NULL);
        if (ph_scope_end(&scope)) {
            printf("Execution succeeded\n");
        }
    }

    // --- Safe execution example ---
    printf("\n=== Safe Execution ===\n");
    ph_exec("print('Hello from ph_exec!')", "<test>");

    // Exception handling (division by zero)
    printf("\n=== Exception Handling ===\n");
    if (!ph_exec("result = 1 / 0", "<test>")) {
        printf("(Exception was caught and printed above)\n");
    }

    // --- Value creation example ---
    printf("\n=== Value Creation ===\n");
    ph_setglobal("my_int", ph_tmp_int(42));
    ph_setglobal("my_str", ph_tmp_str("hello"));
    ph_exec("print(f'my_int = {my_int}, my_str = {my_str}')", "<test>");

    // Using different registers
    ph_setglobal("a", ph_int_r(0, 10));
    ph_setglobal("b", ph_int_r(1, 20));
    ph_exec("print(f'a + b = {a + b}')", "<test>");

    // --- Binding functions example ---
    printf("\n=== Function Binding ===\n");
    ph_def("my_add(a, b)", my_add);
    ph_def("greet(name)", my_greet);

    ph_exec("print(f'my_add(3, 4) = {my_add(3, 4)}')", "<test>");
    ph_exec("greet('World')", "<test>");

    // --- Function calls from C ---
    printf("\n=== Calling Python from C ===\n");
    ph_exec("def double(x): return x * 2", "<test>");

    ph_Result r = ph_call1("double", ph_tmp_int(21));
    if (r.ok) {
        printf("double(21) = %lld\n", py_toint(r.val));
    }

    // --- Evaluation example ---
    printf("\n=== Evaluation ===\n");
    if (ph_eval("2 ** 10")) {
        printf("2 ** 10 = %lld\n", py_toint(py_retval()));
    }

    // --- Value extraction with defaults ---
    printf("\n=== Value Extraction ===\n");
    py_ItemRef val = ph_getglobal("my_int");
    if (val) {
        py_i64 x = ph_as_int(val, -1);
        printf("my_int = %lld\n", x);
    }

    // Default value when type doesn't match
    py_i64 y = ph_as_int(ph_tmp_str("not an int"), 999);
    printf("ph_as_int on string = %lld (default)\n", y);

    // --- Debug helpers ---
    printf("\n=== Debug Helpers ===\n");
    ph_exec("my_list = [1, 2, 3]", "<test>");
    py_ItemRef list = ph_getglobal("my_list");
    if (list) {
        printf("my_list repr: %s\n", ph_repr(list));
        printf("my_list type: %s\n", ph_typename(list));
    }

    // --- Interoperability with low-level API ---
    printf("\n=== Interoperability ===\n");
    // Can freely mix ph_* with py_* functions
    py_newlist(py_r0());
    py_list_append(py_r0(), ph_int_r(1, 100));
    py_list_append(py_r0(), ph_int_r(2, 200));
    py_list_append(py_r0(), ph_int_r(3, 300));
    ph_setglobal("mixed_list", py_r0());
    ph_exec("print(f'mixed_list = {mixed_list}')", "<test>");

    printf("\nAll examples completed.\n");

    py_finalize();
    return 0;
}
