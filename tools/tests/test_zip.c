/* Protocol compatibility tests using fixed project-authored cases. */
#include "test.h"
#include "wk_zip.h"
#include "path_cases.h"

static void test_map_generated_table(void)
{
    for (int i = 0; i < ref_zip_case_count; i++) {
        char out[256] = "UNSET";
        bool ok = wk_zip_map_path(ref_zip_cases[i].uri, out, sizeof(out));
        if (!ok) {
            fprintf(stderr, "FAIL %s: mapping rejected: %s\n",
                    g_current_test, ref_zip_cases[i].uri);
            g_assert_failures++;
            continue;
        }
        CHECK_EQ_STR(out, ref_zip_cases[i].expect);
    }
}

static void test_map_handwritten(void)
{
    char out[256];

    /* Absolute /app stays "app" (no trailing slash -> no rewrite). */
    CHECK(wk_zip_map_path("/app", out, sizeof(out)));
    CHECK_EQ_STR(out, "app");

    /* Query stripped before decode+normalize. */
    CHECK(wk_zip_map_path("/index.html?next=%2e%2e%2fetc", out, sizeof(out)));
    CHECK_EQ_STR(out, "index.html");

    /* Empty result for "/" is a valid mapping. */
    CHECK(wk_zip_map_path("/", out, sizeof(out)));
    CHECK_EQ_STR(out, "");
}

static void test_zip_init_from_bytes(void)
{
    extern void *test_build_mini_zip(size_t *len_out);
    size_t zlen = 0;
    void *zbuf = test_build_mini_zip(&zlen);
    CHECK(zbuf != NULL);
    if (!zbuf) return;

    wk_zip_t zip;
    CHECK_EQ_INT(wk_zip_init(&zip, (const uint8_t *)zbuf, zlen), 0);
    if (zip.initialized) {
        CHECK_EQ_INT(zip.count, 3);
        const wk_zip_entry_t *a = wk_zip_find(&zip, "hello.txt");
        const wk_zip_entry_t *b = wk_zip_find(&zip, "dir/world.txt");
        const wk_zip_entry_t *c = wk_zip_find(&zip, "compressed.js");
        const wk_zip_entry_t *missing = wk_zip_find(&zip, "nope.txt");
        CHECK(a != NULL);
        CHECK(b != NULL);
        CHECK(c != NULL);
        CHECK(missing == NULL);
        if (a && b && c) {
            CHECK_EQ_INT(a->uncomp_size, 5);
            CHECK_EQ_INT(a->method, 0);
            CHECK_EQ_INT(b->uncomp_size, 5);
            CHECK_EQ_INT(c->uncomp_size, 12);
            CHECK_EQ_INT(c->method, 8);
            CHECK_EQ_INT(c->comp_size, 14);
            uint8_t ra[8], rb[8];
            CHECK_EQ_INT(wk_zip_read_entry(&zip, a, 0, ra, 5), 5);
            CHECK_EQ_INT(wk_zip_read_entry(&zip, b, 0, rb, 5), 5);
            CHECK_MEM_EQ(ra, "hello", 5);
            CHECK_MEM_EQ(rb, "world", 5);
        }
        wk_zip_deinit(&zip);
    }
    free(zbuf);
}

static void test_zip_rejects_garbage(void)
{
    wk_zip_t zip;
    uint8_t garbage[64];
    memset(garbage, 0xAB, sizeof(garbage));
    CHECK_EQ_INT(wk_zip_init(&zip, garbage, sizeof(garbage)), -1);
    CHECK(!zip.initialized);
}

REGISTER_TEST("zip: path mapping vs host.py generated table", test_map_generated_table);
REGISTER_TEST("zip: handwritten path-mapping sanity cases", test_map_handwritten);
REGISTER_TEST("zip: mini archive index + STORE bytes", test_zip_init_from_bytes);
REGISTER_TEST("zip: garbage input rejected", test_zip_rejects_garbage);

static void test_zip_malformed_directory(void)
{
    extern void *test_build_mini_zip(size_t *len_out);
    size_t len;
    uint8_t *original = test_build_mini_zip(&len);
    CHECK(original != NULL);
    if (!original) return;
    uint8_t *copy = malloc(len);
    uint8_t *end = original + len - 22;
    uint32_t cd = (uint32_t)end[16] | (uint32_t)end[17] << 8 |
                  (uint32_t)end[18] << 16 | (uint32_t)end[19] << 24;
    /* Truncated inputs used to get past the signature and overrun the CD. */
    for (size_t size = 0; size < len; size++) {
        wk_zip_t zip;
        CHECK_EQ_INT(wk_zip_init(&zip, original, size), -1);
        wk_zip_deinit(&zip);
    }
    const size_t offsets[] = {len - 22 + 10, len - 22 + 12, cd + 28, cd + 42, 0, cd + 10};
    const uint8_t values[] = {99, 1, 255, 255, 0, 99};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        memcpy(copy, original, len);
        copy[offsets[i]] = values[i];
        wk_zip_t zip;
        CHECK_EQ_INT(wk_zip_init(&zip, copy, len), -1);
        CHECK(!zip.initialized);
        wk_zip_deinit(&zip);
    }
    free(copy);
    free(original);
}
REGISTER_TEST("zip: truncated and malformed directories rejected safely", test_zip_malformed_directory);
