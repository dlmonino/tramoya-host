/* wk_dns.h - pure DNS response builder (no platform dependencies).
 *
 * Mirrors the reference pc-host/host.py DNSHandler so the firmware answers
 * byte-for-byte like the PC host:
 *   - a query for the target domain -> A record with the configured IP,
 *   - known reachability probe hosts -> local A/ANY, NODATA for IPv6,
 *   - every other query -> NXDOMAIN.
 *
 * Queries from a real resolv.conf include EDNS0; the answer echoes the
 * query's first question verbatim (QDCOUNT 1), which satisfies every
 * resolver without parsing anything past the first question.
 *
 * Host-testable: this file compiles on the host unmodified (tools/tests).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WK_DNS_QUERY_MAX 512
#define WK_DNS_RESP_MAX  512
#define WK_DNS_TTL       300u  /* matches DEFAULT_TTL in pc-host/host.py */

typedef enum {
    WK_DNS_PARSE_BAD = -1,   /* not a usable question */
    WK_DNS_PARSE_OK = 0,
} wk_dns_parse_t;

typedef struct {
    uint16_t qid;
    char name[256];          /* lower-case, no trailing dot */
    uint16_t qtype;
    uint16_t qclass;
    /* Raw question section (name + qtype + qclass) echoed verbatim into the
     * response, exactly like host.py's `question_bytes`. */
    uint8_t question[WK_DNS_QUERY_MAX - 12];
    uint16_t question_len;
} wk_dns_query_t;

typedef enum {
    WK_DNS_ANSWER_A = 0,     /* target domain -> A record */
    WK_DNS_ANSWER_NXDOMAIN,  /* everything else */
    WK_DNS_ANSWER_NODATA,    /* known probe host, unsupported record type */
} wk_dns_answer_kind_t;

/* Parse the first question of `query` (len `qlen`). Returns WK_DNS_PARSE_BAD
 * for non-questions, truncated packets, or compressed question names (which
 * never occur in well-formed queries). */
wk_dns_parse_t wk_dns_parse_query(const uint8_t *query, size_t qlen,
                                  wk_dns_query_t *out);

/* Decide + build the response for `q` against `target` (lower-case) and
 * `answer_ip` (network byte order). Returns the response length, or 0 on
 * failure. The response echoes the query's question section and uses the
 * same qid. */
size_t wk_dns_build_response(const wk_dns_query_t *q,
                             const char *target,
                             uint32_t answer_ip,
                             uint8_t *resp, size_t resp_cap,
                             wk_dns_answer_kind_t *kind_out);

/* One-shot packet handler: parse + decide + build + stats. Returns 0 and
 * sets *resp_len when a response was produced, -1 when the packet is not
 * answerable (mirrors host.py DNSHandler: target name -> A record for ANY
 * qtype; every other name -> NXDOMAIN). */
int wk_dns_handle_packet(const char *target, uint32_t answer_ip,
                         uint32_t *a_count, uint32_t *nx_count,
                         const uint8_t *query, size_t qlen,
                         uint8_t *resp, size_t resp_cap, size_t *resp_len);

#ifdef __cplusplus
}
#endif
