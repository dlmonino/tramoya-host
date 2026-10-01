/* usb_storage.h - firmware glue for the MSC-exposed storage partition. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Bring up wear-levelling + tinyusb MSC on the storage partition.
 * Returns 0 on success; the FAT filesystem is mounted at the configured
 * base path (CONFIG_WKAL_STORAGE_MOUNT_PATH). */
int usb_storage_init(void);

/* The base path the FAT filesystem is mounted at. */
const char *usb_storage_mount_path(void);

#ifdef __cplusplus
}
#endif
