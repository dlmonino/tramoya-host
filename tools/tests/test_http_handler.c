/* test_http_handler.c - firmware HTTP handler through the esp_http_server
 * mock (response-capturing stub).
 *
 * Drives the real wk_httpd_handle_get from main/http_server.c against the
 * embedded frontend.zip and asserts status/headers/body via g_mock_resp.
 * Also covers the handler's 404 path and the Content-Length header.
 */
#include "test.h"
#include "http_server.h"
#include "wk_zip.h"
#include "wk_http.h"
#include "test_mem_sink.h"

/* Mock from esp_http_server_stub.c. */
typedef struct {
    int status;
    int ctype_set;
    char ctype[64];
    int clen_set;
    uint32_t clen;
    uint8_t body[6 * 1024 * 1024];
    size_t body_len;
    int send_err_called;
} mock_resp_t;

extern mock_resp_t g_mock_resp;
extern int g_mock_send_chunk_ok;
extern char g_mock_wire_headers[512];
extern int g_mock_chunk_ended;
extern size_t g_mock_send_limit, g_mock_recv_remaining;
extern int g_mock_recv_fail;
extern esp_err_t wk_httpd_handle_post(httpd_req_t *req);
extern esp_err_t wk_httpd_handle_head(httpd_req_t *req, web_server_t *srv);

/* Handler under test (weak symbol from main/http_server.c). */
extern esp_err_t wk_httpd_handle_get(httpd_req_t *req, web_server_t *srv);

/* Embedded archive (generated blob). */
extern const uint8_t g_frontend_zip[];
extern const size_t g_frontend_zip_len;

static void run_request(web_server_t *srv, const char *uri)
{
    memset(&g_mock_resp, 0, sizeof(g_mock_resp));
    g_mock_resp.status = 200;
    g_mock_wire_headers[0] = '\0';
    g_mock_chunk_ended = 0;
    g_mock_send_chunk_ok = 1;
    httpd_req_t req = { .uri = uri, .user_ctx = srv };
    wk_httpd_handle_get(&req, srv);
}

static wk_zip_t g_test_zip;

static int init_srv(web_server_t *srv)
{
    memset(srv, 0, sizeof(*srv));
    if (wk_zip_init(&g_test_zip, g_frontend_zip, g_frontend_zip_len) != 0) {
        return -1;
    }
    srv->zip = &g_test_zip;
    return 0;
}

static void test_handler_serves_index(void)
{
    web_server_t srv;
    if (init_srv(&srv) != 0) {
        fprintf(stderr, "FAIL %s: cannot index archive\n", g_current_test);
        g_assert_failures++;
        return;
    }

    run_request(&srv, "/");
    CHECK_EQ_INT(g_mock_resp.status, 200);
    CHECK_EQ_INT(g_mock_resp.ctype_set, 1);
    CHECK_EQ_STR(g_mock_resp.ctype, "text/html; charset=utf-8");
    CHECK_EQ_INT(g_mock_resp.clen_set, 0);
    CHECK(strstr(g_mock_wire_headers, "Transfer-Encoding: chunked") != NULL);
    CHECK(strstr(g_mock_wire_headers, "Content-Length:") == NULL);
    CHECK(g_mock_chunk_ended);

    const wk_zip_entry_t *e = wk_zip_find(srv.zip, "index.html");
    CHECK(e != NULL);
    if (e) {
        CHECK_EQ_INT(g_mock_resp.body_len, e->uncomp_size);
        /* index.html is a DEFLATE entry: compare against its inflated
         * bytes (via the same core the handler uses), not the raw archive. */
        uint8_t *want = malloc(e->uncomp_size);
        mem_sink_t want_sink = { .buf = want, .cap = e->uncomp_size };
        CHECK_EQ_INT(wk_http_inflate_entry(srv.zip, e, &mem_io, &want_sink),
                     WK_HTTP_OK);
        CHECK_MEM_EQ(g_mock_resp.body, want, e->uncomp_size);
        free(want);
    }
    wk_zip_deinit(&g_test_zip);
}

static void test_storage_owner_witness_without_zip(void)
{
    run_request(NULL, "/__tramoya_storage");
    CHECK_EQ_INT(g_mock_resp.status, 200);
    CHECK(strstr(g_mock_wire_headers, "Cache-Control: no-store") != NULL);
    CHECK_EQ_INT(g_mock_resp.body_len, sizeof("usb-snapshot-v1\n") - 1);
    CHECK_MEM_EQ(g_mock_resp.body, "usb-snapshot-v1\n", sizeof("usb-snapshot-v1\n") - 1);
}
REGISTER_TEST("handler: USB ownership witness independent of ZIP", test_storage_owner_witness_without_zip);

static void test_handler_404(void)
{
    web_server_t srv;
    if (init_srv(&srv) != 0) {
        g_assert_failures++;
        return;
    }
    run_request(&srv, "/definitely/not/here.js");
    CHECK_EQ_INT(g_mock_resp.send_err_called, 1);
    CHECK_EQ_INT(g_mock_resp.status, 404);
    wk_zip_deinit(&g_test_zip);
}

static void test_handler_payload_elf(void)
{
    web_server_t srv;
    if (init_srv(&srv) != 0) {
        g_assert_failures++;
        return;
    }
    run_request(&srv, "/payloads/payload.elf");
    const wk_zip_entry_t *e = wk_zip_find(srv.zip, "payloads/payload.elf");
    CHECK(e != NULL);
    if (e) {
        CHECK_EQ_INT(g_mock_resp.status, 200);
        CHECK_EQ_INT(g_mock_resp.clen_set, 0);
        CHECK_EQ_INT(g_mock_resp.body_len, e->uncomp_size);
    }
    wk_zip_deinit(&g_test_zip);
}

static void test_handler_selected_exploit(void)
{
    web_server_t srv;
    if (init_srv(&srv) != 0) {
        g_assert_failures++;
        return;
    }
    run_request(&srv, "/selected_exploit");
    CHECK_EQ_INT(g_mock_resp.status, 200);
    CHECK_EQ_INT(g_mock_resp.clen_set, 0);
    CHECK_EQ_INT(g_mock_resp.body_len, 8);
    CHECK_MEM_EQ(g_mock_resp.body, "relapse\n", 8);
    wk_zip_deinit(&g_test_zip);
}

static void test_handler_document_path(void)
{
    /* The PS5 User's Guide URL must serve the frontend. */
    web_server_t srv;
    if (init_srv(&srv) != 0) {
        g_assert_failures++;
        return;
    }
    run_request(&srv, "/document/en/ps5/index.html");
    CHECK_EQ_INT(g_mock_resp.status, 200);
    const wk_zip_entry_t *e = wk_zip_find(srv.zip, "index.html");
    CHECK(e != NULL);
    if (e) CHECK_EQ_INT(g_mock_resp.body_len, e->uncomp_size);
    wk_zip_deinit(&g_test_zip);
}

REGISTER_TEST("handler: GET / serves index.html bytes", test_handler_serves_index);
REGISTER_TEST("handler: GET unknown path -> 404", test_handler_404);
REGISTER_TEST("handler: GET payload.elf streams full size", test_handler_payload_elf);
REGISTER_TEST("handler: GET selected_exploit", test_handler_selected_exploit);
REGISTER_TEST("handler: GET /document/.../ps5/ maps to frontend", test_handler_document_path);

static void test_head(void)
{
    web_server_t srv;
    CHECK_EQ_INT(init_srv(&srv), 0);
    const char *uris[] = {"/", "/payloads/payload.elf", "/selected_exploit"};
    for (size_t i = 0; i < 3; i++) {
        memset(&g_mock_resp, 0, sizeof(g_mock_resp));
        g_mock_wire_headers[0] = '\0';
        httpd_req_t req = { .uri = uris[i], .user_ctx = &srv };
        CHECK_EQ_INT(wk_httpd_handle_head(&req, &srv), ESP_OK);
        CHECK_EQ_INT(g_mock_resp.body_len, 0);
        CHECK(strstr(g_mock_wire_headers, "Transfer-Encoding") == NULL);
        const wk_zip_entry_t *e = wk_zip_find(srv.zip, i == 0 ? "index.html" : "payloads/payload.elf");
        char expected[64];
        snprintf(expected, sizeof(expected), "Content-Length: %u\r\n",
                 i == 2 ? 8u : e->uncomp_size);
        CHECK(strstr(g_mock_wire_headers, expected) != NULL);
    }
    httpd_req_t req = { .uri = "/" };
    CHECK_EQ_INT(wk_httpd_handle_head(&req, NULL), ESP_FAIL);
    wk_zip_deinit(&g_test_zip);
}
REGISTER_TEST("handler: HEAD correct length without body, missing storage safe", test_head);

static void test_network_probes(void)
{
    const char *paths[] = {"/netstart/icst", "/netstart/ps4", "/netstart/ps5",
                          "/connecttest.txt", "/networktest/get_2m", "/networktest/get_6m"};
    const uint32_t sizes[] = {7, 7, 7, 22, 2097152, 6291456};
    /* Probes work without frontend.zip, handle partial writes, and use one
     * exact Content-Length with no chunked framing. */
    for (size_t i = 0; i < 6; i++) {
        g_mock_send_limit = 97;
        run_request(NULL, paths[i]);
        CHECK_EQ_INT(g_mock_resp.send_err_called, 0);
        CHECK_EQ_INT(g_mock_resp.body_len, sizes[i]);
        CHECK_EQ_INT(g_mock_resp.clen, sizes[i]);
        CHECK(strstr(g_mock_wire_headers, "Transfer-Encoding") == NULL);
        if (i < 3) CHECK_MEM_EQ(g_mock_resp.body, "Success", 7);
        else if (i == 3) CHECK_MEM_EQ(g_mock_resp.body, "Microsoft Connect Test", 22);
        else {
            bool zero = true;
            for (size_t j = 0; j < sizes[i]; j++) if (g_mock_resp.body[j]) zero = false;
            CHECK(zero);
        }
        run_request(NULL, "/netstart/icst?check=1");
        CHECK_MEM_EQ(g_mock_resp.body, "Success", 7);
        memset(&g_mock_resp, 0, sizeof(g_mock_resp));
        g_mock_wire_headers[0] = '\0';
        httpd_req_t req = { .uri = paths[i] };
        CHECK_EQ_INT(wk_httpd_handle_head(&req, NULL), ESP_OK);
        CHECK_EQ_INT(g_mock_resp.body_len, 0);
        CHECK_EQ_INT(g_mock_resp.clen, sizes[i]);
    }
    g_mock_send_limit = 0;
    run_request(NULL, "/netstart/icst-extra");
    CHECK_EQ_INT(g_mock_resp.status, 404);
    run_request(NULL, "/networktest/get_2m");
    g_mock_send_chunk_ok = 0;
    httpd_req_t req = { .uri = "/netstart/icst" };
    CHECK_EQ_INT(wk_httpd_handle_get(&req, NULL), ESP_FAIL);
    g_mock_send_chunk_ok = 1;
}
REGISTER_TEST("handler: connectivity GET/HEAD exact framing and bounded streaming", test_network_probes);

static void test_network_upload(void)
{
    memset(&g_mock_resp, 0, sizeof(g_mock_resp));
    g_mock_wire_headers[0] = '\0';
    g_mock_recv_remaining = 131072;
    httpd_req_t req = { .uri = "/networktest/post_128", .content_len = 131072 };
    CHECK_EQ_INT(wk_httpd_handle_post(&req), ESP_OK);
    CHECK_EQ_INT(g_mock_recv_remaining, 0);
    CHECK_EQ_INT(g_mock_resp.body_len, 0);
    CHECK(strstr(g_mock_wire_headers, "Content-Length: 0\r\n") != NULL);
    req.content_len++;
    CHECK_EQ_INT(wk_httpd_handle_post(&req), ESP_FAIL);
    CHECK_EQ_INT(g_mock_resp.status, 400);
    req.content_len = 128;
    g_mock_recv_fail = 1;
    CHECK_EQ_INT(wk_httpd_handle_post(&req), ESP_FAIL);
    g_mock_recv_fail = 0;
    req.uri = "/networktest/post_128-extra";
    CHECK_EQ_INT(wk_httpd_handle_post(&req), ESP_FAIL);
    CHECK_EQ_INT(g_mock_resp.status, 404);
}
REGISTER_TEST("handler: upload probe drains body, rejects oversized/failed input", test_network_upload);
