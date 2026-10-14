#include "wk_wifi_config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Rechazar UTF-8 mal formado, codificaciones redundantes, sustitutos y
 * controles. La longitud del SSID se mide en bytes, como en wifi_ap_config_t. */
static int valid_ssid(const unsigned char *s, size_t len)
{
    if (!len || len > WK_WIFI_SSID_MAX_BYTES) return 0;
    for (size_t i = 0; i < len;) {
        unsigned char first = s[i++];
        if (first < 0x80) {
            if (first < 0x20 || first == 0x7f) return 0;
            continue;
        }
        unsigned continuation;
        uint32_t codepoint, minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            continuation = 1; codepoint = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation = 2; codepoint = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation = 3; codepoint = first & 0x07; minimum = 0x10000;
        } else return 0;
        if (len - i < continuation) return 0;
        while (continuation--) {
            unsigned char next = s[i++];
            if ((next & 0xc0) != 0x80) return 0;
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
            (codepoint >= 0x80 && codepoint <= 0x9f)) return 0;
    }
    return 1;
}

wk_wifi_config_status_t wk_wifi_config_parse(const char *data, size_t len,
                                            wk_wifi_config_t *out)
{
    if (!out) return WK_WIFI_CONFIG_SYNTAX;
    memset(out, 0, sizeof(*out));
    if (!data) return WK_WIFI_CONFIG_SYNTAX;
    if (len > WK_WIFI_CONFIG_MAX_BYTES) return WK_WIFI_CONFIG_TOO_LARGE;
    if (memchr(data, '\0', len)) return WK_WIFI_CONFIG_SYNTAX;

    const char *ssid = NULL, *password = NULL;
    size_t ssid_len = 0, password_len = 0, pos = 0;
    if (len >= 3 && memcmp(data, "\xef\xbb\xbf", 3) == 0) pos = 3;
    while (pos < len) {
        size_t start = pos;
        while (pos < len && data[pos] != '\n') pos++;
        size_t end = pos;
        /* CR solo es fin de línea cuando forma parte de CRLF. */
        if (pos < len && end > start && data[end - 1] == '\r') end--;
        if (pos < len) pos++;
        size_t n = end - start;
        const char *line = data + start;
        if (!n || line[0] == '#') continue;
        if (n >= 5 && memcmp(line, "ssid=", 5) == 0) {
            if (ssid) return WK_WIFI_CONFIG_DUPLICATE;
            ssid = line + 5; ssid_len = n - 5;
        } else if (n >= 9 && memcmp(line, "password=", 9) == 0) {
            if (password) return WK_WIFI_CONFIG_DUPLICATE;
            password = line + 9; password_len = n - 9;
        } else return WK_WIFI_CONFIG_SYNTAX;
    }
    if (!ssid || !password) return WK_WIFI_CONFIG_MISSING_FIELD;
    if (!valid_ssid((const unsigned char *)ssid, ssid_len))
        return WK_WIFI_CONFIG_INVALID_SSID;
    if (password_len < 8 || password_len > WK_WIFI_PASSWORD_MAX_BYTES)
        return WK_WIFI_CONFIG_INVALID_PASSWORD;
    for (size_t i = 0; i < password_len; i++) {
        unsigned char c = (unsigned char)password[i];
        if (c < 0x20 || c > 0x7e) return WK_WIFI_CONFIG_INVALID_PASSWORD;
    }
    memcpy(out->ssid, ssid, ssid_len);
    memcpy(out->password, password, password_len);
    out->ssid_len = ssid_len;
    return WK_WIFI_CONFIG_OK;
}

wk_wifi_config_status_t wk_wifi_config_load(const char *path,
                                           wk_wifi_config_t *out)
{
    if (!out) return WK_WIFI_CONFIG_SYNTAX;
    memset(out, 0, sizeof(*out));
    if (!path) return WK_WIFI_CONFIG_IO_ERROR;
    FILE *f = fopen(path, "rb");
    if (!f) return errno == ENOENT ? WK_WIFI_CONFIG_NOT_FOUND : WK_WIFI_CONFIG_IO_ERROR;
    char data[WK_WIFI_CONFIG_MAX_BYTES + 1];
    size_t len = fread(data, 1, sizeof(data), f);
    int failed = ferror(f);
    if (fclose(f) != 0) failed = 1;
    if (failed) return WK_WIFI_CONFIG_IO_ERROR;
    return wk_wifi_config_parse(data, len, out);
}

const char *wk_wifi_config_status_string(wk_wifi_config_status_t status)
{
    switch (status) {
    case WK_WIFI_CONFIG_OK: return "valid";
    case WK_WIFI_CONFIG_NOT_FOUND: return "file missing";
    case WK_WIFI_CONFIG_IO_ERROR: return "file read failed";
    case WK_WIFI_CONFIG_TOO_LARGE: return "file exceeds 1024 bytes";
    case WK_WIFI_CONFIG_SYNTAX: return "invalid text or unknown key";
    case WK_WIFI_CONFIG_DUPLICATE: return "duplicate key";
    case WK_WIFI_CONFIG_MISSING_FIELD: return "ssid or password missing";
    case WK_WIFI_CONFIG_INVALID_SSID: return "SSID must be 1-32 UTF-8 bytes without controls";
    case WK_WIFI_CONFIG_INVALID_PASSWORD: return "password must be 8-63 printable ASCII characters";
    default: return "invalid configuration";
    }
}
