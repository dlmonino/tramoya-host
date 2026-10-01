#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Local responses for network reachability checks; no upstream connection. */
bool wk_connectivity_host(const char *name);
bool wk_connectivity_probe(const char *uri, bool upload,
                           const char **body, uint32_t *length);
