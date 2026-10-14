/* Regresión de la instantánea: FAT se libera antes de que MSC pueda escribir. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test.h"
#include "wk_frontend.h"

#ifndef WKAL_ZIP_SRC
#define WKAL_ZIP_SRC "../../main/frontend.zip"
#endif

extern const uint8_t g_frontend_zip[];
extern const size_t g_frontend_zip_len;
extern void *test_build_mini_zip(size_t *len_out);

typedef struct { FILE *f; long length; } archivo_t;
static int g_abiertos;
static int g_permitir_fat = 1;
static int g_corte_lectura;
static int g_forzar_error_cierre;
static int g_forzar_error_tamano;
static int g_liberaciones;

static void *abrir(const char *ruta)
{
    if (!g_permitir_fat) return NULL;
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    archivo_t *a = malloc(sizeof(*a));
    if (!a) { fclose(f); return NULL; }
    *a = (archivo_t){.f = f, .length = size};
    g_abiertos++;
    return a;
}

static long tamano(void *fh)
{
    return g_forzar_error_tamano ? -1 : ((archivo_t *)fh)->length;
}

static int leer(void *fh, void *buf, size_t len)
{
    if (g_corte_lectura) return -1;
    size_t n = fread(buf, 1, len, ((archivo_t *)fh)->f);
    return ferror(((archivo_t *)fh)->f) ? -1 : (int)n;
}

static int cerrar(void *fh)
{
    archivo_t *a = fh;
    int rc = fclose(a->f);
    free(a);
    g_abiertos--;
    return g_forzar_error_cierre ? -1 : rc;
}

static const wk_fs_ops_t ops = {
    .open = abrir, .read = leer, .close = cerrar, .size = tamano,
};

static void *reservar(size_t size) { return malloc(size); }
static void *fallar_reserva(size_t size) { (void)size; return NULL; }
static void liberar(void *p) { g_liberaciones++; free(p); }

static char ruta[128];
static char directorio[128];

static int crear_fuente(void)
{
    strcpy(directorio, "/tmp/tramoya_frontend_XXXXXX");
    if (!mkdtemp(directorio)) return -1;
    snprintf(ruta, sizeof(ruta), "%s/frontend.zip", directorio);
    FILE *origen = fopen(WKAL_ZIP_SRC, "rb");
    FILE *destino = fopen(ruta, "wb");
    if (!origen || !destino) {
        if (origen) fclose(origen);
        if (destino) fclose(destino);
        return -1;
    }
    uint8_t buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), origen)) != 0) {
        if (fwrite(buffer, 1, n, destino) != n) break;
    }
    int ok = !ferror(origen) && !ferror(destino);
    fclose(origen);
    if (fclose(destino) != 0) ok = 0;
    g_permitir_fat = 1;
    g_corte_lectura = 0;
    g_forzar_error_cierre = 0;
    g_forzar_error_tamano = 0;
    g_liberaciones = 0;
    g_abiertos = 0;
    return ok ? 0 : -1;
}

static void limpiar_fuente(void)
{
    unlink(ruta);
    rmdir(directorio);
}

REGISTER_TEST("frontend: copia inmutable sirve tras sustituir FAT", test_copia_y_reinicio)
{
    CHECK_EQ_INT(crear_fuente(), 0);
    wk_frontend_t f = {0};
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, g_frontend_zip_len + 1024,
                                  reservar, liberar), WK_FRONTEND_OK);
    CHECK_EQ_INT(g_abiertos, 0);
    CHECK(f.ready);
    CHECK_EQ_INT(f.archive_size, g_frontend_zip_len);
    CHECK_MEM_EQ(f.archive, g_frontend_zip, g_frontend_zip_len);

    /* El ordenador sustituye frontend.zip mientras la aplicación solo sirve RAM. */
    size_t nuevo_len = 0;
    uint8_t *nuevo = test_build_mini_zip(&nuevo_len);
    CHECK(nuevo != NULL);
    FILE *destino = fopen(ruta, "wb");
    CHECK(destino != NULL);
    if (destino && nuevo) {
        CHECK_EQ_INT(fwrite(nuevo, 1, nuevo_len, destino), nuevo_len);
        CHECK_EQ_INT(fclose(destino), 0);
    }
    g_permitir_fat = 0;
    const wk_zip_entry_t *antiguo = wk_zip_find(&f.zip, "app.js");
    CHECK(antiguo != NULL);
    if (antiguo) {
        uint8_t *fragmento = malloc(antiguo->comp_size);
        CHECK(fragmento != NULL);
        if (fragmento) {
            CHECK_EQ_INT(wk_zip_read_entry(&f.zip, antiguo, 0, fragmento,
                                           antiguo->comp_size), antiguo->comp_size);
            CHECK_MEM_EQ(fragmento, g_frontend_zip + antiguo->data_off, antiguo->comp_size);
            free(fragmento);
        }
    }
    CHECK(wk_zip_find(&f.zip, "hello.txt") == NULL);
    wk_frontend_deinit(&f);
    CHECK_EQ_INT(g_liberaciones, 1);

    /* El siguiente arranque vuelve a leer FAT y sirve la versión nueva. */
    g_permitir_fat = 1;
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, nuevo_len + 1024,
                                  reservar, liberar), WK_FRONTEND_OK);
    const wk_zip_entry_t *actual = wk_zip_find(&f.zip, "hello.txt");
    CHECK(actual != NULL);
    if (actual) {
        char contenido[5];
        CHECK_EQ_INT(wk_zip_read_entry(&f.zip, actual, 0, contenido, 5), 5);
        CHECK_MEM_EQ(contenido, "hello", 5);
    }
    CHECK(wk_zip_find(&f.zip, "app.js") == NULL);
    wk_frontend_deinit(&f);
    CHECK_EQ_INT(g_liberaciones, 2);
    free(nuevo);
    limpiar_fuente();
}

REGISTER_TEST("frontend: capacidad y memoria fallan sin dejar FAT abierto", test_capacidad)
{
    CHECK_EQ_INT(crear_fuente(), 0);
    wk_frontend_t f = {0};
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, g_frontend_zip_len - 1,
                                  reservar, liberar), WK_FRONTEND_TOO_LARGE);
    CHECK_EQ_INT(g_abiertos, 0);
    CHECK(!f.ready);
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, g_frontend_zip_len + 1,
                                  fallar_reserva, liberar), WK_FRONTEND_NO_MEMORY);
    CHECK_EQ_INT(g_abiertos, 0);
    CHECK_EQ_INT(g_liberaciones, 0);
    limpiar_fuente();
}

REGISTER_TEST("frontend: errores de lectura y cierre liberan la copia", test_errores)
{
    CHECK_EQ_INT(crear_fuente(), 0);
    wk_frontend_t f = {0};
    g_corte_lectura = 1;
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, g_frontend_zip_len + 1,
                                  reservar, liberar), WK_FRONTEND_IO_ERROR);
    CHECK_EQ_INT(g_abiertos, 0);
    CHECK_EQ_INT(g_liberaciones, 1);
    g_corte_lectura = 0;
    g_forzar_error_cierre = 1;
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, g_frontend_zip_len + 1,
                                  reservar, liberar), WK_FRONTEND_IO_ERROR);
    CHECK_EQ_INT(g_abiertos, 0);
    CHECK_EQ_INT(g_liberaciones, 2);
    g_forzar_error_cierre = 0;
    g_forzar_error_tamano = 1;
    CHECK_EQ_INT(wk_frontend_load(&f, &ops, ruta, g_frontend_zip_len + 1,
                                  reservar, liberar), WK_FRONTEND_IO_ERROR);
    CHECK_EQ_INT(g_abiertos, 0);
    limpiar_fuente();
}

REGISTER_TEST("frontend: ZIP inválido no queda servido", test_zip_invalido)
{
    CHECK_EQ_INT(crear_fuente(), 0);
    FILE *f = fopen(ruta, "wb");
    CHECK(f != NULL);
    if (f) {
        uint8_t relleno[100] = {0};
        CHECK_EQ_INT(fwrite(relleno, 1, sizeof(relleno), f), sizeof(relleno));
        fclose(f);
    }
    wk_frontend_t frontend = {0};
    CHECK_EQ_INT(wk_frontend_load(&frontend, &ops, ruta, 1024,
                                  reservar, liberar), WK_FRONTEND_INVALID_ZIP);
    CHECK_EQ_INT(g_abiertos, 0);
    CHECK(!frontend.ready);
    CHECK_EQ_INT(g_liberaciones, 1);
    limpiar_fuente();
}
