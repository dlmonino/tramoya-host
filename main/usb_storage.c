/* usb_storage.c - firmware glue: wear-levelling FAT partition exposed as a
 * USB MSC device while the app keeps local read access.
 *
 * Architecture (esp_tinyusb 2.3.0 semantics):
 *   - The MSC storage object maps the `storage` partition through a
 *     wear-levelling handle. READ/WRITE sector callbacks go straight to the
 *     medium with a mutex, regardless of mount_point.
 *   - mount_point starts at APP: the FATFS object is registered in VFS at
 *     /msc so the app (wk_storage) can read frontend.zip.
 *   - auto_mount_off=1 disables the component's automatic VFS unmount on
 *     USB attach, so the app keeps its local mount while the host sees the
 *     drive. Both sides read; the host may also write when it is a PC.
 *
 * Caveat: if the USB host modifies the FAT while the app has it mounted,
 * the app's cached FAT state goes stale until reboot (reads may follow a
 * stale cluster chain). Accepted usage: PC edits happen while the device is
 * unplugged from the PS5; replugging power-cycles it.
 */
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

/* esp_tinyusb 2.3.0 reports APP-owned storage as absent even with
 * auto_mount_off. Keep the read-only app mount and expose this single LUN.
 * The linker wraps only readiness; sector I/O remains in the component.
 * PC writes still require a power cycle before subsequent app reads. */
bool __wrap_tud_msc_test_unit_ready_cb(uint8_t lun)
{
    return lun == 0 && s_storage != NULL && s_wl != WL_INVALID_HANDLE;
}

int usb_storage_init(void)
{
    // 1. Wear-levelling handle over the storage partition.
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT,
        CONFIG_WKAL_STORAGE_PARTITION);
    if (!part) {
        ESP_LOGE(TAG, "storage partition '%s' missing", CONFIG_WKAL_STORAGE_PARTITION);
        return -1;
    }
    if (wl_mount(part, &s_wl) != ESP_OK) {
        ESP_LOGE(TAG, "wl_mount failed");
        return -1;
    }
    ESP_LOGI(TAG, "storage partition: %u sectors x %u B",
             (unsigned)(wl_size(s_wl) / wl_sector_size(s_wl)),
             (unsigned)wl_sector_size(s_wl));

    // 2. MSC driver (installs TinyUSB device task plumbing for MSC).
    tinyusb_msc_driver_config_t msc_driver_cfg = {
        .user_flags = { .auto_mount_off = 1 },
        .callback = NULL,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(tinyusb_msc_install_driver(&msc_driver_cfg));

    // 3. Storage object on the WL handle; FAT mounted for the app.
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
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
    };
    esp_err_t err = tinyusb_msc_new_storage_spiflash(&storage_cfg, &s_storage);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_msc_new_storage_spiflash failed: %s", esp_err_to_name(err));
        return -1;
    }

    // 4. TinyUSB device driver (enumerates to the USB host as MSC).
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
    ESP_LOGI(TAG, "MSC device up; FAT mounted at %s", CONFIG_WKAL_STORAGE_MOUNT_PATH);
    return 0;
}

const char *usb_storage_mount_path(void)
{
    return CONFIG_WKAL_STORAGE_MOUNT_PATH;
}
