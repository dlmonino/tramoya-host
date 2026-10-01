/* wk_http.h - HTTP serving core (platform-neutral).
 *
 * The decision and streaming logic of the web server, mirroring the
 * reference pc-host/host.py DualDirHandler:
 *   - virtual file `selected_exploit`,
 *   - path mapping via wk_zip_map_path,
 *   - index.html/index.htm fallback for directory-ish paths,
 *   - DEFLATE entries streamed through tinfl in fixed-size chunks.
 *
 * Platform-specific I/O (httpd_resp_send_chunk on the firmware, memcpy on
 * the host) is injected through wk_http_io. The firmware's http_server.c
 * adapts it to esp_http_server; the host unit tests adapt it to a buffer.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wk_zip.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WK_HTTP_CHUNK 16384

/* Output sink the server streams into. */
typedef struct {
    /* Start a response: headers describe the resolved entry. Return false to
     * abort (and to avoid any body calls). */
    bool (*begin)(void *ctx, const char *content_type, uint32_t body_len,
                  const char *name, bool streamed);
    /* Append `len` bytes of the body. */
    bool (*write)(void *ctx, const uint8_t *data, size_t len);
    /* Finish the response (called on success; not called after abort). */
    void (*end)(void *ctx);
} wk_http_io_t;

typedef enum {
    WK_HTTP_OK = 0,
    WK_HTTP_NOT_FOUND,
    WK_HTTP_INTERNAL,       /* inflate failure */
    WK_HTTP_ABORTED,        /* I/O sink aborted the transfer */
    WK_HTTP_BAD_URI,
} wk_http_status_t;

/* Serve one GET for `uri` (path + optional query; no fragment).
 * `sink` carries the bytes out. */
wk_http_status_t wk_http_serve_get(const wk_zip_t *zip,
                                   const char *uri,
                                   const char *selected_exploit,
                                   const wk_http_io_t *sink,
                                   void *sink_ctx);

/* Content type for a path (mirrors SimpleHTTPRequestHandler/mimetypes). */
const char *wk_http_content_type(const char *path);

/* Inflate one DEFLATE entry via tinfl, pushing output through `sink` in
 * WK_HTTP_CHUNK-sized pieces. Returns WK_HTTP_OK when the entry inflates to
 * exactly its declared size, WK_HTTP_INTERNAL on stream errors, or
 * WK_HTTP_ABORTED when the sink aborts. */
wk_http_status_t wk_http_inflate_entry(const wk_zip_t *zip,
                                       const wk_zip_entry_t *e,
                                       const wk_http_io_t *sink,
                                       void *sink_ctx);

#ifdef __cplusplus
}
#endif
