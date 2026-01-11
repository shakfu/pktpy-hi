/*
 * test_common.h - Common test utilities
 *
 * Each test runs with a clean __main__ namespace to prevent order-dependent
 * behavior and state leakage between tests.
 *
 * Note: pocketpy doesn't support re-initialization after py_finalize(),
 * so we clear the namespace between tests instead of creating fresh interpreters.
 */

#pragma once

#include "pktpy_hi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

// Clear __main__ namespace to isolate tests
// Preserves builtins but removes all user-defined names
static inline void ph_test_reset_namespace(void) {
    // Get list of names to delete (excluding builtins and dunder names)
    // Then delete each one
    ph_exec(
        "_names_to_del = [n for n in list(globals().keys()) "
        "if not n.startswith('__') and n != '_names_to_del']\n"
        "for _n in _names_to_del: del globals()[_n]\n"
        "del _names_to_del, _n",
        "<test_reset>"
    );
}

#define TEST(name) static void test_##name(void)

// Run a single test with clean namespace
// Each test starts with a fresh __main__ module state
#define RUN_TEST(name) do { \
    g_tests_run++; \
    printf("  %-40s ", #name); \
    fflush(stdout); \
    ph_test_reset_namespace(); \
    test_##name(); \
    g_tests_passed++; \
    printf("[PASS]\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL]\n"); \
        printf("    Assertion failed: %s\n", #cond); \
        printf("    At: %s:%d\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("[FAIL]\n"); \
        printf("    Expected: %s == %s\n", #a, #b); \
        printf("    At: %s:%d\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("[FAIL]\n"); \
        printf("    Expected: \"%s\" == \"%s\"\n", (a), (b)); \
        printf("    At: %s:%d\n", __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define TEST_SUITE_BEGIN(name) \
    int main(void) { \
        printf("=== %s ===\n", name); \
        py_initialize();

#define TEST_SUITE_END() \
        py_finalize(); \
        printf("Passed %d/%d tests\n", g_tests_passed, g_tests_run); \
        return (g_tests_passed == g_tests_run) ? 0 : 1; \
    }
