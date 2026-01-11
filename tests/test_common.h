/*
 * test_common.h - Common test utilities
 */

#pragma once

#include "pktpy_hi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) do { \
    g_tests_run++; \
    printf("  %-40s ", #name); \
    fflush(stdout); \
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
