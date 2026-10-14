/* Constructor DNS local sin recursión ni dependencias de plataforma.
 * Contrato de tipos, clases y flags documentado en docs/dns.md.
 */
#include <string.h>
#include "wk_dns.h"
#include "wk_connectivity.h"

#define FLAG_QR     0x8000u
#define FLAG_OPCODE 0x7800u
#define FLAG_TC     0x0200u
#define FLAG_RD     0x0100u

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
    if (!query || !out || qlen < 12 || qlen > WK_DNS_QUERY_MAX)
        return WK_DNS_PARSE_BAD;
    uint16_t flags = rd16be(query + 2);
    /* No responder a respuestas, mensajes truncados o varias preguntas. */
    if ((flags & (FLAG_QR | FLAG_TC)) || rd16be(query + 4) != 1)
        return WK_DNS_PARSE_BAD;

    size_t off = 12, nl = 0;
    bool terminated = false;
    while (off < qlen) {
        uint8_t len = query[off++];
        if (len == 0) { terminated = true; break; }
        /* Este servidor limitado no interpreta preguntas comprimidas. */
        if (len > 63 || off + len > qlen) return WK_DNS_PARSE_BAD;
        if (nl) out->name[nl++] = '.';
        if (nl + len >= sizeof(out->name)) return WK_DNS_PARSE_BAD;
        for (uint8_t i = 0; i < len; i++) {
            uint8_t ch = query[off + i];
            /* Evitar que una etiqueta binaria suplante un nombre permitido
             * al convertirse en una cadena con puntos y terminador NUL. */
            if (ch == 0 || ch == '.') return WK_DNS_PARSE_BAD;
            if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            out->name[nl++] = (char)ch;
        }
        off += len;
    }
    if (!terminated || off - 12 > 255 || off + 4 > qlen)
        return WK_DNS_PARSE_BAD;
    out->name[nl] = '\0';
    out->qid = rd16be(query);
    out->flags = flags;
    out->qtype = rd16be(query + off);
    out->qclass = rd16be(query + off + 2);
    out->question_len = (uint16_t)(off + 4 - 12);
    if (out->question_len > sizeof(out->question)) return WK_DNS_PARSE_BAD;
    /* Se conserva la pregunta original, incluidas mayúsculas, tipo y clase.
     * Los adicionales (p. ej. OPT) no se copian ni activan extensiones EDNS. */
    memcpy(out->question, query + 12, out->question_len);
    return WK_DNS_PARSE_OK;
}

size_t wk_dns_build_response(const wk_dns_query_t *q,
                             const char *target,
                             uint32_t answer_ip,
                             uint8_t *resp, size_t resp_cap,
                             wk_dns_answer_kind_t *kind_out)
{
    if (!q || !target || !resp || resp_cap < 12 ||
        q->question_len > sizeof(q->question)) return 0;
    bool local = strcmp(q->name, target) == 0 || wk_connectivity_host(q->name);
    wk_dns_answer_kind_t kind;
    uint16_t rcode;
    if (q->flags & FLAG_OPCODE) {
        kind = WK_DNS_ANSWER_NOTIMP; rcode = 4;
    } else if (q->qclass != 1) {
        kind = WK_DNS_ANSWER_REFUSED; rcode = 5;
    } else if (q->qtype == 0 || q->qtype == 41 ||
               q->qtype == 251 || q->qtype == 252) {
        /* TYPE0, OPT como pregunta y transferencias IXFR/AXFR no se admiten. */
        kind = WK_DNS_ANSWER_NOTIMP; rcode = 4;
    } else if (!local) {
        kind = WK_DNS_ANSWER_NXDOMAIN; rcode = 3;
    } else if (q->qtype == 1 || q->qtype == 255) {
        kind = WK_DNS_ANSWER_A; rcode = 0;
    } else {
        /* El nombre local existe, pero no hay datos de ese tipo (incluido AAAA). */
        kind = WK_DNS_ANSWER_NODATA; rcode = 0;
    }
    bool answered_a = kind == WK_DNS_ANSWER_A;
    size_t total = 12 + q->question_len + (answered_a ? 16 : 0);
    if (total > resp_cap) return 0;

    /* RD y OPCODE se reflejan; RA, AA, TC, AD, CD y reservados quedan a cero.
     * Esta política local no ofrece recursión ni autoridad pública/DNSSEC. */
    uint16_t flags = FLAG_QR | (q->flags & (FLAG_OPCODE | FLAG_RD)) | rcode;
    wr16be(resp, q->qid);
    wr16be(resp + 2, flags);
    wr16be(resp + 4, 1);
    wr16be(resp + 6, answered_a ? 1 : 0);
    wr16be(resp + 8, 0);
    wr16be(resp + 10, 0);
    memcpy(resp + 12, q->question, q->question_len);
    if (answered_a) {
        uint8_t *answer = resp + 12 + q->question_len;
        answer[0] = 0xc0; answer[1] = 0x0c;
        wr16be(answer + 2, 1);
        wr16be(answer + 4, 1);
        answer[6] = (uint8_t)(WK_DNS_TTL >> 24);
        answer[7] = (uint8_t)(WK_DNS_TTL >> 16);
        answer[8] = (uint8_t)(WK_DNS_TTL >> 8);
        answer[9] = (uint8_t)WK_DNS_TTL;
        wr16be(answer + 10, 4);
        memcpy(answer + 12, &answer_ip, 4);
    }
    if (kind_out) *kind_out = kind;
    return total;
}

int wk_dns_handle_packet(const char *target, uint32_t answer_ip,
                         uint32_t *a_count, uint32_t *nx_count,
                         const uint8_t *query, size_t qlen,
                         uint8_t *resp, size_t resp_cap, size_t *resp_len)
{
    if (resp_len) *resp_len = 0;
    wk_dns_query_t q;
    if (wk_dns_parse_query(query, qlen, &q) != WK_DNS_PARSE_OK) return -1;
    wk_dns_answer_kind_t kind;
    size_t n = wk_dns_build_response(&q, target, answer_ip, resp, resp_cap, &kind);
    if (n == 0) return -1;
    if (kind == WK_DNS_ANSWER_A) {
        if (a_count) (*a_count)++;
    } else if (kind == WK_DNS_ANSWER_NXDOMAIN && nx_count) {
        (*nx_count)++;
    }
    if (resp_len) *resp_len = n;
    return 0;
}
