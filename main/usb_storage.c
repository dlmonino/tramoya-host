/* FAT pertenece a la aplicación solo mientras lee WIFI.TXT y el ZIP.
 * Antes de enumerar MSC se desmonta FAT y se cierra su nivelación de desgaste.
 * El volumen queda entonces exclusivamente en manos del ordenador. */
#include <string.h>
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "usb_storage.h"
#if CONFIG_TINYUSB_CDC_ENABLED
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#endif

static const char *TAG = "usb_storage";

static wl_handle_t s_wl = WL_INVALID_HANDLE;
static tinyusb_msc_storage_handle_t s_storage = NULL;
static bool s_app_mounted = false;

int usb_storage_init(void)
{
    if (s_app_mounted || s_storage) return -1;
    const esp_vfs_fat_mount_config_t config = {
        .max_files = 4,
        .format_if_mount_failed = false,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(
        CONFIG_WKAL_STORAGE_MOUNT_PATH, CONFIG_WKAL_STORAGE_PARTITION,
        &config, &s_wl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FAT mount failed: %s", esp_err_to_name(err));
        return -1;
    }
    s_app_mounted = true;
    ESP_LOGI(TAG, "storage partition: %u sectors x %u B",
             (unsigned)(wl_size(s_wl) / wl_sector_size(s_wl)),
             (unsigned)wl_sector_size(s_wl));
    ESP_LOGI(TAG, "FAT mounted at %s; USB enumeration deferred", CONFIG_WKAL_STORAGE_MOUNT_PATH);
    return 0;
}

int usb_storage_start(void)
{
    if (!s_app_mounted || s_storage) return -1;
    /* El SDK devuelve un error real de desmontaje. No se anuncia MSC si falla;
     * esp_tinyusb 2.3.0 no propaga el error de set_storage_mount_point(). */
    esp_err_t err = esp_vfs_fat_spiflash_unmount_rw_wl(
        CONFIG_WKAL_STORAGE_MOUNT_PATH, s_wl);
    s_app_mounted = false;
    s_wl = WL_INVALID_HANDLE;
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FAT unmount failed: %s", esp_err_to_name(err));
        return -1;
    }
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT,
        CONFIG_WKAL_STORAGE_PARTITION);
    if (!part || wl_mount(part, &s_wl) != ESP_OK) {
        ESP_LOGE(TAG, "storage medium mount failed after FAT unmount");
        return -1;
    }

    // El controlador MSC se instala solo después de ceder el volumen.
    tinyusb_msc_driver_config_t msc_driver_cfg = {
        .user_flags = { .auto_mount_off = 1 },
        .callback = NULL,
        .callback_arg = NULL,
    };
    if (tinyusb_msc_install_driver(&msc_driver_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "MSC driver install failed");
        return -1;
    }
    tinyusb_msc_storage_config_t storage_cfg = {
        .medium = { .wl_handle = s_wl },
        .fat_fs = {
            .base_path = CONFIG_WKAL_STORAGE_MOUNT_PATH,
            .config = {
                .max_files = 4,
                .format_if_mount_failed = false,
            },
            .do_not_format = true,
            .format_flags = 0,
        },
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
    };
    err = tinyusb_msc_new_storage_spiflash(&storage_cfg, &s_storage);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_msc_new_storage_spiflash failed: %s", esp_err_to_name(err));
        return -1;
    }
    // El anfitrión ve FAT solo cuando la aplicación ya carece de montaje.
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(NULL);
    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
        return -1;
    }
#if CONFIG_TINYUSB_CDC_ENABLED
    tinyusb_config_cdcacm_t cdc_cfg = { .cdc_port = TINYUSB_CDC_ACM_0 };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&cdc_cfg));
    ESP_ERROR_CHECK(tinyusb_console_init(TINYUSB_CDC_ACM_0));
#endif
    ESP_LOGI(TAG, "MSC device up; FAT owned exclusively by USB");
    return 0;
}

const char *usb_storage_mount_path(void)
{
    return CONFIG_WKAL_STORAGE_MOUNT_PATH;
}
