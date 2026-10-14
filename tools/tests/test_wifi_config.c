#include "test.h"
#include "wk_wifi_config.h"
#include <unistd.h>

static wk_wifi_config_status_t parse(const char *text, wk_wifi_config_t *out)
{
    return wk_wifi_config_parse(text, strlen(text), out);
}

static void rejects(const char *text, wk_wifi_config_status_t expected)
{
    wk_wifi_config_t out, empty = {0};
    memset(&out, 0x55, sizeof(out));
    CHECK_EQ_INT(parse(text, &out), expected);
    CHECK_MEM_EQ(&out, &empty, sizeof(out));
}

REGISTER_TEST("wifi: UTF-8 SSID and WPA2 password", test_wifi_valid)
{
    wk_wifi_config_t out;
    CHECK_EQ_INT(parse("ssid=TRAMOYA-ñ\npassword=abcdefgh\n", &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_STR(out.ssid, "TRAMOYA-ñ");
    CHECK_EQ_INT(out.ssid_len, strlen("TRAMOYA-ñ"));
    CHECK_EQ_STR(out.password, "abcdefgh");
}

REGISTER_TEST("wifi: BOM, CRLF, comments and reversed keys", test_wifi_editors)
{
    wk_wifi_config_t out;
    CHECK_EQ_INT(parse("\xef\xbb\xbf# comentario\r\n\r\npassword=abcdefgh\r\nssid=TRAMOYA", &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_STR(out.ssid, "TRAMOYA");
    CHECK_EQ_STR(out.password, "abcdefgh");
    CHECK_EQ_INT(parse("ssid=TRAMOYA\rpassword=abcdefgh\r", &out), WK_WIFI_CONFIG_MISSING_FIELD);
}

REGISTER_TEST("wifi: spaces and punctuation are literal values", test_wifi_literal)
{
    wk_wifi_config_t out;
    CHECK_EQ_INT(parse("ssid= red #1= \npassword= a#b=cde \n", &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_STR(out.ssid, " red #1= ");
    CHECK_EQ_STR(out.password, " a#b=cde ");
}

REGISTER_TEST("wifi: no missing field or empty password fallback", test_wifi_missing)
{
    rejects("", WK_WIFI_CONFIG_MISSING_FIELD);
    rejects("# only a comment\n", WK_WIFI_CONFIG_MISSING_FIELD);
    rejects("ssid=TRAMOYA\n", WK_WIFI_CONFIG_MISSING_FIELD);
    rejects("password=abcdefgh\n", WK_WIFI_CONFIG_MISSING_FIELD);
    rejects("ssid=TRAMOYA\npassword=\n", WK_WIFI_CONFIG_INVALID_PASSWORD);
    rejects("ssid=\npassword=abcdefgh\n", WK_WIFI_CONFIG_INVALID_SSID);
}

REGISTER_TEST("wifi: reject ambiguity and unknown settings", test_wifi_syntax)
{
    rejects("ssid=TRAMOYA\nssid=other\npassword=abcdefgh", WK_WIFI_CONFIG_DUPLICATE);
    rejects("ssid=TRAMOYA\npassword=abcdefgh\npassword=ijklmnop", WK_WIFI_CONFIG_DUPLICATE);
    rejects("ssid=TRAMOYA\npassword=abcdefgh\nauth=open", WK_WIFI_CONFIG_SYNTAX);
    rejects("SSID=TRAMOYA\npassword=abcdefgh", WK_WIFI_CONFIG_SYNTAX);
    rejects("ssid =TRAMOYA\npassword=abcdefgh", WK_WIFI_CONFIG_SYNTAX);
    rejects("{\"ssid\":\"TRAMOYA\"}", WK_WIFI_CONFIG_SYNTAX);
}

REGISTER_TEST("wifi: exact SSID and password byte boundaries", test_wifi_lengths)
{
    char ssid[34], password[65], text[160];
    wk_wifi_config_t out;
    memset(ssid, 's', 32); ssid[32] = 0;
    memset(password, 'p', 63); password[63] = 0;
    snprintf(text, sizeof(text), "ssid=%s\npassword=%s", ssid, password);
    CHECK_EQ_INT(parse(text, &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_INT(out.ssid_len, 32);
    CHECK_EQ_INT(strlen(out.password), 63);
    CHECK_EQ_INT(out.ssid[32], 0);
    CHECK_EQ_INT(out.password[63], 0);
    ssid[32] = 's'; ssid[33] = 0;
    snprintf(text, sizeof(text), "ssid=%s\npassword=%s", ssid, password);
    rejects(text, WK_WIFI_CONFIG_INVALID_SSID);
    password[63] = 'p'; password[64] = 0;
    snprintf(text, sizeof(text), "ssid=s\npassword=%s", password);
    rejects(text, WK_WIFI_CONFIG_INVALID_PASSWORD);
    rejects("ssid=s\npassword=1234567", WK_WIFI_CONFIG_INVALID_PASSWORD);
    CHECK_EQ_INT(parse("ssid=s\npassword=12345678", &out), WK_WIFI_CONFIG_OK);
}

REGISTER_TEST("wifi: Unicode SSID byte limit and malformed UTF-8", test_wifi_unicode)
{
    wk_wifi_config_t out;
    CHECK_EQ_INT(parse("ssid=ññññññññññññññññ\npassword=abcdefgh", &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_INT(out.ssid_len, 32);
    rejects("ssid=ñññññññññññññññññ\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=\xc0\xaf\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=\xed\xa0\x80\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=\xf4\x90\x80\x80\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=\xe2\x82\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=\xc2\x85\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=tab\t\npassword=abcdefgh", WK_WIFI_CONFIG_INVALID_SSID);
    rejects("ssid=TRAMOYA\npassword=contraseña", WK_WIFI_CONFIG_INVALID_PASSWORD);
    rejects("ssid=TRAMOYA\npassword=abcdefg\t", WK_WIFI_CONFIG_INVALID_PASSWORD);
}

REGISTER_TEST("wifi: NUL and maximum file length", test_wifi_file_bounds)
{
    const char nul[] = "ssid=TRAMOYA\npassword=abcdefgh\0ignored";
    const char valid[] = "ssid=TRAMOYA\npassword=abcdefgh\n";
    char text[WK_WIFI_CONFIG_MAX_BYTES + 1];
    wk_wifi_config_t out, empty = {0};
    CHECK_EQ_INT(wk_wifi_config_parse(nul, sizeof(nul) - 1, &out), WK_WIFI_CONFIG_SYNTAX);
    CHECK_MEM_EQ(&out, &empty, sizeof(out));
    memcpy(text, valid, sizeof(valid) - 1);
    text[sizeof(valid) - 1] = '#';
    memset(text + sizeof(valid), 'x', sizeof(text) - sizeof(valid));
    CHECK_EQ_INT(wk_wifi_config_parse(text, WK_WIFI_CONFIG_MAX_BYTES, &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_INT(wk_wifi_config_parse(text, sizeof(text), &out), WK_WIFI_CONFIG_TOO_LARGE);
    CHECK_MEM_EQ(&out, &empty, sizeof(out));
}

REGISTER_TEST("wifi: real file read, edits, missing and oversized files", test_wifi_file_io)
{
    char path[] = "/tmp/tramoya-wifi-test-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd < 0) return;
    FILE *f = fdopen(fd, "wb");
    CHECK(f != NULL);
    if (!f) { close(fd); unlink(path); return; }
    fputs("ssid=first\npassword=abcdefgh\n", f);
    CHECK_EQ_INT(fclose(f), 0);
    wk_wifi_config_t out, empty = {0};
    CHECK_EQ_INT(wk_wifi_config_load(path, &out), WK_WIFI_CONFIG_OK);
    CHECK_EQ_STR(out.ssid, "first");
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        fputs("ssid=second\npassword=ijklmnop\n", f);
        CHECK_EQ_INT(fclose(f), 0);
        CHECK_EQ_INT(wk_wifi_config_load(path, &out), WK_WIFI_CONFIG_OK);
        CHECK_EQ_STR(out.ssid, "second");
        CHECK_EQ_STR(out.password, "ijklmnop");
    }
    f = fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        for (int i = 0; i < 2048; i++) fputc('x', f);
        CHECK_EQ_INT(fclose(f), 0);
        CHECK_EQ_INT(wk_wifi_config_load(path, &out), WK_WIFI_CONFIG_TOO_LARGE);
        CHECK_MEM_EQ(&out, &empty, sizeof(out));
    }
    CHECK_EQ_INT(unlink(path), 0);
    CHECK_EQ_INT(wk_wifi_config_load(path, &out), WK_WIFI_CONFIG_NOT_FOUND);
    CHECK_MEM_EQ(&out, &empty, sizeof(out));
    CHECK_EQ_INT(wk_wifi_config_load("/", &out), WK_WIFI_CONFIG_IO_ERROR);
}

REGISTER_TEST("wifi: arbitrary bounded input never leaves partial credentials", test_wifi_random)
{
    unsigned seed = 0x12345678;
    char data[WK_WIFI_CONFIG_MAX_BYTES + 2];
    wk_wifi_config_t out, empty = {0};
    for (size_t run = 0; run < 4096; run++) {
        size_t len = run % sizeof(data);
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1664525u + 1013904223u;
            data[i] = (char)(seed >> 24);
        }
        wk_wifi_config_status_t rc = wk_wifi_config_parse(data, len, &out);
        if (rc != WK_WIFI_CONFIG_OK) CHECK_MEM_EQ(&out, &empty, sizeof(out));
        else {
            CHECK(out.ssid_len >= 1 && out.ssid_len <= 32);
            CHECK(strlen(out.password) >= 8 && strlen(out.password) <= 63);
        }
    }
}
