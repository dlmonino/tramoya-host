/* Protocol compatibility tests using fixed project-authored cases. */
#include "test.h"
#include "wk_dns.h"
#include "dns_cases.h"

/* --- query builder (same wire format as the generator) --------------- */

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

/* --- differential cases ---------------------------------------------- */

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
                fprintf(stderr, "FAIL %s: case '%s': bytes differ from host.py\n",
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
                if (a != 0 || nx != 1) {
                    fprintf(stderr, "FAIL %s: case '%s': counters a=%u nx=%u\n",
                            g_current_test, c->name, a, nx);
                    g_assert_failures++;
                }
            }
            g_assert_count++;
        } else {
            /* host.py's DNSHandler answers nothing for these. */
            if (rc == 0) {
                fprintf(stderr, "FAIL %s: case '%s': answered a packet host.py ignores\n",
                        g_current_test, c->name);
                g_assert_failures++;
            }
            g_assert_count++;
        }
    }
}

/* --- firmware-specific robustness (no host.py ground truth) ----------- */

static void test_malformed_packets(void)
{
    uint8_t resp[WK_DNS_RESP_MAX];
    size_t resp_len = 0;
    uint32_t a = 0, nx = 0;
    const char *target = "manuals.playstation.net";

    /* Too short. */
    uint8_t shortq[5] = {0};
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      shortq, sizeof(shortq),
                                      resp, sizeof(resp), &resp_len), -1);

    /* Zero questions. */
    uint8_t zero[WK_DNS_QUERY_MAX];
    size_t ql = build_query(zero, sizeof(zero), 0x99, target, 1);
    zero[5] = 0x00;
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      zero, ql, resp, sizeof(resp), &resp_len), -1);

    /* Truncated name. */
    uint8_t trunc[WK_DNS_QUERY_MAX];
    ql = build_query(trunc, sizeof(trunc), 0x99, target, 1);
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      trunc, ql - 3, resp, sizeof(resp), &resp_len), -1);

    /* Compressed pointer in the question (never legal, must not be chased). */
    uint8_t comp[WK_DNS_QUERY_MAX];
    ql = build_query(comp, sizeof(comp), 0x99, target, 1);
    comp[12] = 0xC0; /* fake compression pointer as first label */
    CHECK_EQ_INT(wk_dns_handle_packet(target, 0x0101A8C0u, &a, &nx,
                                      comp, ql, resp, sizeof(resp), &resp_len), -1);
}

REGISTER_TEST("dns: differential table vs host.py (bytes + counters)",
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
