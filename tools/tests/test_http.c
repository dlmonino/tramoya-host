/* test_http.c - HTTP serving core tests over a real frontend.zip.
 *
 * Exercises wk_http_serve_get end to end through a memory sink:
 *   - virtual selected_exploit file,
 *   - /app + /document mappings resolve to real entries,
 *   - DEFLATE entries stream to exactly their declared size,
 *   - 404s for absent paths,
 *   - every DEFLATE entry in the real archive inflates (zlib cross-check).
 */
#include "test.h"
#include "wk_http.h"
#include "wk_zip.h"
#include "test_mem_sink.h"

#include <zlib.h>

/* Provided by test_http_zip.c (generated when frontend.zip is embedded). */
extern const uint8_t g_frontend_zip[];
extern const size_t g_frontend_zip_len;

static void init_zip(wk_zip_t *zip)
{
    int rc = wk_zip_init(zip, g_frontend_zip, g_frontend_zip_len);
    if (rc != 0) {
        fprintf(stderr, "FAIL %s: cannot index embedded frontend.zip\n", g_current_test);
        g_assert_failures++;
    }
}

/* --- cases ---------------------------------------------------------- */

static void test_selected_exploit(void)
{
    wk_zip_t zip;
    init_zip(&zip);
    if (!zip.initialized) return;

    uint8_t buf[64];
    mem_sink_t sink = { .buf = buf, .cap = sizeof(buf) };
    wk_http_status_t rc = wk_http_serve_get(&zip, "/selected_exploit",
                                            "relapse\n", &mem_io, &sink);
    CHECK_EQ_INT(rc, WK_HTTP_OK);
    CHECK(sink.begun && sink.ended);
    CHECK_EQ_STR(sink.ctype, "text/plain");
    CHECK_EQ_INT(sink.len, 8);
    CHECK_MEM_EQ(buf, "relapse\n", 8);
    wk_zip_deinit(&zip);
}

static void test_selected_exploit_variant_paths(void)
{
    wk_zip_t zip;
    init_zip(&zip);
    if (!zip.initialized) return;

    /* Anything ending in selected_exploit is the virtual file (host.py
     * matches the tail segment). */
    const char *uris[] = {
        "/selected_exploit",
        "/app/selected_exploit",
        "/document/en/ps5/selected_exploit",
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        uint8_t buf[64];
        mem_sink_t sink = { .buf = buf, .cap = sizeof(buf) };
        CHECK_EQ_INT(wk_http_serve_get(&zip, uris[i], "poops\n", &mem_io, &sink),
                     WK_HTTP_OK);
        CHECK_EQ_INT(sink.len, 6);
    }
    wk_zip_deinit(&zip);
}

static void test_index_resolution(void)
{
    wk_zip_t zip;
    init_zip(&zip);
    if (!zip.initialized) return;

    /* "/" and "/app/" resolve to index.html of the autoloader root. */
    const char *uris[] = { "/", "/app/", "/document/en/ps5/" };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        uint8_t *buf = malloc(64 * 1024);
        mem_sink_t sink = { .buf = buf, .cap = 64 * 1024 };
        CHECK_EQ_INT(wk_http_serve_get(&zip, uris[i], "relapse\n", &mem_io, &sink),
                     WK_HTTP_OK);
        CHECK_EQ_INT(sink.declared, sink.len);
        CHECK(sink.len > 0);
        free(buf);
    }
    wk_zip_deinit(&zip);
}

static void test_payload_elf_streams(void)
{
    wk_zip_t zip;
    init_zip(&zip);
    if (!zip.initialized) return;

    const wk_zip_entry_t *e = wk_zip_find(&zip, "payloads/payload.elf");
    CHECK(e != NULL);
    if (!e) { wk_zip_deinit(&zip); return; }

    uint8_t *buf = malloc(e->uncomp_size + 16);
    CHECK(buf != NULL);
    if (!buf) { wk_zip_deinit(&zip); return; }

    mem_sink_t sink = { .buf = buf, .cap = e->uncomp_size + 16 };
    wk_http_status_t rc = wk_http_serve_get(&zip, "/payloads/payload.elf",
                                            "relapse\n", &mem_io, &sink);
    CHECK_EQ_INT(rc, WK_HTTP_OK);
    CHECK(sink.streamed);           /* DEFLATE entry -> streamed */
    CHECK_EQ_INT(sink.len, e->uncomp_size);
    CHECK_EQ_INT(sink.declared, e->uncomp_size);

    /* Cross-check with zlib (independent inflate implementation; zip
     * entries are raw DEFLATE, so inflateInit2 with -15). */
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    int zrc = inflateInit2(&zs, -15);
    CHECK_EQ_INT(zrc, Z_OK);
    if (zrc == Z_OK) {
        uint8_t *zout = malloc(e->uncomp_size);
        static uint8_t zsrc_buf[3 * 1024 * 1024];
        uint32_t zoff = 0;
        while (zoff < e->comp_size) {
            int n = wk_zip_read_entry(&zip, e, zoff, zsrc_buf + zoff,
                                      e->comp_size - zoff);
            if (n <= 0) break;
            zoff += (uint32_t)n;
        }
        zs.next_in = zsrc_buf;
        zs.avail_in = (uInt)e->comp_size;
        zs.next_out = zout;
        zs.avail_out = (uInt)e->uncomp_size;
        zrc = inflate(&zs, Z_FINISH);
        CHECK_EQ_INT(zrc, Z_STREAM_END);
        CHECK_EQ_INT((long long)zs.total_out, (long long)e->uncomp_size);
        if (zrc == Z_STREAM_END && zs.total_out == e->uncomp_size) {
            CHECK_MEM_EQ(buf, zout, e->uncomp_size);
        }
        inflateEnd(&zs);
        free(zout);
    }
    free(buf);
    wk_zip_deinit(&zip);
}

static void test_404(void)
{
    wk_zip_t zip;
    init_zip(&zip);
    if (!zip.initialized) return;

    const char *uris[] = { "/no/such/file.js", "/payloads/missing.elf",
                           "/../etc/passwd" };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        uint8_t buf[64];
        mem_sink_t sink = { .buf = buf, .cap = sizeof(buf) };
        CHECK_EQ_INT(wk_http_serve_get(&zip, uris[i], "relapse\n", &mem_io, &sink),
                     WK_HTTP_NOT_FOUND);
        CHECK(!sink.begun);
    }
    wk_zip_deinit(&zip);
}

static void test_every_deflate_entry(void)
{
    wk_zip_t zip;
    init_zip(&zip);
    if (!zip.initialized) return;

    size_t checked = 0, failed = 0;
    for (int i = 0; i < zip.count; i++) {
        const wk_zip_entry_t *e = wk_zip_at(&zip, i);
        if (e->method != 8) continue;
        uint8_t *buf = malloc(e->uncomp_size + 16);
        if (!buf) { failed++; continue; }
        mem_sink_t sink = { .buf = buf, .cap = e->uncomp_size + 16 };
        /* serve_get needs a URI; drive the inflate path directly. */
        wk_http_status_t rc = wk_http_inflate_entry(&zip, e, &mem_io, &sink);
        if (rc != WK_HTTP_OK || sink.len != e->uncomp_size) {
            fprintf(stderr, "FAIL %s: inflate %s: rc=%d len=%zu want=%u\n",
                    g_current_test, e->name, rc, sink.len, e->uncomp_size);
            failed++;
        } else {
            /* crc check with zlib's crc32 */
            uLong crc = crc32(0L, Z_NULL, 0);
            crc = crc32(crc, buf, e->uncomp_size);
            if (crc != e->crc32) {
                fprintf(stderr, "FAIL %s: crc mismatch %s\n", g_current_test, e->name);
                failed++;
            }
        }
        free(buf);
        checked++;
    }
    CHECK_EQ_INT(checked, g_frontend_zip_file_count);
    CHECK(wk_zip_find(&zip, "logo.svg") != NULL);
    CHECK(wk_zip_find(&zip, "favicon.svg") != NULL);
    CHECK_EQ_INT((long long)failed, 0);
    wk_zip_deinit(&zip);
}

static void test_content_types(void)
{
    CHECK_EQ_STR(wk_http_content_type("index.html"), "text/html; charset=utf-8");
    CHECK_EQ_STR(wk_http_content_type("app.js"), "application/javascript; charset=utf-8");
    CHECK_EQ_STR(wk_http_content_type("style.css"), "text/css; charset=utf-8");
    CHECK_EQ_STR(wk_http_content_type("logo.svg"), "image/svg+xml");
    CHECK_EQ_STR(wk_http_content_type("payload.elf"), "application/octet-stream");
    CHECK_EQ_STR(wk_http_content_type("kexp-ps5.bin"), "application/octet-stream");
    CHECK_EQ_STR(wk_http_content_type("noextension"), "application/octet-stream");
}

REGISTER_TEST("http: selected_exploit virtual file", test_selected_exploit);
REGISTER_TEST("http: selected_exploit tail-match variants", test_selected_exploit_variant_paths);
REGISTER_TEST("http: index.html resolution for / and /app/", test_index_resolution);
REGISTER_TEST("http: payload.elf streams and matches zlib", test_payload_elf_streams);
REGISTER_TEST("http: absent paths 404 without response", test_404);
REGISTER_TEST("http: every DEFLATE entry inflates with matching CRC", test_every_deflate_entry);
REGISTER_TEST("http: content-type mapping", test_content_types);
