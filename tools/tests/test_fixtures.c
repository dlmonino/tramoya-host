/* test_fixtures.c - shared test fixtures.
 *
 * - test_build_mini_zip(): a tiny 3-entry zip built by hand (2 STORE,
 *   1 DEFLATE) exercising the index scan without needing frontend.zip.
 * - test_zip_bytes(): access to the embedded frontend.zip bytes. When the
 *   frontend.zip is available at build time, test_frontend_zip.c is
 *   generated with the real archive; otherwise a stub returns NULL so the
 *   dependent tests fail loudly.
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* --- hand-built mini zip -------------------------------------------- */

static void put16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = v >> 24;
}

void *test_build_mini_zip(size_t *len_out)
{
    /* 3 entries: hello.txt (STORE, "hello"), dir/world.txt (STORE, "world"),
     * compressed.js (DEFLATE, "hello world!"). */
    static const char *names[3] = { "hello.txt", "dir/world.txt", "compressed.js" };
    static const uint8_t *data[3] = {
        (const uint8_t *)"hello", (const uint8_t *)"world",
        (const uint8_t *)"\xcb\x48\xcd\xc9\xc9\x57\x28\xcf\x2f\xca\x49\x51\x04\x00",
    };
    static const size_t comp[3] = { 5, 5, 14 };
    static const size_t uncomp[3] = { 5, 5, 12 };
    static const uint16_t method[3] = { 0, 0, 8 };
    static const uint32_t crc[3] = { 0x5d41402au, 0x94373069u, 0x03b4c26du };

    uint8_t *zip = calloc(1, 4096);
    if (!zip) return NULL;
    size_t off = 0;
    uint32_t local_off[3];

    for (int i = 0; i < 3; i++) {
        local_off[i] = (uint32_t)off;
        uint8_t *lh = zip + off;
        put32(lh, 0x04034b50);
        put16(lh + 4, 20);             /* version needed */
        put16(lh + 6, 0);              /* flags */
        put16(lh + 8, method[i]);
        put16(lh + 10, 0);             /* mod time */
        put16(lh + 12, 0);             /* mod date */
        put32(lh + 14, crc[i]);
        put32(lh + 18, (uint32_t)comp[i]);
        put32(lh + 22, (uint32_t)uncomp[i]);
        put16(lh + 26, (uint16_t)strlen(names[i]));
        put16(lh + 28, 0);             /* extra len */
        off += 30;
        memcpy(zip + off, names[i], strlen(names[i]));
        off += strlen(names[i]);
        memcpy(zip + off, data[i], comp[i]);
        off += comp[i];
    }

    uint32_t cd_off = (uint32_t)off;
    for (int i = 0; i < 3; i++) {
        uint8_t *cd = zip + off;
        put32(cd, 0x02014b50);
        put16(cd + 4, 20);             /* version made by */
        put16(cd + 6, 20);             /* version needed */
        put16(cd + 8, 0);              /* flags */
        put16(cd + 10, method[i]);
        put16(cd + 12, 0);
        put16(cd + 14, 0);
        put32(cd + 16, crc[i]);
        put32(cd + 20, (uint32_t)comp[i]);
        put32(cd + 24, (uint32_t)uncomp[i]);
        put16(cd + 28, (uint16_t)strlen(names[i]));
        put16(cd + 30, 0);
        put16(cd + 32, 0);
        put16(cd + 34, 0);
        put16(cd + 36, 0);
        put32(cd + 38, 0);
        put32(cd + 42, local_off[i]);
        off += 46;
        memcpy(zip + off, names[i], strlen(names[i]));
        off += strlen(names[i]);
    }

    uint32_t cd_size = (uint32_t)(off - cd_off);
    uint8_t *eocd = zip + off;
    put32(eocd, 0x06054b50);
    put16(eocd + 4, 0);
    put16(eocd + 6, 0);
    put16(eocd + 8, 3);
    put16(eocd + 10, 3);
    put32(eocd + 12, cd_size);
    put32(eocd + 16, cd_off);
    put16(eocd + 20, 0);
    off += 22;

    *len_out = off;
    return zip;
}
