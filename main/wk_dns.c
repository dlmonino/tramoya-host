/* wk_dns.c - pure DNS response builder (no platform dependencies).
 *
 * Byte-level behavior mirrors pc-host/host.py's build_response() and
 * parse_query(): the question section of the query is echoed back, answers
 * carry QR|RD|RA, TTL is 300, NXDOMAIN answers use rcode 3 with zero answer
 * records.
 */
#include <string.h>
#include <ctype.h>
#include "wk_dns.h"
#include "wk_connectivity.h"

#define FLAG_OK      0x8180u  /* QR|RD|RA + rcode 0 */
#define FLAG_NXDOMAIN 0x8183u /* QR|RD|RA + rcode 3 */

static uint16_t rd16be(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void wr16be(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

wk_dns_parse_t wk_dns_parse_query(const uint8_t *query, size_t qlen,
                                  wk_dns_query_t *out)
{
    if (qlen < 12) return WK_DNS_PARSE_BAD;

    uint16_t qdcount = rd16be(query + 4);
    /* host.py checks `qdcount < 1` and ignores the flags entirely (it
     * answers even packets with QR set); match that window. */
    if (qdcount < 1) return WK_DNS_PARSE_BAD;

    /* Walk the QNAME labels (no compression in questions). Labels are
     * joined with '.' between them, exactly like host.py's ".".join();
     * the root byte terminates without adding a trailing dot. */
    size_t off = 12;
    size_t nl = 0;
    int have_label = 0;
    while (off < qlen) {
        uint8_t len = query[off];
        if (len == 0) { off++; break; }
        if (len & 0xC0) return WK_DNS_PARSE_BAD; /* compressed name */
        off++;
        if (off + len > qlen) return WK_DNS_PARSE_BAD;
        if (have_label && nl < sizeof(out->name) - 2) out->name[nl++] = '.';
        for (uint8_t i = 0; i < len; i++) {
            if (nl >= sizeof(out->name) - 2) return WK_DNS_PARSE_BAD;
            out->name[nl++] = (char)tolower(query[off + i]);
        }
        have_label = 1;
        off += len;
    }
    out->name[nl] = '\0';

    if (off + 4 > qlen) return WK_DNS_PARSE_BAD;
    out->qid = rd16be(query);
    out->qtype = rd16be(query + off);
    out->qclass = rd16be(query + off + 2);

    /* Echo the question verbatim (name + type + class). */
    out->question_len = (uint16_t)(off + 4 - 12);
    if (out->question_len > sizeof(out->question)) return WK_DNS_PARSE_BAD;
    memcpy(out->question, query + 12, out->question_len);
    return WK_DNS_PARSE_OK;
}

size_t wk_dns_build_response(const wk_dns_query_t *q,
                             const char *target,
                             uint32_t answer_ip,
                             uint8_t *resp, size_t resp_cap,
                             wk_dns_answer_kind_t *kind_out)
{
    if (!resp || resp_cap < 12) return 0;
    if (!q) return 0;

    bool is_target = strcmp(q->name, target) == 0;
    /* host.py answers the target's ANY query with an A record (it never
     * inspects qtype); match that so the bytes line up. */
    bool is_probe = wk_connectivity_host(q->name) && q->qclass == 1;
    bool answered_a = is_target || (is_probe && (q->qtype == 1 || q->qtype == 255));

    uint16_t flags = FLAG_NXDOMAIN;
    size_t total = 12 + q->question_len;
    uint16_t ancount = 0;
    uint8_t answer[16] = {0};
    size_t answer_len = 0;

    if (answered_a) {
        flags = FLAG_OK;
        ancount = 1;
        answer[0] = 0xC0; answer[1] = 0x0C;  /* name ptr -> offset 12 */
        wr16be(answer + 2, 1);               /* TYPE A */
        wr16be(answer + 4, 1);               /* CLASS IN */
        answer[6] = (uint8_t)(WK_DNS_TTL >> 24);
        answer[7] = (uint8_t)(WK_DNS_TTL >> 16);
        answer[8] = (uint8_t)(WK_DNS_TTL >> 8);
        answer[9] = (uint8_t)WK_DNS_TTL;
        wr16be(answer + 10, 4);              /* RDLENGTH */
        memcpy(answer + 12, &answer_ip, 4);
        answer_len = 16;
        if (kind_out) *kind_out = WK_DNS_ANSWER_A;
    } else if (is_probe) {
        /* No IPv6 on this AP: NOERROR/NODATA allows clients to use IPv4. */
        flags = FLAG_OK;
        if (kind_out) *kind_out = WK_DNS_ANSWER_NODATA;
    } else {
        if (kind_out) *kind_out = WK_DNS_ANSWER_NXDOMAIN;
    }

    total += answer_len;
    if (total > resp_cap) return 0;

    resp[0] = (uint8_t)(q->qid >> 8);
    resp[1] = (uint8_t)q->qid;
    resp[2] = (uint8_t)(flags >> 8);
    resp[3] = (uint8_t)flags;
    wr16be(resp + 4, 1);     /* QDCOUNT: echo one question */
    wr16be(resp + 6, ancount);
    wr16be(resp + 8, 0);     /* NSCOUNT */
    wr16be(resp + 10, 0);    /* ARCOUNT */
    memcpy(resp + 12, q->question, q->question_len);
    memcpy(resp + 12 + q->question_len, answer, answer_len);
    return total;
}

int wk_dns_handle_packet(const char *target, uint32_t answer_ip,
                         uint32_t *a_count, uint32_t *nx_count,
                         const uint8_t *query, size_t qlen,
                         uint8_t *resp, size_t resp_cap, size_t *resp_len)
{
    wk_dns_query_t q;
    if (wk_dns_parse_query(query, qlen, &q) != WK_DNS_PARSE_OK) {
        return -1;
    }
    wk_dns_answer_kind_t kind;
    size_t n = wk_dns_build_response(&q, target, answer_ip,
                                     resp, resp_cap, &kind);
    if (n == 0) return -1;
    if (kind == WK_DNS_ANSWER_A) {
        if (a_count) (*a_count)++;
    } else if (kind == WK_DNS_ANSWER_NXDOMAIN && nx_count) {
        (*nx_count)++;
    }
    if (resp_len) *resp_len = n;
    return 0;
}
