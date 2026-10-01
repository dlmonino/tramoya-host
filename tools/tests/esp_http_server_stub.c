/* esp_http_server_stub.c - response-capturing mock for host tests.
 *
 * httpd_resp_* write into g_mock_resp so tests can assert on status,
 * headers and body bytes. httpd_start / httpd_ssl_start / register are
 * no-ops (web_server_start is never called from tests).
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_https_server.h"

typedef struct {
    int status;
    int ctype_set;
    char ctype[64];
    int clen_set;
    uint32_t clen;
    uint8_t body[6 * 1024 * 1024];
    size_t body_len;
    int send_err_called;
} mock_resp_t;

mock_resp_t g_mock_resp;
int g_mock_send_chunk_ok = 1;
char g_mock_wire_headers[512];
int g_mock_chunk_ended;
size_t g_mock_send_limit;
size_t g_mock_recv_remaining;
int g_mock_recv_fail;

/* Host stand-ins for the firmware's EMBED_TXTFILES certificates. The
 * firmware embeds them under those binary symbol names; web_server_start
 * only copies the pointers into the esp_https_server config, which the
 * stub above discards, so dummy content is enough to link (including
 * sanitizer builds, whose instrumentation defeats --gc-sections). */
__attribute__((used)) const uint8_t wk_stub_cert[] asm("_binary_servercert_pem_start") =
    "-----BEGIN CERTIFICATE-----\n";
__attribute__((used)) const uint8_t wk_stub_cert_end[] asm("_binary_servercert_pem_end") = "";
__attribute__((used)) const uint8_t wk_stub_key[] asm("_binary_prvtkey_pem_start") =
    "-----BEGIN PRIVATE KEY-----\n";
__attribute__((used)) const uint8_t wk_stub_key_end[] asm("_binary_prvtkey_pem_end") = "";

esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type)
{
    g_mock_resp.ctype_set = 1;
    snprintf(g_mock_resp.ctype, sizeof(g_mock_resp.ctype), "%s", type);
    (void)r;
    return ESP_OK;
}

void httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value)
{
    if (strcmp(field, "Content-Length") == 0) {
        g_mock_resp.clen_set = 1;
        g_mock_resp.clen = (uint32_t)strtoul(value, NULL, 10);
    }
    (void)r;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len)
{
    (void)r;
    if (len < 0) return ESP_FAIL;
    if (!g_mock_send_chunk_ok) return ESP_FAIL;
    if (!g_mock_wire_headers[0]) {
        snprintf(g_mock_wire_headers, sizeof(g_mock_wire_headers),
                 "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nTransfer-Encoding: chunked\r\n%s\r\n",
                 g_mock_resp.ctype, g_mock_resp.clen_set ? "Content-Length: unexpected\r\n" : "");
    }
    if (len == 0) g_mock_chunk_ended = 1;
    if (g_mock_resp.body_len + (size_t)len > sizeof(g_mock_resp.body)) return ESP_FAIL;
    if (len > 0) {
        memcpy(g_mock_resp.body + g_mock_resp.body_len, buf, (size_t)len);
        g_mock_resp.body_len += (size_t)len;
    }
    return ESP_OK;
}

esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t len)
{
    if (buf && len > 0) return httpd_resp_send_chunk(r, buf, len);
    return ESP_OK;
}

int httpd_send(httpd_req_t *r, const char *buf, size_t len)
{
    (void)r;
    if (!g_mock_send_chunk_ok) return -1;
    if (g_mock_send_limit && len > g_mock_send_limit) len = g_mock_send_limit;
    if (!strstr(g_mock_wire_headers, "\r\n\r\n")) {
        size_t used = strlen(g_mock_wire_headers);
        if (used + len >= sizeof(g_mock_wire_headers)) return -1;
        memcpy(g_mock_wire_headers + used, buf, len);
        g_mock_wire_headers[used + len] = '\0';
        if (strstr(g_mock_wire_headers, "\r\n\r\n")) {
            char *clen = strstr(g_mock_wire_headers, "Content-Length: ");
            if (clen) {
                g_mock_resp.clen_set = 1;
                g_mock_resp.clen = (uint32_t)strtoul(clen + 16, NULL, 10);
            }
        }
    } else {
        if (g_mock_resp.body_len + len > sizeof(g_mock_resp.body)) return -1;
        memcpy(g_mock_resp.body + g_mock_resp.body_len, buf, len);
        g_mock_resp.body_len += len;
    }
    return (int)len;
}

int httpd_req_recv(httpd_req_t *r, char *buf, size_t len)
{
    (void)r;
    if (g_mock_recv_fail) return -1;
    if (len > g_mock_recv_remaining) len = g_mock_recv_remaining;
    /* Exercise partial receives as well as the complete drain. */
    if (len > 37) len = 37;
    memset(buf, 'x', len);
    g_mock_recv_remaining -= len;
    return (int)len;
}

void httpd_resp_send_err(httpd_req_t *r, int status, const char *msg)
{
    (void)r;
    (void)msg;
    g_mock_resp.send_err_called = 1;
    g_mock_resp.status = status;
}

void httpd_resp_send_500(httpd_req_t *r)
{
    (void)r;
    g_mock_resp.status = 500;
}

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config)
{
    (void)handle;
    (void)config;
    return ESP_FAIL;
}

esp_err_t httpd_ssl_start(httpd_handle_t *handle, httpd_ssl_config_t *config)
{
    (void)handle;
    (void)config;
    return ESP_FAIL;
}

esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri)
{
    (void)handle;
    (void)uri;
    return ESP_OK;
}
