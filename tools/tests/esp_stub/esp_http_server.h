/* Minimal esp_http_server.h shim for host tests.
 *
 * Only the surface used by main/http_server.c; the implementation lives in
 * esp_http_server_stub.c and captures responses into g_mock_resp.
 */
#ifndef ESP_HTTP_SERVER_STUB_H
#define ESP_HTTP_SERVER_STUB_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

static inline const char *esp_err_to_name(esp_err_t err)
{
    return err == ESP_OK ? "ESP_OK" : err == ESP_FAIL ? "ESP_FAIL" : "UNKNOWN ERROR";
}

typedef struct httpd_data *httpd_handle_t;

typedef struct httpd_req {
    const char *uri;
    void *user_ctx;
    size_t content_len;
} httpd_req_t;  /* matches ESP-IDF 5.4: struct type, handlers take httpd_req_t * */

typedef enum {
    HTTP_GET = 0,
    HTTP_HEAD = 1,
    HTTP_POST = 2,
} httpd_method_t;

typedef struct {
    const char *uri;
    httpd_method_t method;
    esp_err_t (*handler)(httpd_req_t *r);
    void *user_ctx;
} httpd_uri_t;

typedef struct httpd_config {
    int server_port;
    int max_uri_handlers;
    int stack_size;
    int core_id;
    int task_caps;
    int max_open_sockets;
    void *uri_match_fn;
    void *global_user_ctx;
} httpd_config_t;

#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){ .max_uri_handlers = 8 })
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_400_BAD_REQUEST 400
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define MALLOC_CAP_INTERNAL 0
#define MALLOC_CAP_8BIT 0

#define httpd_uri_match_wildcard ((void *)0)

/* Response API (captured by the stub); IDF passes httpd_req_t *. */
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type);
void httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value);
esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len);
esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t len);
int httpd_send(httpd_req_t *r, const char *buf, size_t len);
int httpd_req_recv(httpd_req_t *r, char *buf, size_t len);
void httpd_resp_send_err(httpd_req_t *r, int status, const char *msg);
void httpd_resp_send_500(httpd_req_t *r);

/* Server lifecycle (stubbed; unused in tests). */
esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri);

#endif
