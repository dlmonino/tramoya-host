/* dns_server.h - tiny captive-portal DNS: A for the target, NXDOMAIN else. */
#pragma once

#include <stdint.h>
#include "wk_dns.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int sock;                 /* UDP socket, -1 when not running */
    char target[128];         /* spoofed domain, lower-case (manuals.playstation.net) */
    uint32_t answer_ip;       /* IPv4 addr (network byte order) handed out for the target */
    uint32_t a_queries;       /* stat counters */
    uint32_t nxdomain_queries;
} dns_server_t;

/* Weak override points for the host tests: the packet-handling logic can be
 * driven with canned queries without a socket. */
int dns_server_handle_packet(dns_server_t *dns, const uint8_t *query,
                             size_t qlen, uint8_t *resp, size_t resp_cap,
                             size_t *resp_len);

int dns_server_start(dns_server_t *dns, uint16_t port, uint32_t answer_ip,
                     const char *target);
void dns_server_stop(dns_server_t *dns);

#ifdef __cplusplus
}
#endif
