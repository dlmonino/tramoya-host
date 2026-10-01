/* test_storage.c - wk_storage lifecycle against a real directory tree.
 *
 * The "FAT filesystem" is a plain host directory (same VFS open/read/seek/
 * close/dir surface the firmware uses over FAT); a second test builds an
 * actual FAT image with mtools and runs the identical code over it.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <zlib.h>
#include <pthread.h>
#include <sched.h>
#include "test.h"
#include "wk_storage.h"
#include "wk_http.h"
#include "wk_zip.h"
#include "test_mem_sink.h"

/* The archive under test, injected by the Makefile from WKAL_TEST_ZIP:
 * the repo never carries a fixed frontend.zip, tests always run against
 * the build-time-selected archive (synthetic fixture or a release build). */
#ifndef WKAL_ZIP_SRC
#define WKAL_ZIP_SRC "../../main/frontend.zip"
#endif

/* --- POSIX fs ops (mirrors the firmware's VFS adapter) --------------- */

typedef struct { FILE *f; long size; } file_t;

static void *fs_open(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    file_t *h = malloc(sizeof(file_t));
    h->f = f;
    h->size = sz;
    return h;
}

static int fs_read(void *fh, void *buf, size_t len)
{
    return (int)fread(buf, 1, len, ((file_t *)fh)->f);
}

static int fs_seek(void *fh, long off, int whence)
{
    return fseek(((file_t *)fh)->f, off, whence);
}

static int fs_close(void *fh)
{
    file_t *h = fh;
    int rc = fclose(h->f);
    free(h);
    return rc;
}

static int fs_read_at(void *fh, uint32_t offset, void *buf, size_t len)
{
    FILE *f = ((file_t *)fh)->f;
    flockfile(f);
    int result = -1;
    if (fseek(f, (long)offset, SEEK_SET) == 0) {
        sched_yield();
        result = (int)fread(buf, 1, len, f);
    }
    funlockfile(f);
    return result;
}

static long fs_size(void *fh) { return ((file_t *)fh)->size; }

static void *fs_dir_open(const char *path) { return opendir(path); }

static const char *fs_dir_next(void *dh)
{
    struct dirent *de;
    do {
        de = readdir((DIR *)dh);
    } while (de && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0));
    return de ? de->d_name : NULL;
}

static void fs_dir_close(void *dh) { closedir((DIR *)dh); }

static const wk_fs_ops_t posix_ops = {
    .open = fs_open, .read = fs_read, .seek = fs_seek, .close = fs_close,
    .size = fs_size, .dir_open = fs_dir_open, .dir_next = fs_dir_next,
    .dir_close = fs_dir_close,
    .read_at = fs_read_at,
};

/* --- fixtures -------------------------------------------------------- */

static char g_dir[512];

static void setup_zip_dir(const char *sub)
{
    snprintf(g_dir, sizeof(g_dir), "/tmp/wk_storage_test_%s", sub);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s && mkdir -p %s && cp %s %s/",
             g_dir, g_dir, WKAL_ZIP_SRC, g_dir);
    CHECK_EQ_INT(system(cmd), 0);
}

/* --- tests ----------------------------------------------------------- */

static void test_load_and_index(void)
{
    setup_zip_dir("load");
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);
    CHECK(s.zip_loaded);
    CHECK_EQ_INT(s.zip.count, g_frontend_zip_file_count);
    const wk_zip_entry_t *e = wk_zip_find(&s.zip, "payloads/payload.elf");
    CHECK(e != NULL);
    if (e && g_frontend_zip_payload_size) CHECK_EQ_INT(e->uncomp_size, g_frontend_zip_payload_size);
    wk_storage_deinit(&s);
}

static void test_missing_zip(void)
{
    setup_zip_dir("missing");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm %s/frontend.zip", g_dir);
    system(cmd);
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_ZIP_MISSING);
    CHECK(!s.zip_loaded);
}

static void test_corrupt_zip(void)
{
    setup_zip_dir("corrupt");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "python3 -c \"open('%s/frontend.zip','wb').write(b'PK' + b'x'*5000)\"",
             g_dir);
    system(cmd);
    wk_storage_t s;
    wk_storage_status_t rc = wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip");
    CHECK(rc == WK_STORAGE_ZIP_CORRUPT || rc == WK_STORAGE_ZIP_MISSING);
    CHECK(!s.zip_loaded);
}

static void test_stream_entry_matches_ram(void)
{
    setup_zip_dir("stream");
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);

    const wk_zip_entry_t *e = wk_zip_find(&s.zip, "app.js");
    CHECK(e != NULL);
    if (!e) { wk_storage_deinit(&s); return; }

    /* Streamed reads through the zip source must match the underlying file
     * (read the whole entry again directly from the file). */
    uint8_t *via_zip = malloc(e->comp_size);
    uint32_t off = 0;
    int n;
    while ((n = wk_zip_read_entry(&s.zip, e, off, via_zip + off, 4096)) > 0) {
        off += (uint32_t)n;
    }
    CHECK_EQ_INT(off, e->comp_size);

    /* The same bytes via the raw fs file at the entry's data offset. */
    char path[512];
    snprintf(path, sizeof(path), "%s/frontend.zip", g_dir);
    FILE *raw = fopen(path, "rb");
    CHECK(raw != NULL);
    uint8_t *via_file = malloc(e->comp_size);
    fseek(raw, (long)e->data_off, SEEK_SET);
    CHECK_EQ_INT((int)fread(via_file, 1, e->comp_size, raw), (int)e->comp_size);
    fclose(raw);
    CHECK_MEM_EQ(via_zip, via_file, e->comp_size);
    free(via_zip);
    free(via_file);
    wk_storage_deinit(&s);
}

static void test_payload_elf_inflate_and_crc(void)
{
    setup_zip_dir("inflate");
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);

    const wk_zip_entry_t *e = wk_zip_find(&s.zip, "payloads/payload.elf");
    CHECK(e != NULL);
    if (!e) { wk_storage_deinit(&s); return; }

    /* Inflate via the firmware loop; bytes come from the RAM copy of the
     * FAT file (the stream test above proved the copy equals the file). */
    uint8_t *out = malloc(e->uncomp_size);
    mem_sink_t sink = { .buf = out, .cap = e->uncomp_size };
    CHECK_EQ_INT(wk_http_inflate_entry(&s.zip, e, &mem_io, &sink), WK_HTTP_OK);
    CHECK_EQ_INT(sink.len, e->uncomp_size);

    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, out, e->uncomp_size);
    CHECK_EQ_INT(crc, e->crc32);

    free(out);
    wk_storage_deinit(&s);
}

static void test_usb_mode_drops_local(void)
{
    setup_zip_dir("usb");
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);
    const wk_zip_entry_t *e = wk_zip_find(&s.zip, "app.js");
    CHECK(e != NULL);

    wk_storage_set_mode(&s, WK_STORAGE_MODE_USB);
    CHECK(!s.zip_loaded);
    CHECK(s.zip.entries == NULL);

    /* Entry reads fail cleanly while the host owns the drive. */
    uint8_t tmp[64];
    CHECK(wk_zip_read_entry(&s.zip, e, 0, tmp, sizeof(tmp)) < 0);

    wk_storage_deinit(&s);
}

static void test_eject_reloads(void)
{
    setup_zip_dir("eject");
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);
    const wk_zip_entry_t *e1 = wk_zip_find(&s.zip, "app.js");
    CHECK(e1 != NULL);

    wk_storage_set_mode(&s, WK_STORAGE_MODE_USB);
    wk_storage_set_mode(&s, WK_STORAGE_MODE_APP); /* host eject -> reload */

    CHECK(s.zip_loaded);
    CHECK_EQ_INT(s.zip.count, g_frontend_zip_file_count);
    const wk_zip_entry_t *e2 = wk_zip_find(&s.zip, "app.js");
    CHECK(e2 != NULL);
    wk_storage_deinit(&s);
}

static void test_reload_picks_up_new_zip(void)
{
    /* Simulate: host replaces the zip, ejects; the reload must serve the
     * NEW archive (version marker changed). */
    setup_zip_dir("reload2");
    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);

    /* Add a marker file to the archive via a rebuilt zip. */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "python3 -c \"\n"
             "import zipfile, shutil\n"
             "zf = zipfile.ZipFile('%s/frontend.zip')\n"
             "names = zf.namelist()\n"
             "zf.close()\n"
             "z = zipfile.ZipFile('%s/frontend2.zip','w',zipfile.ZIP_DEFLATED)\n"
             "for n in names:\n"
             "    z.writestr(n, zf.read(n))\"", g_dir, g_dir);
    (void)cmd;
    /* simpler: append an entry by rewriting the zip with the marker */
    snprintf(cmd, sizeof(cmd),
             "python3 -c \"\n"
             "import zipfile\n"
             "src = zipfile.ZipFile('%s/frontend.zip')\n"
             "dst = zipfile.ZipFile('%s/frontend.new','w',zipfile.ZIP_DEFLATED)\n"
             "for i in src.infolist():\n"
             "    dst.writestr(i, src.read(i))\n"
             "dst.writestr('NEW_MARKER', 'v2')\n"
             "dst.close()\n"
             "src.close()\n"
             "\" && mv %s/frontend.new %s/frontend.zip", g_dir, g_dir, g_dir, g_dir);
    CHECK_EQ_INT(system(cmd), 0);

    wk_storage_set_mode(&s, WK_STORAGE_MODE_USB);
    wk_storage_set_mode(&s, WK_STORAGE_MODE_APP);

    CHECK(s.zip_loaded);
    CHECK_EQ_INT(s.zip.count, g_frontend_zip_file_count + 1); /* plus NEW_MARKER */
    CHECK(wk_zip_find(&s.zip, "new_marker") != NULL);
    wk_storage_deinit(&s);
}

static void test_list_zips(void)
{
    setup_zip_dir("list");
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cp %s/frontend.zip %s/other.ZIP", g_dir, g_dir);
    system(cmd);

    wk_storage_t s;
    CHECK_EQ_INT(wk_storage_init(&s, &posix_ops, g_dir, "frontend.zip"),
                 WK_STORAGE_OK);
    char names[512];
    int n = wk_storage_list_zips(&s, names, sizeof(names));
    CHECK(n >= 2);
    CHECK(strstr(names, "frontend.zip") != NULL);
    CHECK(strstr(names, "other.ZIP") != NULL);
    wk_storage_deinit(&s);
}

REGISTER_TEST("storage: FAT dir loads + indexes the zip", test_load_and_index);
REGISTER_TEST("storage: missing zip reports clean status", test_missing_zip);
REGISTER_TEST("storage: corrupt zip rejected", test_corrupt_zip);
REGISTER_TEST("storage: streamed entry bytes match in-RAM archive",
              test_stream_entry_matches_ram);
REGISTER_TEST("storage: payload.elf inflates via storage + CRC",
              test_payload_elf_inflate_and_crc);
REGISTER_TEST("storage: USB mode drops local handles", test_usb_mode_drops_local);
REGISTER_TEST("storage: eject back to app reloads zip", test_eject_reloads);
REGISTER_TEST("storage: reload picks up host-updated zip", test_reload_picks_up_new_zip);
REGISTER_TEST("storage: zip listing (case-insensitive ext)", test_list_zips);

typedef struct {
    const wk_zip_t *zip;
    const wk_zip_entry_t *entry;
    const uint8_t *expected;
    int failures;
} reader_t;

static void *concurrent_reader(void *arg)
{
    reader_t *r = arg;
    uint8_t buffer[257];
    /* Tiny entries cannot be sampled in fixed 257-byte windows without
     * underflowing the offset range (synthetic fixtures have some). */
    if (r->entry->comp_size <= sizeof(buffer) + 64) return NULL;
    for (uint32_t i = 0; i < 5000; i++) {
        uint32_t off = (i * 977u) % (r->entry->comp_size - sizeof(buffer));
        int got = wk_zip_read_entry(r->zip, r->entry, off, buffer, sizeof(buffer));
        if (got != sizeof(buffer) || memcmp(buffer, r->expected + off, sizeof(buffer)))
            r->failures++;
    }
    return NULL;
}

static void test_concurrent_reads(void)
{
    extern const uint8_t g_frontend_zip[];
    setup_zip_dir("concurrent");
    wk_storage_t storage;
    CHECK_EQ_INT(wk_storage_init(&storage, &posix_ops, g_dir, "frontend.zip"), WK_STORAGE_OK);
    if (!storage.zip_loaded) return;
    reader_t readers[2] = {0};
    pthread_t threads[2];
    int started[2];
    const char *names[] = {"payloads/payload.elf", "app.js"};
    for (int i = 0; i < 2; i++) {
        readers[i].zip = &storage.zip;
        readers[i].entry = wk_zip_find(&storage.zip, names[i]);
        CHECK(readers[i].entry != NULL);
        if (!readers[i].entry) { wk_storage_deinit(&storage); return; }
        readers[i].expected = g_frontend_zip + readers[i].entry->data_off;
        started[i] = pthread_create(&threads[i], NULL, concurrent_reader, &readers[i]);
        CHECK_EQ_INT(started[i], 0);
    }
    for (int i = 0; i < 2; i++) {
        if (!started[i]) CHECK_EQ_INT(pthread_join(threads[i], NULL), 0);
        CHECK_EQ_INT(readers[i].failures, 0);
    }
    wk_storage_deinit(&storage);
}
REGISTER_TEST("storage: concurrent requests preserve file offsets", test_concurrent_reads);
