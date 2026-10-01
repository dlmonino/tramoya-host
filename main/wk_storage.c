/* wk_storage.c - zip serving from a FAT filesystem (platform-neutral).
 *
 * The zip's central directory is scanned into RAM; entry data is served by
 * seeking the open FAT file per request through a wk_source, so serving the
 * 2.5 MB payload never needs the archive in memory. When the USB host owns
 * the partition all local handles drop; an eject switches back and reloads.
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "wk_storage.h"

/* ---- FAT-file-backed source (one open file, seek per read) ---------- */

typedef struct {
    wk_storage_t *s;
} fat_source_ctx_t;

static int fat_source_read(void *ctx, uint32_t offset, void *buf, size_t len)
{
    fat_source_ctx_t *c = ctx;
    wk_storage_t *s = c->s;
    if (!s->zip_fh) return -1;
    if (s->fs.read_at) return s->fs.read_at(s->zip_fh, offset, buf, len);
    if (s->fs.seek(s->zip_fh, (long)offset, SEEK_SET) != 0) return -1;
    return s->fs.read(s->zip_fh, buf, len);
}

/* ---- lifecycle ------------------------------------------------------ */

static void drop_local(wk_storage_t *s)
{
    if (s->zip_loaded) {
        wk_zip_deinit(&s->zip);
        s->zip_loaded = false;
    }
    if (s->zip_fh) {
        s->fs.close(s->zip_fh);
        s->zip_fh = NULL;
    }
}

wk_storage_status_t wk_storage_init(wk_storage_t *s, const wk_fs_ops_t *fs,
                                    const char *mount_path,
                                    const char *zip_name)
{
    memset(s, 0, sizeof(*s));
    s->fs = *fs;
    s->mount_path = mount_path;
    s->zip_name = zip_name;
    s->mode = WK_STORAGE_MODE_APP;
    s->initialized = true;
    wk_storage_status_t rc = wk_storage_reload(s);
    if (rc != WK_STORAGE_OK) {
        /* reload failed mid-way: release anything it allocated. */
        wk_storage_deinit(s);
    }
    return rc;
}

wk_storage_status_t wk_storage_reload(wk_storage_t *s)
{
    drop_local(s);
    if (s->mode != WK_STORAGE_MODE_APP) {
        return WK_STORAGE_OK; /* nothing local while USB owns the drive */
    }

    char path[320];
    snprintf(path, sizeof(path), "%s/%s", s->mount_path, s->zip_name);

    void *fh = s->fs.open(path);
    if (!fh) return WK_STORAGE_ZIP_MISSING;
    long sz = s->fs.size(fh);
    if (sz <= 22) {
        s->fs.close(fh);
        return WK_STORAGE_ZIP_CORRUPT;
    }

    /* Index over the open file (central dir only; reads via fat_source). */
    s->zip_fh = fh;
    fat_source_ctx_t *fctx = s->fat_source_ctx;
    if (!fctx) {
        fctx = malloc(sizeof(*fctx));
        if (!fctx) {
            s->fs.close(fh);
            s->zip_fh = NULL;
            return WK_STORAGE_OOM;
        }
        s->fat_source_ctx = fctx;
    }
    fctx->s = s;
    wk_source_t src = { .ctx = fctx, .read_at = fat_source_read };

    if (wk_zip_init_from_source(&s->zip, &src, (size_t)sz) != 0) {
        s->fs.close(fh);
        s->zip_fh = NULL;
        return WK_STORAGE_ZIP_CORRUPT;
    }
    s->zip_loaded = true;
    return WK_STORAGE_OK;
}

void wk_storage_set_mode(wk_storage_t *s, wk_storage_mode_t mode)
{
    if (s->mode == mode) return;
    s->mode = mode;
    if (mode == WK_STORAGE_MODE_USB) {
        drop_local(s);
    } else {
        wk_storage_reload(s);
    }
}

int wk_storage_list_zips(wk_storage_t *s, char *out, size_t out_cap)
{
    if (s->mode != WK_STORAGE_MODE_APP) return -1;
    void *dh = s->fs.dir_open(s->mount_path);
    if (!dh) return -1;
    size_t olen = 0;
    int count = 0;
    out[0] = '\0';
    const char *name;
    while ((name = s->fs.dir_next(dh)) != NULL) {
        size_t nlen = strlen(name);
        if (nlen < 4) continue;
        if (strcasecmp(name + nlen - 4, ".zip") != 0) continue;
        if (olen + nlen + 2 > out_cap) break;
        if (olen) out[olen++] = ' ';
        memcpy(out + olen, name, nlen);
        olen += nlen;
        out[olen] = '\0';
        count++;
    }
    s->fs.dir_close(dh);
    return count;
}

void wk_storage_deinit(wk_storage_t *s)
{
    drop_local(s);
    if (s->fat_source_ctx) {
        free(s->fat_source_ctx);
        s->fat_source_ctx = NULL;
    }
    s->initialized = false;
}
