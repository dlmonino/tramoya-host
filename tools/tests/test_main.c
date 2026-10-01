/* test_main.c - test runner (TAP output). */
#include "test.h"

test_case_t g_tests[MAX_TESTS];
int g_test_count = 0;
int g_assert_failures = 0;
int g_assert_count = 0;
const char *g_current_test = "";

int main(void)
{
    printf("1..%d\n", g_test_count);
    int failed = 0;
    for (int i = 0; i < g_test_count; i++) {
        g_current_test = g_tests[i].name;
        int before = g_assert_failures;
        g_tests[i].fn();
        if (g_assert_failures != before) {
            failed++;
            printf("not ok %d - %s\n", i + 1, g_tests[i].name);
        } else {
            printf("ok %d - %s\n", i + 1, g_tests[i].name);
        }
        fflush(stdout);
    }
    printf("# %d tests, %d assertions, %d failures\n",
           g_test_count, g_assert_count, g_assert_failures);
    return failed ? 1 : 0;
}
