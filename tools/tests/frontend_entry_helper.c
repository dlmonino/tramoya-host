/* Comprueba las rutas y bytes del ZIP usando el núcleo HTTP real del firmware. */
#include <stdio.h>
#include <stdlib.h>
#include "wk_http.h"

static bool begin(void *ctx, const char *type, uint32_t size,
                  const char *name, bool streamed)
{
    (void)ctx; (void)size; (void)name; (void)streamed;
    fprintf(stderr, "%s\n", type);
    return true;
}
static bool write_bytes(void *ctx, const uint8_t *data, size_t size)
{
    (void)ctx;
    return fwrite(data, 1, size, stdout) == size;
}
static void end(void *ctx) { (void)ctx; }

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 2;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return 2; }
    long size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET)) { fclose(file); return 2; }
    uint8_t *data = malloc((size_t)size);
    if (!data) { fclose(file); return 2; }
    size_t got = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) { free(data); return 2; }
    wk_zip_t zip;
    if (wk_zip_init(&zip, data, (size_t)size)) { free(data); return 2; }
    const wk_http_io_t io = {.begin = begin, .write = write_bytes, .end = end};
    wk_http_status_t result = wk_http_serve_get(&zip, argv[2], "relapse\n", &io, NULL);
    wk_zip_deinit(&zip);
    free(data);
    return result == WK_HTTP_OK ? 0 : 1;
}
