/* Instantánea del frontend: el servidor usa RAM mientras MSC posee FAT. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wk_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WK_FRONTEND_OK = 0,
    WK_FRONTEND_NO_FILE,
    WK_FRONTEND_TOO_LARGE,
    WK_FRONTEND_NO_MEMORY,
    WK_FRONTEND_IO_ERROR,
    WK_FRONTEND_INVALID_ZIP,
} wk_frontend_status_t;

typedef void *(*wk_frontend_alloc_t)(size_t size);
typedef void (*wk_frontend_free_t)(void *ptr);

typedef struct {
    uint8_t *archive;
    size_t archive_size;
    wk_frontend_free_t release;
    wk_zip_t zip;
    bool ready;
} wk_frontend_t;

/* Leer y cerrar el ZIP íntegro antes de ceder FAT a USB. max_bytes reserva
 * memoria para Wi-Fi/HTTPS. En caso de fallo no queda ningún archivo abierto. */
wk_frontend_status_t wk_frontend_load(wk_frontend_t *frontend,
                                      const wk_fs_ops_t *fs, const char *path,
                                      size_t max_bytes,
                                      wk_frontend_alloc_t alloc,
                                      wk_frontend_free_t release);
void wk_frontend_deinit(wk_frontend_t *frontend);

#ifdef __cplusplus
}
#endif
