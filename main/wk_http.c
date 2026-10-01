/* wk_http.c - HTTP serving core (platform-neutral).
 *
 * Includes the tinfl streaming loop in its final, host-verified form:
 * miniz's *pIn_buf_size returns CONSUMED and *pOut_buf_size returns WRITTEN
 * on return (miniz common_exit: cur - next); TINFL_FLAG_HAS_MORE_INPUT must
 * be set whenever a refill of the input buffer is possible.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "wk_http.h"

/* Declarations only: the implementation is compiled once as miniz.c in
 * both the firmware and the host test build. */
#define MINIZ_HEADER_FILE_ONLY
#include "miniz.h"

#define OUT_RING 32768  /* power of 2, >= the deflate window */

const char *wk_http_content_type(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return "application/octet-stream";
    if (!strcmp(dot, ".html") || !strcmp(dot, ".htm")) return "text/html; charset=utf-8";
    if (!strcmp(dot, ".js"))   return "application/javascript; charset=utf-8";
    if (!strcmp(dot, ".css"))  return "text/css; charset=utf-8";
    if (!strcmp(dot, ".json")) return "application/json";
    if (!strcmp(dot, ".svg"))  return "image/svg+xml";
    if (!strcmp(dot, ".png"))  return "image/png";
    if (!strcmp(dot, ".jpg") || !strcmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcmp(dot, ".txt"))  return "text/plain; charset=utf-8";
    if (!strcmp(dot, ".woff")) return "font/woff";
    if (!strcmp(dot, ".woff2")) return "font/woff2";
    if (!strcmp(dot, ".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

wk_http_status_t wk_http_inflate_entry(const wk_zip_t *zip,
                                       const wk_zip_entry_t *e,
                                       const wk_http_io_t *sink,
                                       void *sink_ctx)
{
    if (!zip->initialized || !e) return WK_HTTP_INTERNAL;

    tinfl_decompressor *infl = malloc(sizeof(*infl));

    uint8_t *in = malloc(WK_HTTP_CHUNK);
    uint8_t *ring = malloc(OUT_RING);
    if (!infl || !in || !ring) {
        free(infl);
        free(in);
        free(ring);
        return WK_HTTP_INTERNAL;
    }
    tinfl_init(infl);

    /* Compressed bytes come through the zip's source (flash mmap, FAT file
     * seek, or a test buffer — the same loop serves all three). */
    uint32_t comp_read = 0;
    size_t comp_remain = e->comp_size;
    size_t total_out = 0;
    size_t avail_in = 0;
    const uint8_t *next_in = in;
    tinfl_status status = TINFL_STATUS_NEEDS_MORE_INPUT;

    for (;;) {
        if (avail_in == 0) {
            if (comp_remain == 0) break; /* corrupt: stream wants more input */
            size_t n = comp_remain < WK_HTTP_CHUNK ? comp_remain : WK_HTTP_CHUNK;
            int got = wk_zip_read_entry(zip, e, comp_read, in, n);
            if (got <= 0) break;
            comp_read += (uint32_t)got;
            comp_remain -= (size_t)got;
            next_in = in;
            avail_in = (size_t)got;
        }
        /* TINFL_FLAG_HAS_MORE_INPUT means "more input exists beyond this
         * buffer" — set whenever a refill is possible, including when this
         * call does not consume all of `in` (miniz keeps the tail for the
         * next call). */
        uint32_t fl = comp_remain > 0
                          ? TINFL_FLAG_HAS_MORE_INPUT : 0;

        uint8_t *next_out = ring + (total_out & (OUT_RING - 1));
        size_t window_capacity = OUT_RING - (total_out & (OUT_RING - 1));
        size_t avail_out = window_capacity;
        size_t in_before = avail_in;

        status = tinfl_decompress(infl, next_in, &avail_in,
                                  ring, next_out, &avail_out, fl);

        /* miniz returns CONSUMED in *pIn_buf_size (cur - next) and WRITTEN in
         * *pOut_buf_size. Track the remaining input ourselves. */
        size_t consumed = avail_in;
        size_t remaining = in_before - consumed;
        next_in += consumed;
        avail_in = remaining;

        size_t produced = avail_out;
        total_out += produced;
        if (produced && sink->write && !sink->write(sink_ctx, next_out, produced)) {
            free(infl);
            free(in);
            free(ring);
            return WK_HTTP_ABORTED;
        }
        if (status <= TINFL_STATUS_DONE) break;
    }

    free(infl);
    free(in);
    free(ring);

    if (status != TINFL_STATUS_DONE || total_out != e->uncomp_size) {
        return WK_HTTP_INTERNAL;
    }
    return WK_HTTP_OK;
}

wk_http_status_t wk_http_serve_get(const wk_zip_t *zip,
                                   const char *uri,
                                   const char *selected_exploit,
                                   const wk_http_io_t *sink,
                                   void *sink_ctx)
{
    if (!zip->initialized || !sink || !sink->write) return WK_HTTP_INTERNAL;

    /* Virtual file: which exploit app.js should pick on dual firmwares.
     * Matches host.py's _resolve_source: any path whose trailing segment is
     * exactly "selected_exploit" (query strings are never part of the mapped
     * path). */
    const char *q = strchr(uri, '?');
    size_t path_len = q ? (size_t)(q - uri) : strlen(uri);
    if (path_len >= strlen("selected_exploit")) {
        const char *tail = uri + path_len - strlen("selected_exploit");
        if (memcmp(tail, "selected_exploit", strlen("selected_exploit")) == 0) {
            const char *body = selected_exploit ? selected_exploit : "relapse\n";
            size_t len = strlen(body);
            if (sink->begin && !sink->begin(sink_ctx, "text/plain",
                                            (uint32_t)len, "selected_exploit", false))
                return WK_HTTP_ABORTED;
            if (!sink->write(sink_ctx, (const uint8_t *)body, len)) return WK_HTTP_ABORTED;
            if (sink->end) sink->end(sink_ctx);
            return WK_HTTP_OK;
        }
    }

    char rel[256];
    if (!wk_zip_map_path(uri, rel, sizeof(rel))) return WK_HTTP_BAD_URI;

    const wk_zip_entry_t *e = wk_zip_find(zip, rel);
    if (!e && (rel[0] == '\0' || rel[strlen(rel) - 1] == '/')) {
        char cand[256 + 10];
        if (snprintf(cand, sizeof(cand), "%sindex.html", rel) < (int)sizeof(cand))
            e = wk_zip_find(zip, cand);
        if (!e) {
            if (snprintf(cand, sizeof(cand), "%sindex.htm", rel) < (int)sizeof(cand))
                e = wk_zip_find(zip, cand);
        }
    }
    if (!e) return WK_HTTP_NOT_FOUND;

    bool streamed = !(e->method == 0 && e->comp_size == e->uncomp_size);
    const char *ctype = wk_http_content_type(e->name);
    if (sink->begin && !sink->begin(sink_ctx, ctype, e->uncomp_size, e->name, streamed))
        return WK_HTTP_ABORTED;

    wk_http_status_t rc;
    if (!streamed) {
        /* STORE entry: stream it through the source in chunks too (a FAT
         * file has no contiguous pointer). */
        uint32_t off = 0;
        bool ok = true;
        uint8_t *chunk = malloc(WK_HTTP_CHUNK);
        if (!chunk) return WK_HTTP_INTERNAL;
        while (off < e->uncomp_size) {
            size_t n = e->uncomp_size - off;
            if (n > WK_HTTP_CHUNK) n = WK_HTTP_CHUNK;
            int got = wk_zip_read_entry(zip, e, off, chunk, n);
            if (got <= 0 || !sink->write(sink_ctx, chunk, (size_t)got)) {
                ok = false;
                break;
            }
            off += (uint32_t)got;
        }
        free(chunk);
        rc = ok ? WK_HTTP_OK : WK_HTTP_ABORTED;
    } else {
        rc = wk_http_inflate_entry(zip, e, sink, sink_ctx);
    }
    if (rc == WK_HTTP_OK && sink->end) sink->end(sink_ctx);
    return rc;
}
