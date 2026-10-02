/* main.c - PS5 WebKit Autoloader host on ESP32-S3.
 *
 * The device brings up an open AP ("PS5 WKAL" by default). The DHCP server
 * hands clients this AP's IP as their DNS server, so opening the User's
 * Guide on a connected PS5 hits our captive DNS: manuals.playstation.net is
 * answered with the AP IP and everything else is NXDOMAIN (blocking
 * telemetry). The HTTPS (443) and HTTP (80) servers then serve the embedded
 * frontend zip exactly like the reference PC host.
 */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "dhcpserver/dhcpserver.h"
#include "esp_netif_ip_addr.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_psram.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_pm.h"
#include "dns_server.h"
#include "http_server.h"
#include "usb_storage.h"
#include "wk_storage.h"
#include "wk_source.h"
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
#include "esp_core_dump.h"
#endif

static const char *TAG = "main";

#define AP_SSID         CONFIG_WKAL_AP_SSID
#define AP_MAX_CONN     4
#define DNS_PORT        53
#define TARGET_DOMAIN   CONFIG_WKAL_TARGET_DOMAIN

static dns_server_t g_dns;
static web_server_t g_web;
static wk_storage_t g_storage;

#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
static void log_last_crash(void)
{
    esp_core_dump_summary_t *summary = malloc(sizeof(*summary));
    if (!summary) return;
    if (esp_core_dump_get_summary(summary) == ESP_OK) {
        char reason[200];
        if (esp_core_dump_get_panic_reason(reason, sizeof(reason)) == ESP_OK)
            ESP_LOGE(TAG, "Last panic: %s", reason);
        ESP_LOGE(TAG, "Crash task=%s PC=0x%08x cause=%u address=0x%08x",
                 summary->exc_task, (unsigned)summary->exc_pc,
                 (unsigned)summary->ex_info.exc_cause,
                 (unsigned)summary->ex_info.exc_vaddr);
        for (unsigned i = 0; i < summary->exc_bt_info.depth && i < 16; i++)
            ESP_LOGE(TAG, "Crash frame %u: 0x%08x", i,
                     (unsigned)summary->exc_bt_info.bt[i]);
    }
    free(summary);
}
#endif

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *evt = data;
        ESP_LOGI(TAG, "station connected: " MACSTR, MAC2STR(evt->mac));
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *evt = data;
        ESP_LOGI(TAG, "station disconnected: " MACSTR, MAC2STR(evt->mac));
    }
}


/* ---- VFS adapter for wk_storage (POSIX over the FAT mount) ---------- */

static void *vfs_open(const char *path)
{
    FILE *f = fopen(path, "rb");
    return f;
}

static int vfs_read(void *fh, void *buf, size_t len)
{
    return (int)fread(buf, 1, len, (FILE *)fh);
}

static int vfs_seek(void *fh, long off, int whence)
{
    return fseek((FILE *)fh, off, whence);
}

static int vfs_close(void *fh) { return fclose((FILE *)fh); }

/* HTTP and HTTPS run in separate tasks. Lock the seek/read pair, not just
 * the individual stdio calls, so they cannot move each other's cursor. */
static int vfs_read_at(void *fh, uint32_t offset, void *buf, size_t len)
{
    FILE *f = fh;
    flockfile(f);
    int result = -1;
    if (fseek(f, (long)offset, SEEK_SET) == 0) {
        size_t got = fread(buf, 1, len, f);
        result = ferror(f) ? -1 : (int)got;
    }
    funlockfile(f);
    return result;
}

static long vfs_size(void *fh)
{
    long cur = ftell((FILE *)fh);
    fseek((FILE *)fh, 0, SEEK_END);
    long end = ftell((FILE *)fh);
    fseek((FILE *)fh, cur, SEEK_SET);
    return end;
}

static void *vfs_dir_open(const char *path) { return opendir(path); }

static const char *vfs_dir_next(void *dh)
{
    struct dirent *de;
    do {
        de = readdir((DIR *)dh);
    } while (de && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0));
    return de ? de->d_name : NULL;
}

static void vfs_dir_close(void *dh) { closedir((DIR *)dh); }

static const wk_fs_ops_t vfs_ops = {
    .open = vfs_open, .read = vfs_read, .seek = vfs_seek, .close = vfs_close,
    .size = vfs_size, .dir_open = vfs_dir_open, .dir_next = vfs_dir_next,
    .dir_close = vfs_dir_close,
    .read_at = vfs_read_at,
};

static void start_servers(void)
{
    esp_netif_ip_info_t ip;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap) {
        ESP_LOGE(TAG, "AP netif missing");
        return;
    }
    esp_netif_get_ip_info(ap, &ip);

    if (dns_server_start(&g_dns, DNS_PORT, ip.ip.addr, TARGET_DOMAIN) != 0) {
        ESP_LOGE(TAG, "DNS server failed to start");
    }

    /* Storage: one partition, two consumers — the USB host reads it as a
     * MSC drive (payloads / frontend updates) and the app streams the
     * frontend from it. auto_mount_off keeps the local FAT mount alive
     * while a host has the drive. */
    if (usb_storage_init() != 0) {
        ESP_LOGE(TAG, "storage init failed - the AP will serve 404s only");
    }

    wk_storage_status_t src = wk_storage_init(&g_storage, &vfs_ops,
                                              usb_storage_mount_path(),
                                              CONFIG_WKAL_STORAGE_ZIP_NAME);
    if (src != WK_STORAGE_OK) {
        ESP_LOGE(TAG, "no usable %s on storage (status %d) - "
                      "plug the device into a PC and copy it",
                 CONFIG_WKAL_STORAGE_ZIP_NAME, src);
    } else {
        ESP_LOGI(TAG, "frontend '%s' indexed: %d entries",
                 CONFIG_WKAL_STORAGE_ZIP_NAME, g_storage.zip.count);
    }

    web_server_t srv = { .zip = src == WK_STORAGE_OK ? &g_storage.zip : NULL };
    g_web = srv;
    if (web_server_start(&g_web, srv.zip) != 0) {
        ESP_LOGE(TAG, "web server failed to start");
    }

    ESP_LOGI(TAG, "AP '%s' up at " IPSTR " - connect the PS5 and open the User's Guide",
             AP_SSID, IP2STR(&ip.ip));
}

void app_main(void)
{
    /* Keep the PLL and 80 MHz APB clock available to USB OTG and PSRAM.
     * FreeRTOS raises the CPU to 160 MHz while tasks are runnable, and
     * allows 80 MHz when idle, even while the SoftAP holds its APB lock.
     * XTAL-only DFS / automatic light sleep are unsuitable for this
     * always-connected USB drive and Wi-Fi access point. */
    const esp_pm_config_t power = {
        .min_freq_mhz = 80,
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = false,
    };
    ESP_ERROR_CHECK(esp_pm_configure(&power));

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "PS5 WKAL host on %s, %u MB flash, %s PSRAM",
             CONFIG_IDF_TARGET, (unsigned)(flash_size / (1024 * 1024)),
             esp_psram_is_initialized() ? "with" : "no");
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM required but not available - aborting");
        abort();
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap = esp_netif_create_default_wifi_ap();
    assert(ap != NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = "",
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));

    /* Fixed AP addressing: DHCP clients get .1 as gateway + DNS. */
    esp_netif_ip_info_t ip = {
        .ip = {.addr = 0x0101A8C0},      /* 192.168.1.1 */
        .gw = {.addr = 0x0101A8C0},
        .netmask = {.addr = 0x00FFFFFF}, /* 255.255.255.0 */
    };
    /* Even a newly created DHCP server is INIT, not STOPPED. IDF rejects
     * set_ip_info until it is explicitly stopped. */
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap, &ip));

    /* The DHCP server advertises this AP as the DNS server.
     * (esp_netif_set_dns_info on a DHCP-server netif -> dhcps_dns_setserver;
     * only ESP_NETIF_DNS_MAIN is valid for DHCP server interfaces.) */
    esp_netif_dns_info_t dnsinfo = {
        .ip = {.type = ESP_IPADDR_TYPE_V4, .u_addr = {.ip4 = ip.ip}},
    };
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dnsinfo));
    uint8_t dns_offer = OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                                         ESP_NETIF_DOMAIN_NAME_SERVER,
                                         &dns_offer, sizeof(dns_offer)));

    /* With the netif still down, this arms DHCP for WIFI_EVENT_AP_START. */
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));

    ESP_ERROR_CHECK(esp_wifi_start());

    /* The server tasks must see the netif in a started state. */
    vTaskDelay(pdMS_TO_TICKS(500));
    start_servers();
    ESP_LOGI(TAG, "CPU power management: %d-%d MHz, light sleep disabled",
             power.min_freq_mhz, power.max_freq_mhz);
#if CONFIG_TINYUSB_CDC_ENABLED
    ESP_LOGI(TAG, "Boot reset reason: %d", (int)esp_reset_reason());
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
    log_last_crash();
#endif
#endif
    /* app_main may return: the USB, DNS and HTTP tasks own their lifetimes.
     * Do not wake every five seconds to reread flash / write CDC logs. */
}

