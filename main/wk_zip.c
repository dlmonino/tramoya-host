/* wk_zip.c - zip central-directory scan + path mapping (platform-neutral).
 *
 * Only the central directory is retained in RAM; entry data is pulled
 * through the configured source (flash mmap on the firmware, seek+read on
 * FAT, a plain buffer in tests). One scan code path serves both.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "wk_zip.h"

#define EOCD_SIG      0x06054B50u
#define CENTRAL_SIG   0x02014B50u
#define LOCAL_SIG     0x04034B50u

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* ------------------------------------------------------------------ */
/* Path mapping (mirrors host.py DualDirHandler._relative_path)        */

/* Percent-decode; invalid escapes and raw '%' pass through. */
static size_t pct_decode(const char *src, size_t n, char *dst, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        char c = src[i];
        if (c == '%' && i + 2 < n &&
            isxdigit((unsigned char)src[i + 1]) &&
            isxdigit((unsigned char)src[i + 2])) {
            unsigned v;
            sscanf(src + i + 1, "%2x", &v);
            dst[o++] = (char)v;
            i += 2;
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = '\0';
    return o;
}

/* Emulates posixpath.normpath() for an absolute path: '.' and empty tokens
 * are dropped, '..' pops the previous component (leading '..' above the
 * root are dropped), the result is joined with single '/'. */
static size_t normpath(const char *path, char *out, size_t out_len)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);

    char *tokens[128];
    int n = 0;
    char *save = NULL;
    for (char *tok = strtok_r(tmp, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        size_t l = strlen(tok);
        if (l == 1 && tok[0] == '.') continue;
        if (l == 2 && tok[0] == '.' && tok[1] == '.') {
            if (n > 0) n--;
            continue;
        }
        tokens[n++] = tok;
    }

    size_t olen = 0;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(tokens[i]);
        if (olen + l + 2 > out_len) return 0;
        if (olen) out[olen++] = '/';
        memcpy(out + olen, tokens[i], l);
        olen += l;
    }
    out[olen] = '\0';
    return olen;
}

bool wk_zip_map_path(const char *uri, char *out, size_t out_len)
{
    if (!uri || !out || out_len == 0) return false;

    /* Strip query and fragment, then percent-decode (host.py decodes before
     * normalizing, so %2e%2e becomes a real ".." and folds). */
    char raw[256];
    size_t n = 0;
    const char *p = uri;
    while (*p && *p != '?' && *p != '#') {
        if (n >= sizeof(raw) - 1) return false;
        raw[n++] = *p++;
    }
    raw[n] = '\0';

    char path[256];
    pct_decode(raw, n, path, sizeof(path));

    /* /document/<lang>/ps5/<rest> -> <rest> (User's Guide URLs). */
    if (strncmp(path, "/document/", 10) == 0) {
        const char *ps5 = strstr(path + 10, "/ps5/");
        if (ps5) {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "/%s", ps5 + 5);
            snprintf(path, sizeof(path), "%s", tmp);
        }
    }
    /* /app/<rest> -> <rest> (the autoloader hardcodes /app/ URLs). */
    if (strncmp(path, "/app/", 5) == 0) {
        memmove(path, path + 4, strlen(path + 4) + 1);
    }

    normpath(path, out, out_len);
    return true;
}

/* ------------------------------------------------------------------ */
/* Index scan                                                          */

int wk_zip_init_from_source(wk_zip_t *zip, wk_source_t *src, size_t len)
{
    memset(zip, 0, sizeof(*zip));
    zip->src = *src;
    zip->src_owned = false;

    uint8_t first[4];
    if (wk_source_read_at(&zip->src, 0, first, 4) != 4 || rd32(first) != LOCAL_SIG) {
        return -1;
    }
    if (len < 22 || len > UINT32_MAX) return -1;

    /* Locate the EOCD: scan backwards (bounded reads). */
    size_t scan = len < 4096 ? len : 4096;
    uint8_t tail[4096];
    if (wk_source_read_at(&zip->src, (uint32_t)(len - scan), tail, scan) != (int)scan) {
        return -1;
    }
    const uint8_t *eocd = NULL;
    for (size_t i = scan - 22 + 1; i-- > 0;) {
        if (rd32(tail + i) == EOCD_SIG) { eocd = tail + i; break; }
    }
    if (!eocd) return -1;
    uint16_t total = rd16(eocd + 10);
    uint32_t cd_off = rd32(eocd + 16);
    size_t cd_size = rd32(eocd + 12);
    size_t eocd_off = len - scan + (size_t)(eocd - tail);
    if (rd16(eocd + 4) || rd16(eocd + 6) ||
        rd16(eocd + 8) != total || total == 0 ||
        eocd_off + 22u + rd16(eocd + 20) != len ||
        (uint64_t)cd_off + cd_size > eocd_off ||
        cd_size < (size_t)total * 46u) return -1;

    /* Pull the whole central directory into RAM (small: ~200 B/entry). */
    if (cd_size > (4u << 20)) return -1; /* sanity: 153 entries ~ 25 KB */
    uint8_t *cd = malloc(cd_size ? cd_size : 1);
    if (!cd) return -1;
    if (wk_source_read_at(&zip->src, cd_off, cd, cd_size) != (int)cd_size) {
        free(cd);
        return -1;
    }

    wk_zip_entry_t *ents = calloc(total, sizeof(wk_zip_entry_t));
    char **names = calloc(total, sizeof(char *));
    if ((!ents || !names) && total) {
        free(cd);
        free(ents);
        free(names);
        return -1;
    }

    const uint8_t *p = cd;
    const uint8_t *cd_end = cd + cd_size;
    for (uint16_t i = 0; i < total; i++) {
        if ((size_t)(cd_end - p) < 46) goto fail;
        if (rd32(p) != CENTRAL_SIG) goto fail;
        uint16_t method = rd16(p + 10);
        uint32_t crc    = rd32(p + 16);
        uint32_t comp   = rd32(p + 20);
        uint32_t uncomp = rd32(p + 24);
        uint16_t nlen   = rd16(p + 28);
        uint16_t xlen   = rd16(p + 30);
        uint16_t clen   = rd16(p + 32);
        uint32_t local  = rd32(p + 42);
        size_t record_size = 46u + nlen + xlen + clen;
        if (record_size > (size_t)(cd_end - p) || !nlen ||
            memchr(p + 46, '\0', nlen) || rd16(p + 8) & 1u ||
            (method != 0 && method != 8) || (method == 0 && comp != uncomp) ||
            (uint64_t)local + 30u > cd_off) goto fail;

        char *name = malloc(nlen + 1);
        if (!name) goto fail;
        memcpy(name, p + 46, nlen);
        name[nlen] = '\0';
        for (uint16_t k = 0; k < nlen; k++) name[k] = (char)tolower((unsigned char)name[k]);

        /* Local header (name + extra lengths) read through the source. */
        uint8_t lh[30];
        if (wk_source_read_at(&zip->src, local, lh, 30) != 30) {
            free(name);
            goto fail;
        }
        if (rd32(lh) != LOCAL_SIG || rd16(lh + 8) != method || (rd16(lh + 6) & 1u)) {
            free(name);
            goto fail;
        }
        uint16_t lnlen = rd16(lh + 26);
        uint16_t lxlen = rd16(lh + 28);
        uint64_t data_start = (uint64_t)local + 30u + lnlen + lxlen;
        if (data_start + comp > cd_off) {
            free(name);
            goto fail;
        }
        uint32_t data_off = (uint32_t)data_start;

        ents[i].name = name;
        ents[i].local_off = local;
        ents[i].data_off = data_off;
        ents[i].comp_size = comp;
        ents[i].uncomp_size = uncomp;
        ents[i].method = method;
        ents[i].crc32 = crc;
        names[i] = name;

        p += record_size;
    }

    free(cd);
    zip->entries = ents;
    zip->names = names;
    zip->count = total;
    zip->initialized = true;
    return 0;

fail:
    free(cd);
    for (int i = 0; i < total; i++) free(names[i]);
    free(ents);
    free(names);
    return -1;
}

int wk_zip_init(wk_zip_t *zip, const uint8_t *archive, size_t len)
{
    wk_source_t src;
    if (!wk_source_from_buffer(&src, archive, len)) return -1;
    int rc = wk_zip_init_from_source(zip, &src, len);
    if (rc != 0) {
        wk_source_deinit(&src); /* init failed: release the buffer ctx */
        return rc;
    }
    zip->src_owned = true; /* the buffer ctx is ours now */
    return rc;
}

void wk_zip_deinit(wk_zip_t *zip)
{
    if (!zip) return;
    if (zip->src_owned) wk_source_deinit(&zip->src);
    for (int i = 0; i < zip->count; i++) free(zip->names[i]);
    free(zip->names);
    free(zip->entries);
    memset(zip, 0, sizeof(*zip));
}

const wk_zip_entry_t *wk_zip_at(const wk_zip_t *zip, int i)
{
    if (!zip->initialized || i < 0 || i >= zip->count) return NULL;
    return &((const wk_zip_entry_t *)zip->entries)[i];
}

const wk_zip_entry_t *wk_zip_find(const wk_zip_t *zip, const char *path)
{
    if (!zip->initialized || !path) return NULL;
    for (int i = 0; i < zip->count; i++) {
        if (strcmp(zip->names[i], path) == 0) return wk_zip_at(zip, i);
    }
    return NULL;
}

int wk_zip_read_entry(const wk_zip_t *zip, const wk_zip_entry_t *e,
                      uint32_t entry_offset, void *buf, size_t len)
{
    if (!zip->initialized || !e) return -1;
    if (entry_offset >= e->comp_size) return 0;
    size_t avail = (size_t)e->comp_size - entry_offset;
    if (len > avail) len = avail;
    return wk_source_read_at(&zip->src, e->data_off + entry_offset, buf, len);
}
