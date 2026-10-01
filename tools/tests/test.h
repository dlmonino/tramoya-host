/* test.h - minimal test harness: registry + assertions + TAP output.
 *
 * The registry and counters live in test_main.c (single definitions);
 * every test TU registers its cases via REGISTER_TEST and asserts via
 * CHECK*. No framework dependencies.
 */
#ifndef WK_TEST_H
#define WK_TEST_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

typedef void (*test_fn)(void);

typedef struct {
    const char *name;
    test_fn fn;
} test_case_t;

#define MAX_TESTS 256

/* Defined in test_main.c. */
extern test_case_t g_tests[MAX_TESTS];
extern int g_test_count;
extern int g_assert_failures;
extern int g_assert_count;
extern const char *g_current_test;
extern const size_t g_frontend_zip_file_count;
extern const size_t g_frontend_zip_payload_size;

#define REGISTER_TEST(name_, fn_) \
    static void fn_(void); \
    __attribute__((constructor)) static void reg_##fn_(void) { \
        if (g_test_count < MAX_TESTS) { \
            g_tests[g_test_count].name = (name_); \
            g_tests[g_test_count].fn = (fn_); \
            g_test_count++; \
        } \
    } \
    static void fn_(void)

/* --- assertions ---------------------------------------------------- */

#define CHECK(cond) do { \
    g_assert_count++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s: %s:%d: CHECK(%s)\n", \
                g_current_test, __FILE__, __LINE__, #cond); \
        g_assert_failures++; \
    } \
} while (0)

#define CHECK_EQ_STR(a, b) do { \
    g_assert_count++; \
    const char *va = (a), *vb = (b); \
    if (!va || !vb || strcmp(va, vb) != 0) { \
        fprintf(stderr, "FAIL %s: %s:%d: \"%s\" != \"%s\"\n", \
                g_current_test, __FILE__, __LINE__, va ? va : "(null)", vb ? vb : "(null)"); \
        g_assert_failures++; \
    } \
} while (0)

#define CHECK_EQ_INT(a, b) do { \
    g_assert_count++; \
    long long va = (long long)(a), vb = (long long)(b); \
    if (va != vb) { \
        fprintf(stderr, "FAIL %s: %s:%d: %lld != %lld\n", \
                g_current_test, __FILE__, __LINE__, va, vb); \
        g_assert_failures++; \
    } \
} while (0)

#define CHECK_MEM_EQ(a, b, len) do { \
    g_assert_count++; \
    if (memcmp((a), (b), (len)) != 0) { \
        fprintf(stderr, "FAIL %s: %s:%d: memory mismatch (%zu bytes)\n", \
                g_current_test, __FILE__, __LINE__, (size_t)(len)); \
        g_assert_failures++; \
    } \
} while (0)

#endif
