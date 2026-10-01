/* Minimal esp_log.h shim for host tests. */
#ifndef ESP_LOG_STUB_H
#define ESP_LOG_STUB_H

#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "[I] " fmt "\n", ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)

#endif
