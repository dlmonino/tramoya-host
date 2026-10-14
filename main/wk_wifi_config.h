/* Configuración Wi-Fi leída del volumen FAT USB únicamente al arrancar. */
#ifndef WK_WIFI_CONFIG_H
#define WK_WIFI_CONFIG_H

#include <stddef.h>

#define WK_WIFI_CONFIG_FILE "WIFI.TXT"
#define WK_WIFI_CONFIG_MAX_BYTES 1024
#define WK_WIFI_SSID_MAX_BYTES 32
#define WK_WIFI_PASSWORD_MAX_BYTES 63

typedef struct {
    char ssid[WK_WIFI_SSID_MAX_BYTES + 1];
    char password[WK_WIFI_PASSWORD_MAX_BYTES + 1];
    size_t ssid_len;
} wk_wifi_config_t;

typedef enum {
    WK_WIFI_CONFIG_OK = 0,
    WK_WIFI_CONFIG_NOT_FOUND,
    WK_WIFI_CONFIG_IO_ERROR,
    WK_WIFI_CONFIG_TOO_LARGE,
    WK_WIFI_CONFIG_SYNTAX,
    WK_WIFI_CONFIG_DUPLICATE,
    WK_WIFI_CONFIG_MISSING_FIELD,
    WK_WIFI_CONFIG_INVALID_SSID,
    WK_WIFI_CONFIG_INVALID_PASSWORD,
} wk_wifi_config_status_t;

/* Texto UTF-8, BOM opcional, LF/CRLF y claves exactas en minúsculas.
 * No se interpretan comillas ni comentarios al final de una asignación.
 * Se conservan espacios, # e = de los valores. Ante errores se vacía out;
 * quien llama a esta función no debe iniciar Wi-Fi. */
wk_wifi_config_status_t wk_wifi_config_parse(const char *data, size_t len,
                                            wk_wifi_config_t *out);
/* Leer como máximo MAX_BYTES+1, cerrar y validar. Llamar antes de enumerar
 * USB; no releer mientras el ordenador pueda escribir en el volumen. */
wk_wifi_config_status_t wk_wifi_config_load(const char *path,
                                           wk_wifi_config_t *out);
/* Descripciones fijas de errores, sin contenido del fichero ni contraseñas. */
const char *wk_wifi_config_status_string(wk_wifi_config_status_t status);

#endif
