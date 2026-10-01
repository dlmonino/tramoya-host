/* wk_zip.h - zip central-directory scan + path mapping (platform-neutral).
 *
 * Pure logic, no ESP dependencies; compiled into the firmware and the host
 * unit tests from the same sources. Archive bytes are supplied by the
 * caller (flash-mapped buffer on the device, a malloc'd buffer on the host).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wk_source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Central-directory records only (RAM). Entry data is NOT retained;
     * serving reads it through `src` on demand. */
    char **names;            /* lower-case entry names (malloc'd) */
    void *entries;           /* opaque per-entry records */
    int count;

    /* Data source for entry bytes (archive image). */
    wk_source_t src;
    bool src_owned;          /* wk_source_deinit on deinit                  */
    bool initialized;
} wk_zip_t;

/* Entry descriptor returned by wk_zip_find / wk_zip_at. */
typedef struct {
    const char *name;        /* archive path, lower-case */
    uint32_t local_off;      /* local file header offset inside the archive */
    uint32_t data_off;       /* entry data offset inside the archive */
    uint32_t comp_size;      /* stored size */
    uint32_t uncomp_size;    /* size after inflate */
    uint16_t method;         /* 0 = STORE, 8 = DEFLATE */
    uint32_t crc32;
} wk_zip_entry_t;

/* Scan the central directory from an archive image. When `owned_buffer` is
 * true the archive pointer stays owned by the caller; entry DATA is always
 * read through the source (set up for the same image). */
int wk_zip_init(wk_zip_t *zip, const uint8_t *archive, size_t len);
/* Scan from a source (FAT files): only the central directory is pulled
 * into RAM; `src` must cover the same archive for the index lifetime. */
int wk_zip_init_from_source(wk_zip_t *zip, wk_source_t *src, size_t len);
void wk_zip_deinit(wk_zip_t *zip);

/* Find by lower-case archive path (no leading '/', no query string). */
const wk_zip_entry_t *wk_zip_find(const wk_zip_t *zip, const char *path);
/* Nth entry (index order). */
const wk_zip_entry_t *wk_zip_at(const wk_zip_t *zip, int i);

/* Read `len` bytes of entry data at `entry_offset` through the zip's
 * source. Returns bytes read, 0 EOF, <0 error. */
int wk_zip_read_entry(const wk_zip_t *zip, const wk_zip_entry_t *e,
                      uint32_t entry_offset, void *buf, size_t len);

/* Map a request URI onto a normalized relative path, mirroring
 * pc-host/host.py DualDirHandler._relative_path():
 *   - query and fragment are stripped,
 *   - percent-decoding with surrogatepass semantics (invalid escapes are
 *     passed through),
 *   - /document/<lang>/ps5/<rest> -> <rest>,
 *   - /app/<rest> -> <rest>,
 *   - the path is split on '/', '.' and '..' words are folded with
 *     posixpath.normpath semantics, other words are kept.
 * Returns false when the URI is unusable or out of capacity. */
bool wk_zip_map_path(const char *uri, char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
