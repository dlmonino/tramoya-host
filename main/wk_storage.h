/* wk_storage.h - zip serving from a FAT filesystem (platform-neutral).
 *
 * The device exposes its flash FAT partition as a USB MSC drive AND reads
 * the same partition locally. This module owns the *local* side:
 *   - opens frontend.zip on the FAT filesystem,
 *   - builds the wk_zip central-directory index over it (RAM),
 *   - serves entries through seek-based reads (FAT cannot be mmap'd),
 *   - survives host-side eject/remount: when the USB host ejects the drive
 *     the app takes the filesystem back, re-scans the zip, and continues
 *     serving.
 *
 * All filesystem I/O goes through the injected wk_fs_ops (firmware: VFS
 * POSIX calls; host tests: real files on disk or a FAT image), so the
 * whole lifecycle is testable on this machine.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wk_zip.h"
#include "wk_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Filesystem abstraction                                             */

typedef struct {
    void *(*open)(const char *path);                 /* NULL on failure */
    int (*read)(void *fh, void *buf, size_t len);    /* >0 bytes read, 0 EOF, <0 error */
    int (*seek)(void *fh, long off, int whence);     /* 0 ok, <0 error */
    int (*close)(void *fh);
    long (*size)(void *fh);                          /* <0 on error */
    /* Directory scan: repeatedly call next(); return NULL at end. The
     * returned name is valid until the next call. */
    void *(*dir_open)(const char *path);
    const char *(*dir_next)(void *dh);               /* NULL at end */
    void (*dir_close)(void *dh);
    /* Optional atomic random read, required when multiple tasks share fh. */
    int (*read_at)(void *fh, uint32_t offset, void *buf, size_t len);
} wk_fs_ops_t;

/* ------------------------------------------------------------------ */
/* Storage state machine                                              */

typedef enum {
    WK_STORAGE_MODE_APP,   /* app owns the partition (PS5 scenario)   */
    WK_STORAGE_MODE_USB,   /* USB host owns it (PC scenario)          */
} wk_storage_mode_t;

typedef enum {
    WK_STORAGE_OK = 0,
    WK_STORAGE_NO_FS,      /* partition unformatted / no zip present  */
    WK_STORAGE_ZIP_MISSING,
    WK_STORAGE_ZIP_CORRUPT,
    WK_STORAGE_OOM,
} wk_storage_status_t;

typedef struct {
    wk_fs_ops_t fs;
    const char *mount_path;      /* e.g. "/msc" or "./fat" in tests   */
    const char *zip_name;        /* e.g. "frontend.zip"               */

    wk_storage_mode_t mode;
    bool initialized;

    /* Zip index in RAM; entry data reads through zip.src -> FAT file. */
    wk_zip_t zip;
    bool zip_loaded;

    /* The open FAT zip file backing zip.src (NULL when unloaded). */
    void *zip_fh;

    /* Context for zip.src's read callback (lives with the struct; the zip
     * stores a pointer to this, never a stack copy). */
    void *fat_source_ctx;
} wk_storage_t;

/* Initialize (mode APP). OK means the fs is readable and the zip indexed.
 * Other statuses leave the struct safe to deinit. */
wk_storage_status_t wk_storage_init(wk_storage_t *s, const wk_fs_ops_t *fs,
                                    const char *mount_path,
                                    const char *zip_name);

/* Re-scan the zip after the host hands the partition back (eject), or after
 * a local re-mount. */
wk_storage_status_t wk_storage_reload(wk_storage_t *s);

/* Switch mode; USB drops the open file + index (the host may rewrite the
 * partition), APP reopens and reloads. */
void wk_storage_set_mode(wk_storage_t *s, wk_storage_mode_t mode);

/* List *.zip files (space-separated, case-insensitive ext). Returns count
 * or -1 when unavailable. */
int wk_storage_list_zips(wk_storage_t *s, char *out, size_t out_cap);

void wk_storage_deinit(wk_storage_t *s);

#ifdef __cplusplus
}
#endif
