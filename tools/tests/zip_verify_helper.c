/* zip_verify_helper.c - host helper: index a zip + inflate payload.elf.
 *
 * Used by test_fat_image.py to run the firmware's own wk_zip/wk_http code
 * over a round-tripped archive. Exit 0 = index ok, payload.elf inflates to
 * its declared size with matching CRC.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wk_zip.h"
#include "wk_http.h"

static uint8_t out_buf[3 * 1024 * 1024];
static size_t out_len;

static bool sink_begin(void *ctx, const char *ctype, uint32_t body_len,
                       const char *name, bool streamed)
{
    (void)ctx; (void)ctype; (void)name; (void)streamed;
    if (body_len > sizeof(out_buf)) return false;
    out_len = 0;
    return true;
}

static bool sink_write(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    if (out_len + len > sizeof(out_buf)) return false;
    memcpy(out_buf + out_len, data, len);
    out_len += len;
    return true;
}

static void sink_end(void *ctx) { (void)ctx; }

static const wk_http_io_t io = {
    .begin = sink_begin, .write = sink_write, .end = sink_end,
};

/* CRC32 (zip poly). */
static uint32_t crc32_of(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
    }
    return ~c;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s archive.zip\n", argv[0]); return 2; }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 2; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) return 2;
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) return 2;
    fclose(f);

    wk_zip_t zip;
    if (wk_zip_init(&zip, buf, (size_t)sz) != 0) {
        fprintf(stderr, "index failed\n");
        return 1;
    }
    printf("indexed %d entries\n", zip.count);

    const wk_zip_entry_t *e = wk_zip_find(&zip, "payloads/payload.elf");
    if (!e) {
        fprintf(stderr, "payloads/payload.elf missing\n");
        return 1;
    }
    wk_http_status_t rc = wk_http_inflate_entry(&zip, e, &io, NULL);
    if (rc != WK_HTTP_OK || out_len != e->uncomp_size) {
        fprintf(stderr, "inflate rc=%d len=%zu want=%u\n", rc, out_len, e->uncomp_size);
        return 1;
    }
    uint32_t crc = crc32_of(out_buf, out_len);
    if (crc != e->crc32) {
        fprintf(stderr, "crc mismatch: %08x != %08x\n", crc, e->crc32);
        return 1;
    }
    printf("payload.elf inflated %u bytes, crc %08x OK\n", e->uncomp_size, crc);
    return 0;
}
