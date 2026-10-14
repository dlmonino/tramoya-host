/* Pruebas del contrato DNS con vectores fijos y casos de regresión. */
#include "test.h"
#include "wk_dns.h"
#include "dns_cases.h"

/* Constructor de consultas para pruebas. */

static size_t build_query(uint8_t *buf, size_t cap, uint16_t qid,
                          const char *name, uint16_t qtype)
{
    memset(buf, 0, cap);
    buf[0] = (uint8_t)(qid >> 8);
    buf[1] = (uint8_t)qid;
    buf[2] = 0x01; /* RD */
    buf[5] = 0x01; /* QDCOUNT 1 */
    size_t p = 12;
    const char *start = name;
    for (const char *c = name;; c++) {
        if (*c == '.' || *c == '\0') {
            size_t len = (size_t)(c - start);
            if (len > 63 || p + len + 1 > cap) return 0;
            buf[p++] = (uint8_t)len;
            memcpy(buf + p, start, len);
            p += len;
            start = c + 1;
            if (*c == '\0') {
                buf[p++] = 0;
                break;
            }
        }
    }
    buf[p++] = (uint8_t)(qtype >> 8);
    buf[p++] = (uint8_t)qtype;
    buf[p++] = 0x00; /* QCLASS IN */
    buf[p++] = 0x01;
    return p;
}

/* Vectores fijos independientes de la implementación. */

static void test_differential_table(void)
{
    for (int i = 0; i < ref_dns_case_count; i++) {
        const ref_dns_case_t *c = &ref_dns_cases[i];
        uint8_t resp[WK_DNS_RESP_MAX];
        size_t resp_len = 0;
        uint32_t a = 0, nx = 0;

        int rc = wk_dns_handle_packet(c->target, c->answer_ip, &a, &nx,
                                      c->query, c->query_len,
                                      resp, sizeof(resp), &resp_len);
        if (c->expect_response) {
            if (rc != 0) {
                fprintf(stderr, "FAIL %s: case '%s': no response, expected %zu bytes\n",
                        g_current_test, c->name, c->resp_len);
                g_assert_failures++;
                continue;
            }
            if (resp_len != c->resp_len) {
                fprintf(stderr, "FAIL %s: case '%s': resp len %zu != %zu\n",
                        g_current_test, c->name, resp_len, c->resp_len);
                g_assert_failures++;
                continue;
            }
            if (memcmp(resp, c->resp, resp_len) != 0) {
                fprintf(stderr, "FAIL %s: case '%s': bytes differ from documented contract\n",
                        g_current_test, c->name);
                g_assert_failures++;
                continue;
            }
            if (c->is_a) {
                if (a != 1 || nx != 0) {
                    fprintf(stderr, "FAIL %s: case '%s': counters a=%u nx=%u\n",
                            g_current_test, c->name, a, nx);
                    g_assert_failures++;
                }
            } else {
                if (a != 0 || nx != ((c->resp[3] & 15) == 3 ? 1u : 0u)) {
                    fprintf(stderr, "FAIL %s: case '%s': counters a=%u nx=%u\n",
                            g_current_test, c->name, a, nx);
                    g_assert_failures++;
                }
            }
            g_assert_count++;
        } else {
            /* Estos paquetes deben descartarse sin respuesta. */
            if (rc == 0) {
                fprintf(stderr, "FAIL %s: case '%s': answered an unsupported packet\n",
                        g_current_test, c->name);
                g_assert_failures++;
            }
            g_assert_count++;
        }
    }
}

/* Robustez del procesamiento de paquetes. */

static void test_malformed_packets(void)
{
    uint8_t resp[WK_DNS_RESP_MAX];
    size_t resp_len = 0;
    uint32_t a = 0, nx = 0;
    const char *target = "manuals.playstation.net";

    /* Paquete demasiado corto. */
    uint8_t shortq[5] = {0};
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      shortq, sizeof(shortq),
                                      resp, sizeof(resp), &resp_len), -1);

    /* Sin preguntas. */
    uint8_t zero[WK_DNS_QUERY_MAX];
    size_t ql = build_query(zero, sizeof(zero), 0x99, target, 1);
    zero[5] = 0x00;
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      zero, ql, resp, sizeof(resp), &resp_len), -1);

    /* Nombre truncado. */
    uint8_t trunc[WK_DNS_QUERY_MAX];
    ql = build_query(trunc, sizeof(trunc), 0x99, target, 1);
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      trunc, ql - 3, resp, sizeof(resp), &resp_len), -1);

    /* Compresión en la pregunta: fuera del subconjunto soportado. */
    uint8_t comp[WK_DNS_QUERY_MAX];
    ql = build_query(comp, sizeof(comp), 0x99, target, 1);
    comp[12] = 0xC0; /* Puntero de compresión como primera etiqueta. */
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      comp, ql, resp, sizeof(resp), &resp_len), -1);
}

REGISTER_TEST("dns: fixed contract vectors (bytes + counters)",
              test_differential_table);
REGISTER_TEST("dns: malformed packets rejected", test_malformed_packets);

static void test_probe_domains(void)
{
    const char *hosts[] = {"ena.net.playstation.net", "get.net.playstation.net",
                          "post.net.playstation.net", "WWW.MSFTCONNECTTEST.COM",
                          "ipv6.msftconnecttest.com"};
    uint8_t query[WK_DNS_QUERY_MAX], response[WK_DNS_RESP_MAX];
    uint32_t ip = 0x0101A8C0u, a = 0, nx = 0;
    for (size_t i = 0; i < 5; i++) {
        for (size_t j = 0; j < 3; j++) {
            const uint16_t types[] = {1, 255, 28};
            size_t qlen = build_query(query, sizeof(query), 1, hosts[i], types[j]), rlen;
            CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", ip, &a, &nx,
                         query, qlen, response, sizeof(response), &rlen), 0);
            CHECK_EQ_INT(response[3] & 15, 0);
            CHECK_EQ_INT(response[7], j == 2 ? 0 : 1);
            if (j != 2) CHECK_MEM_EQ(response + rlen - 4, &ip, 4);
        }
    }
    CHECK_EQ_INT(a, 10);
    CHECK_EQ_INT(nx, 0);
    const char *blocked[] = {"ena.net.playstation.net.example.com", "ps5.np.playstation.net",
                            "fus01.ps5.update.playstation.net", "telemetry-console.api.playstation.com"};
    for (size_t i = 0; i < 4; i++) {
        size_t qlen = build_query(query, sizeof(query), 1, blocked[i], 1), rlen;
        CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", ip, &a, &nx,
                     query, qlen, response, sizeof(response), &rlen), 0);
        CHECK_EQ_INT(response[3] & 15, 3);
    }
}
REGISTER_TEST("dns: probe hosts resolve locally, IPv6 NODATA and other domains unchanged", test_probe_domains);

/* Matriz independiente: nombres locales, externos, tipos, clases y RD. */
static void test_type_class_flags(void)
{
    const char *hosts[] = {"manuals.playstation.net", "ena.net.playstation.net", "example.com"};
    const uint16_t types[] = {1, 255, 28, 16, 15, 64, 65, 65280, 0, 41, 251, 252};
    const uint16_t classes[] = {1, 3, 4, 255};
    for (size_t h = 0; h < 3; h++)
    for (size_t t = 0; t < sizeof(types)/sizeof(types[0]); t++)
    for (size_t c = 0; c < 4; c++)
    for (unsigned rd = 0; rd < 2; rd++) {
        uint8_t q[512], r[512];
        size_t ql = build_query(q, sizeof(q), 0xaced, hosts[h], types[t]), rl = 0;
        q[2] = (uint8_t)rd;
        q[ql-2] = (uint8_t)(classes[c] >> 8);
        q[ql-1] = (uint8_t)classes[c];
        uint32_t a = 7, nx = 9, ip = 0x0101a8c0u;
        int unsupported = types[t] == 0 || types[t] == 41 || types[t] == 251 || types[t] == 252;
        int rcode = classes[c] != 1 ? 5 : unsupported ? 4 : h == 2 ? 3 : 0;
        int answer = rcode == 0 && (types[t] == 1 || types[t] == 255);
        CHECK_EQ_INT(wk_dns_handle_packet(hosts[0], ip, &a, &nx, q, ql, r, sizeof(r), &rl), 0);
        CHECK_EQ_INT(rl, ql + (answer ? 16 : 0));
        CHECK_MEM_EQ(r, q, 2);
        CHECK_EQ_INT(r[2], 0x80 | rd);
        CHECK_EQ_INT(r[3], rcode);
        CHECK_EQ_INT(r[4], 0); CHECK_EQ_INT(r[5], 1);
        CHECK_EQ_INT(r[6], 0); CHECK_EQ_INT(r[7], answer);
        CHECK_EQ_INT(r[8] | r[9] | r[10] | r[11], 0);
        CHECK_MEM_EQ(r + 12, q + 12, ql - 12);
        CHECK_EQ_INT(a, 7 + answer);
        CHECK_EQ_INT(nx, 9 + (rcode == 3));
        if (answer) {
            const uint8_t prefix[] = {192,12,0,1,0,1,0,0,1,44,0,4};
            CHECK_MEM_EQ(r + ql, prefix, sizeof(prefix));
            CHECK_MEM_EQ(r + ql + 12, &ip, 4);
        }
    }
}
REGISTER_TEST("dns: types, classes, RD reflection and RA clear", test_type_class_flags);

static void test_opcode_and_response_flags(void)
{
    uint8_t q[512], r[512];
    for (unsigned opcode = 0; opcode < 16; opcode++)
    for (unsigned rd = 0; rd < 2; rd++) {
        size_t ql = build_query(q, sizeof(q), 42, "manuals.playstation.net", 28), rl = 0;
        /* AA y bits bajos ajenos a la consulta no se propagan. */
        q[2] = (uint8_t)((opcode << 3) | rd | 4);
        q[3] = 0xff;
        CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                     NULL, NULL, q, ql, r, sizeof(r), &rl), 0);
        CHECK_EQ_INT(rl, ql);
        CHECK_EQ_INT(r[2], 0x80 | (opcode << 3) | rd);
        CHECK_EQ_INT(r[3], opcode ? 4 : 0);
        CHECK_EQ_INT(r[7], 0);
    }
}
REGISTER_TEST("dns: unsupported opcodes and no copied response flags", test_opcode_and_response_flags);

static void assert_dropped(const uint8_t *q, size_t ql)
{
    uint8_t r[512];
    size_t rl = 99;
    uint32_t a = 4, nx = 5;
    CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                 &a, &nx, q, ql, r, sizeof(r), &rl), -1);
    CHECK_EQ_INT(rl, 0);
    CHECK_EQ_INT(a, 4); CHECK_EQ_INT(nx, 5);
}

static void test_packet_boundaries(void)
{
    uint8_t q[513];
    const char *name = "manuals.playstation.net";
    size_t ql = build_query(q, sizeof(q), 7, name, 1);
    for (size_t n = 0; n < ql; n++) assert_dropped(q, n);
    assert_dropped(NULL, ql);
    assert_dropped(q, sizeof(q));
    q[2] |= 0x80; assert_dropped(q, ql);
    q[2] = 3; assert_dropped(q, ql);
    q[2] = 1; q[5] = 2; assert_dropped(q, ql);
    q[5] = 1; q[13] = 0; assert_dropped(q, ql);
    q[13] = '.'; assert_dropped(q, ql);
    q[12] = 64; assert_dropped(q, ql);
    q[12] = 0xc0; assert_dropped(q, ql);
}
REGISTER_TEST("dns: rejected packets, truncation and no stale lengths", test_packet_boundaries);

static void test_edns_and_capacity(void)
{
    uint8_t q[512], r[512];
    size_t ql = build_query(q, sizeof(q), 0xabcd, "manuals.playstation.net", 28);
    const uint8_t opt[] = {0,0,41,4,208,0,0,0,0,0,0};
    memcpy(q + ql, opt, sizeof(opt)); q[11] = 1;
    size_t rl = 0;
    CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                 NULL, NULL, q, ql + sizeof(opt), r, sizeof(r), &rl), 0);
    CHECK_EQ_INT(rl, ql); CHECK_EQ_INT(r[11], 0); CHECK_EQ_INT(r[7], 0);
    CHECK_MEM_EQ(q + 12, r + 12, ql - 12);
    for (size_t cap = 0; cap < ql; cap++) {
        rl = 99;
        uint32_t a = 7, nx = 8;
        CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                     &a, &nx, q, ql, r, cap, &rl), -1);
        CHECK_EQ_INT(rl, 0); CHECK_EQ_INT(a, 7); CHECK_EQ_INT(nx, 8);
    }
    CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                 NULL, NULL, q, ql, r, ql, &rl), 0);
    CHECK_EQ_INT(rl, ql);
}
REGISTER_TEST("dns: EDNS query and bounded response buffers", test_edns_and_capacity);

/* Límite del nombre completo en el formato de red: 255 bytes. */
static void test_name_wire_limit(void)
{
    uint8_t q[512] = {0}, r[512];
    q[5] = 1;
    size_t off = 12;
    const size_t labels[] = {63, 63, 63, 61};
    for (size_t i = 0; i < 4; i++) {
        q[off++] = (uint8_t)labels[i];
        memset(q + off, 'x', labels[i]); off += labels[i];
    }
    q[off++] = 0; q[off++] = 0; q[off++] = 1; q[off++] = 0; q[off++] = 1;
    size_t rl = 0;
    CHECK_EQ_INT(wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                 NULL, NULL, q, off, r, sizeof(r), &rl), 0);
    CHECK_EQ_INT(rl, off); CHECK_EQ_INT(r[3], 3);
    size_t last_label = 12 + 3 * 64;
    q[last_label] = 62;
    memmove(q + off - 4, q + off - 5, 5);
    q[off - 5] = 'x';
    assert_dropped(q, off + 1);
}
REGISTER_TEST("dns: maximum wire name length", test_name_wire_limit);

/* Reproducir el truncamiento de recvfrom en lwIP, además del parser directo. */
static void test_udp_receive_truncation(void)
{
    uint8_t datagram[1024], received[WK_DNS_RECV_CAP], response[512];
    const size_t lengths[] = {512, 513, 1024};
    size_t ql = build_query(datagram, sizeof(datagram), 42, "manuals.playstation.net", 1);
    for (size_t i = 0; i < 3; i++) {
        size_t copied = lengths[i] < sizeof(received) ? lengths[i] : sizeof(received);
        memcpy(received, datagram, copied);
        uint32_t a = 0, nx = 0;
        size_t rl = 99;
        int rc = wk_dns_handle_packet("manuals.playstation.net", 0x0101a8c0u,
                 &a, &nx, received, copied, response, sizeof(response), &rl);
        CHECK_EQ_INT(rc, i == 0 ? 0 : -1);
        CHECK_EQ_INT(rl, i == 0 ? ql + 16 : 0);
        CHECK_EQ_INT(a, i == 0 ? 1 : 0);
        CHECK_EQ_INT(nx, 0);
    }
}
REGISTER_TEST("dns: oversized UDP datagrams remain rejected after truncation", test_udp_receive_truncation);
