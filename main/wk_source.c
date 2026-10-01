/* wk_source.c - pluggable data source implementations. */
#include <string.h>
#include <stdlib.h>
#include "wk_source.h"

int wk_buffer_read_at(void *ctx, uint32_t offset, void *buf, size_t len)
{
    wk_buffer_ctx_t *c = ctx;
    if ((size_t)offset >= c->len) return 0;
    size_t avail = c->len - (size_t)offset;
    if (len > avail) len = avail;
    memcpy(buf, c->base + offset, len);
    return (int)len;
}

bool wk_source_from_buffer(wk_source_t *s, const uint8_t *base, size_t len)
{
    wk_buffer_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return false;
    ctx->base = base;
    ctx->len = len;
    s->ctx = ctx;
    s->read_at = wk_buffer_read_at;
    s->heap_ctx = true;
    return true;
}

void wk_source_deinit(wk_source_t *s)
{
    if (s->heap_ctx && s->ctx) {
        free(s->ctx);
    }
    s->ctx = NULL;
    s->read_at = NULL;
    s->heap_ctx = false;
}
