/* test_mem_sink.c - shared in-memory response sink implementation. */
#include <stdio.h>
#include <string.h>
#include "test_mem_sink.h"

static bool sink_begin(void *ctx, const char *ctype, uint32_t body_len,
                       const char *name, bool streamed)
{
    mem_sink_t *s = ctx;
    s->begun = true;
    snprintf(s->ctype, sizeof(s->ctype), "%s", ctype);
    s->declared = body_len;
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->streamed = streamed;
    s->len = 0;
    return true;
}

static bool sink_write(void *ctx, const uint8_t *data, size_t len)
{
    mem_sink_t *s = ctx;
    if (s->len + len > s->cap) {
        s->len = s->cap + 1; /* overflow marker */
        return true;
    }
    memcpy(s->buf + s->len, data, len);
    s->len += len;
    return true;
}

static void sink_end(void *ctx)
{
    mem_sink_t *s = ctx;
    s->ended = true;
}

const wk_http_io_t mem_io = {
    .begin = sink_begin,
    .write = sink_write,
    .end = sink_end,
};
