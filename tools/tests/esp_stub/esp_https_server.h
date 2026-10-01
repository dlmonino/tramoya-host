/* Minimal esp_https_server.h shim for host tests. */
#ifndef ESP_HTTPS_SERVER_STUB_H
#define ESP_HTTPS_SERVER_STUB_H

#include "esp_http_server.h"

typedef struct {
    httpd_config_t httpd;
    const uint8_t *servercert;
    size_t servercert_len;
    const uint8_t *cacert_pem;
    size_t cacert_len;
    const uint8_t *prvtkey_pem;
    size_t prvtkey_len;
} httpd_ssl_config_t;

#define HTTPD_SSL_CONFIG_DEFAULT() ((httpd_ssl_config_t){0})

esp_err_t httpd_ssl_start(httpd_handle_t *handle, httpd_ssl_config_t *config);

#endif
