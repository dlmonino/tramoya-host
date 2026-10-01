/* test_mem_sink.h - shared in-memory response sink for wk_http tests. */
#ifndef TEST_MEM_SINK_H
#define TEST_MEM_SINK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wk_http.h"

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t len;
    char ctype[64];
    uint32_t declared;
    char name[128];
    bool streamed;
    bool begun;
    bool ended;
} mem_sink_t;

extern const wk_http_io_t mem_io;

#endif
