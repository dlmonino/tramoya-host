/* http_server.h - HTTP(S) file server backed by the embedded zip. */
#pragma once

#include <stdbool.h>
#include "esp_http_server.h"
#include "wk_zip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const wk_zip_t *zip;   /* archive to serve; NULL serves nothing  */
    httpd_handle_t http;   /* plain HTTP on 80   (or 0 when disabled) */
    httpd_handle_t https;  /* TLS on 443         (or 0 when disabled) */
} web_server_t;

/* Start both servers serving `zip` (may be NULL: all requests 404).
 * Returns 0 on success. */
int web_server_start(web_server_t *srv, const wk_zip_t *zip);

#ifdef __cplusplus
}
#endif
