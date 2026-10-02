/* dns_server.c - tiny captive-portal DNS server (thin socket layer over
 * the pure wk_dns response builder).
 *
 * Mirrors pc-host/host.py's DNSHandler:
 *   - A/ANY queries for the target domain -> this AP's IP,
 *   - known network probe domains -> this AP's IPv4 address,
 *   - everything else -> NXDOMAIN (telemetry/updates stay blocked).
 */
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "dns_server.h"

static const char *TAG = "dns";

typedef struct {
    dns_server_t *server;
    uint8_t query[WK_DNS_QUERY_MAX];
    uint8_t response[WK_DNS_RESP_MAX];
    wk_dns_query_t observed;
} dns_workspace_t;

/* ------------------------------------------------------------------ */
/* Packet handling (host-testable; no sockets involved)                */

int dns_server_handle_packet(dns_server_t *dns, const uint8_t *query,
                             size_t qlen, uint8_t *resp, size_t resp_cap,
                             size_t *resp_len)
{
    return wk_dns_handle_packet(dns->target, dns->answer_ip,
                                &dns->a_queries, &dns->nxdomain_queries,
                                query, qlen, resp, resp_cap, resp_len);
}

/* ------------------------------------------------------------------ */
/* Socket task                                                         */

static void dns_task(void *arg)
{
    /* Both packet buffers and diagnostics live on the heap. Logging through
     * the CDC console uses additional stack beyond the DNS parser itself. */
    dns_workspace_t *work = arg;
    dns_server_t *dns = work->server;

    for (;;) {
        struct sockaddr_storage from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(dns->sock, work->query, sizeof(work->query), 0,
                         (struct sockaddr *)&from, &from_len);
        if (n < 0) {
            /* A persistent socket error must not turn idle DNS into a
             * runnable busy loop that holds the CPU at its maximum clock. */
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG &&
            esp_log_level_get(TAG) >= ESP_LOG_DEBUG &&
            wk_dns_parse_query(work->query, (size_t)n, &work->observed) == WK_DNS_PARSE_OK)
            ESP_LOGD(TAG, "Query %s type=%u, DNS stack remaining=%u",
                     work->observed.name, (unsigned)work->observed.qtype,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));

        size_t resp_len = 0;
        if (dns_server_handle_packet(dns, work->query, (size_t)n,
                                     work->response, sizeof(work->response),
                                     &resp_len) == 0) {
            sendto(dns->sock, work->response, resp_len, 0, (struct sockaddr *)&from, from_len);
        }
    }
}

int dns_server_start(dns_server_t *dns, uint16_t port, uint32_t answer_ip,
                     const char *target)
{
    memset(dns, 0, sizeof(*dns));
    dns->sock = -1;
    strncpy(dns->target, target, sizeof(dns->target) - 1);
    for (char *p = dns->target; *p; p++) *p = (char)tolower((unsigned char)*p);
    dns->answer_ip = answer_ip;

    dns->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (dns->sock < 0) {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        return -1;
    }
    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(dns->sock, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        ESP_LOGE(TAG, "bind(:%u) failed: %d", port, errno);
        close(dns->sock);
        dns->sock = -1;
        return -1;
    }
    dns_workspace_t *work = calloc(1, sizeof(*work));
    if (!work) {
        close(dns->sock);
        dns->sock = -1;
        return -1;
    }
    work->server = dns;
    /* ESP_LOGI's console/VFS path is part of the task's full call chain.
     * 3072 bytes overflowed after adding query diagnostics on the S3. */
    if (xTaskCreate(dns_task, "dns", 8192, work, 5, NULL) != pdPASS) {
        free(work);
        close(dns->sock);
        dns->sock = -1;
        dns->sock = -1;
        return -1;
    }
    const uint8_t *a = (const uint8_t *)&dns->answer_ip;
    ESP_LOGI(TAG, "DNS on :%u, %s -> %u.%u.%u.%u", port, dns->target,
             a[0], a[1], a[2], a[3]);
    return 0;
}

void dns_server_stop(dns_server_t *dns)
{
    if (dns->sock >= 0) {
        close(dns->sock);
        dns->sock = -1;
    }
}
