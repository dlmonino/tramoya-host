/* DNS local de TRAMOYA: contrato en docs/dns.md y pruebas en tools/tests.
 * Las mismas funciones se compilan en el Mac y en el ESP32-S3.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WK_DNS_QUERY_MAX 512
#define WK_DNS_RECV_CAP  (WK_DNS_QUERY_MAX + 1) /* Detectar exceso aun con truncamiento UDP. */
#define WK_DNS_RESP_MAX  512
#define WK_DNS_TTL       300u

typedef enum {
    WK_DNS_PARSE_BAD = -1,   /* Pregunta fuera del subconjunto soportado. */
    WK_DNS_PARSE_OK = 0,
} wk_dns_parse_t;

typedef struct {
    uint16_t qid;
    uint16_t flags;
    char name[256];          /* Minúsculas ASCII, sin punto final. */
    uint16_t qtype;
    uint16_t qclass;
    uint8_t question[WK_DNS_QUERY_MAX - 12]; /* Pregunta original. */
    uint16_t question_len;
} wk_dns_query_t;

typedef enum {
    WK_DNS_ANSWER_A = 0,
    WK_DNS_ANSWER_NXDOMAIN,
    WK_DNS_ANSWER_NODATA,
    WK_DNS_ANSWER_REFUSED,
    WK_DNS_ANSWER_NOTIMP,
} wk_dns_answer_kind_t;

/* Analizar una sola pregunta no comprimida (hasta 512 bytes).
 * QR, TC, QDCOUNT distinto de uno y preguntas inválidas se descartan.
 * Los adicionales no se interpretan: no hay soporte EDNS ni DNSSEC. */
wk_dns_parse_t wk_dns_parse_query(const uint8_t *query, size_t qlen,
                                  wk_dns_query_t *out);

/* Construir desde una pregunta analizada. target en minúsculas e IP en
 * orden de red. Devuelve longitud o cero; conserva ID, pregunta, RD y OPCODE.
 * A/ANY locales en clase IN reciben A; otros tipos ordinarios, NODATA.
 * Clase ajena: REFUSED. Operación no soportada: NOTIMP. Ajeno: NXDOMAIN. */
size_t wk_dns_build_response(const wk_dns_query_t *q,
                             const char *target, uint32_t answer_ip,
                             uint8_t *resp, size_t resp_cap,
                             wk_dns_answer_kind_t *kind_out);

/* Analizar, construir y contar respuestas A/NXDOMAIN enviables.
 * Devuelve cero si hay respuesta; -1 y longitud cero si se descarta.
 * Los contadores no cambian ante NODATA, REFUSED, NOTIMP o error. */
int wk_dns_handle_packet(const char *target, uint32_t answer_ip,
                         uint32_t *a_count, uint32_t *nx_count,
                         const uint8_t *query, size_t qlen,
                         uint8_t *resp, size_t resp_cap, size_t *resp_len);

#ifdef __cplusplus
}
#endif
