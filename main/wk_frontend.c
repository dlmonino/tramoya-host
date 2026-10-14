/* Instantánea inmutable del ZIP antes del traspaso exclusivo de FAT a MSC. */
#include <stdint.h>
#include <string.h>
#include "wk_frontend.h"

wk_frontend_status_t wk_frontend_load(wk_frontend_t *frontend,
                                      const wk_fs_ops_t *fs, const char *path,
                                      size_t max_bytes,
                                      wk_frontend_alloc_t alloc,
                                      wk_frontend_free_t release)
{
    if (!frontend || !fs || !fs->open || !fs->size || !fs->read || !fs->close ||
        !path || !alloc || !release) return WK_FRONTEND_IO_ERROR;
    memset(frontend, 0, sizeof(*frontend));
    void *file = fs->open(path);
    if (!file) return WK_FRONTEND_NO_FILE;

    wk_frontend_status_t status = WK_FRONTEND_IO_ERROR;
    uint8_t *bytes = NULL;
    long file_size = fs->size(file);
    if (file_size < 0) goto done;
    if (file_size <= 22) {
        status = WK_FRONTEND_INVALID_ZIP;
        goto done;
    }
    if ((uint64_t)file_size > max_bytes || (uint64_t)file_size > UINT32_MAX) {
        status = WK_FRONTEND_TOO_LARGE;
        goto done;
    }
    bytes = alloc((size_t)file_size);
    if (!bytes) {
        status = WK_FRONTEND_NO_MEMORY;
        goto done;
    }
    size_t offset = 0;
    while (offset < (size_t)file_size) {
        size_t chunk = (size_t)file_size - offset;
        if (chunk > 4096) chunk = 4096;
        int n = fs->read(file, bytes + offset, chunk);
        if (n <= 0 || (size_t)n > chunk) goto done;
        offset += (size_t)n;
    }
    if (fs->close(file) != 0) {
        file = NULL;
        goto done;
    }
    file = NULL;
    if (wk_zip_init(&frontend->zip, bytes, (size_t)file_size) != 0) {
        status = WK_FRONTEND_INVALID_ZIP;
        goto done;
    }
    frontend->archive = bytes;
    frontend->archive_size = (size_t)file_size;
    frontend->release = release;
    frontend->ready = true;
    return WK_FRONTEND_OK;

done:
    if (file && fs->close(file) != 0) status = WK_FRONTEND_IO_ERROR;
    if (bytes) release(bytes);
    memset(frontend, 0, sizeof(*frontend));
    return status;
}

void wk_frontend_deinit(wk_frontend_t *frontend)
{
    if (!frontend) return;
    if (frontend->ready) wk_zip_deinit(&frontend->zip);
    if (frontend->archive && frontend->release) frontend->release(frontend->archive);
    memset(frontend, 0, sizeof(*frontend));
}
