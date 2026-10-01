/* wk_source.h - pluggable data source for zip entry serving.
 *
 * The HTTP layer pulls entry bytes through this interface so the same code
 * serves from three backends:
 *   - firmware, embedded zip: direct flash pointer (zero-copy mmap),
 *   - firmware, FAT/MSC zip: seek + read on the open file (streaming),
 *   - host tests: malloc'd archive buffer or real files.
 *
 * Sources are read with strictly increasing offsets in practice; the
 * implementation must still honor random access (seek on gap).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wk_source wk_source_t;

/* Read `len` bytes starting at `offset` into `buf`. Returns bytes read
 * (may be less than len on short reads), 0 at EOF, <0 on error. */
typedef int (*wk_source_read_at_fn)(void *ctx, uint32_t offset,
                                    void *buf, size_t len);

struct wk_source {
    void *ctx;
    wk_source_read_at_fn read_at;
    bool heap_ctx;            /* ctx allocated by wk_source_from_buffer      */
};

static inline int wk_source_read_at(const wk_source_t *s, uint32_t offset,
                                    void *buf, size_t len)
{
    return s->read_at(s->ctx, offset, buf, len);
}

/* A source over a contiguous memory buffer (firmware: flash mmap; tests:
 * malloc'd archive). Allocates a small context; call wk_source_deinit(). */
typedef struct {
    const uint8_t *base;
    size_t len;
} wk_buffer_ctx_t;

int wk_buffer_read_at(void *ctx, uint32_t offset, void *buf, size_t len);

bool wk_source_from_buffer(wk_source_t *s, const uint8_t *base, size_t len);
void wk_source_deinit(wk_source_t *s);

#ifdef __cplusplus
}
#endif
