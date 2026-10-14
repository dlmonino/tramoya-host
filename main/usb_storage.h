/* usb_storage.h - Acceso a la partición expuesta como almacenamiento MSC. */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Preparar la nivelación de desgaste y montar FAT sin enumerar USB.
 * Leer la configuración de arranque antes de usb_storage_start(). */
int usb_storage_init(void);

/* Cerrar FAT de la aplicación, abrir el medio para MSC y enumerar USB.
 * Si falla el desmontaje, no se anuncia el volumen. */
int usb_storage_start(void);

/* Ruta base donde se monta el sistema de archivos FAT. */
const char *usb_storage_mount_path(void);

#ifdef __cplusplus
}
#endif
