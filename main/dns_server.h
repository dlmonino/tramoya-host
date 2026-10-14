/* Servidor DNS UDP local; contrato de respuestas en wk_dns.h y docs/dns.md. */
#pragma once

#include <stdint.h>
#include "wk_dns.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int sock;                 /* Socket UDP; -1 si está detenido. */
    char target[128];         /* Dominio local en minúsculas. */
    uint32_t answer_ip;       /* Dirección IPv4 local, en orden de red. */
    uint32_t a_queries;       /* Contadores de respuestas. */
    uint32_t nxdomain_queries;
} dns_server_t;

/* Procesar paquetes sin acceder a un socket; la lógica se delega en wk_dns. */
int dns_server_handle_packet(dns_server_t *dns, const uint8_t *query,
                             size_t qlen, uint8_t *resp, size_t resp_cap,
                             size_t *resp_len);

int dns_server_start(dns_server_t *dns, uint16_t port, uint32_t answer_ip,
                     const char *target);
void dns_server_stop(dns_server_t *dns);

#ifdef __cplusplus
}
#endif
