/* main.c - Servidor PS5 WebKit Autoloader sobre ESP32-S3.
 * WIFI.TXT aporta el SSID y la contraseña WPA2 al arrancar.
 * Si falta o es inválido, el Wi-Fi queda apagado y el USB permite corregirlo.
 * DHCP anuncia el AP como DNS; el dominio de la Guía del usuario se resuelve
 * hacia el dispositivo. HTTP y HTTPS sirven la copia inmutable del ZIP en PSRAM.
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
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_pm.h"
#include "dns_server.h"
#include "http_server.h"
#include "usb_storage.h"
#include "wk_storage.h"
#include "wk_frontend.h"
#include "wk_source.h"
#include "wk_wifi_config.h"
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
#include "esp_core_dump.h"
#endif

static const char *TAG = "main";

#define AP_MAX_CONN     4
#define DNS_PORT        53
#define TARGET_DOMAIN   CONFIG_WKAL_TARGET_DOMAIN

static dns_server_t g_dns;
static web_server_t g_web;
static wk_frontend_t g_frontend;
static wk_frontend_status_t g_frontend_status;

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


/* ---- Adaptador VFS para wk_storage: POSIX sobre FAT ---------------- */

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

/* HTTP y HTTPS usan tareas distintas. Se bloquea la pareja búsqueda/lectura
 * para que una tarea no cambie el cursor de la otra entre ambas operaciones. */
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

static void *esp_frontend_alloc(size_t size)
{
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void start_servers(const char *ssid)
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

    web_server_t srv = { .zip = g_frontend_status == WK_FRONTEND_OK ?
                        &g_frontend.zip : NULL };
    g_web = srv;
    if (web_server_start(&g_web, srv.zip) != 0) {
        ESP_LOGE(TAG, "web server failed to start");
    }

    ESP_LOGI(TAG, "AP '%s' up at " IPSTR " - connect the PS5 and open the User's Guide",
             ssid, IP2STR(&ip.ip));
}

void app_main(void)
{
    /* Se mantienen PLL y APB a 80 MHz disponibles para USB OTG y PSRAM.
     * FreeRTOS eleva la CPU a 160 MHz con tareas ejecutables y permite 80 MHz
     * en reposo. El AP puede mantener su bloqueo de APB. La suspensión ligera
     * automática y la frecuencia limitada al cristal no son apropiadas aquí. */
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

    /* El ordenador no puede editar FAT hasta que terminen estas lecturas. */
    if (usb_storage_init() != 0) {
        ESP_LOGE(TAG, "storage init failed; Wi-Fi remains disabled");
        return;
    }
    wk_wifi_config_t network;
    wk_wifi_config_status_t network_status = wk_wifi_config_load(
        CONFIG_WKAL_STORAGE_MOUNT_PATH "/" WK_WIFI_CONFIG_FILE, &network);
    if (network_status != WK_WIFI_CONFIG_OK) {
        /* No se inicia Wi-Fi ni se recuperan credenciales de NVS aunque antes fueran
         * válidas. Se desmonta FAT antes de exponer el volumen de recuperación. */
        if (usb_storage_start() != 0) {
            ESP_LOGE(TAG, "USB recovery startup failed");
        }
        ESP_LOGE(TAG, "%s: %s; Wi-Fi disabled. Edit file over USB, eject and restart.",
                 WK_WIFI_CONFIG_FILE, wk_wifi_config_status_string(network_status));
        return;
    }

    /* El ZIP queda inmutable en PSRAM. FAT puede desmontarse antes de que el
     * ordenador escriba; HTTP/HTTPS nunca volverán a leer ese volumen. */
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    size_t limit = largest > 512u * 1024u ? largest - 512u * 1024u : 0;
    g_frontend_status = wk_frontend_load(
        &g_frontend, &vfs_ops,
        CONFIG_WKAL_STORAGE_MOUNT_PATH "/" CONFIG_WKAL_STORAGE_ZIP_NAME,
        limit, esp_frontend_alloc, heap_caps_free);
    if (g_frontend_status != WK_FRONTEND_OK) {
        ESP_LOGE(TAG, "frontend snapshot unavailable (status %d); USB recovery remains available",
                 g_frontend_status);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *ap = esp_netif_create_default_wifi_ap();
    assert(ap != NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /* WIFI.TXT es la única fuente de credenciales; no se guardan en NVS. */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid_len = network.ssid_len,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pairwise_cipher = WIFI_CIPHER_TYPE_CCMP,
        },
    };
    memcpy(ap_cfg.ap.ssid, network.ssid, network.ssid_len);
    memcpy(ap_cfg.ap.password, network.password, sizeof(network.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    memset(network.password, 0, sizeof(network.password));
    memset(ap_cfg.ap.password, 0, sizeof(ap_cfg.ap.password));

    /* IP fija del AP: DHCP entrega .1 como puerta de enlace y DNS. */
    esp_netif_ip_info_t ip = {
        .ip = {.addr = 0x0101A8C0},      /* 192.168.1.1 */
        .gw = {.addr = 0x0101A8C0},
        .netmask = {.addr = 0x00FFFFFF}, /* 255.255.255.0 */
    };
    /* DHCP recién creado está en INIT, no en STOPPED. IDF rechaza set_ip_info
     * hasta que se detiene el servidor de forma explícita. */
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap, &ip));

    /* DHCP anuncia este AP como DNS. esp_netif_set_dns_info llama a
     * dhcps_dns_setserver; para esta interfaz solo sirve ESP_NETIF_DNS_MAIN. */
    esp_netif_dns_info_t dnsinfo = {
        .ip = {.type = ESP_IPADDR_TYPE_V4, .u_addr = {.ip4 = ip.ip}},
    };
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap, ESP_NETIF_DNS_MAIN, &dnsinfo));
    uint8_t dns_offer = OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET,
                                         ESP_NETIF_DOMAIN_NAME_SERVER,
                                         &dns_offer, sizeof(dns_offer)));

    /* Con la interfaz aún parada, DHCP queda preparado para WIFI_EVENT_AP_START. */
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap));

    ESP_ERROR_CHECK(esp_wifi_start());

    /* Los servidores solo anuncian el traspaso si MSC arrancó correctamente. */
    vTaskDelay(pdMS_TO_TICKS(500));
    if (usb_storage_start() == 0) {
        start_servers(network.ssid);
    } else {
        ESP_LOGE(TAG, "USB startup failed; DNS and web disabled");
    }
    if (g_frontend_status == WK_FRONTEND_OK) {
        ESP_LOGI(TAG, "frontend snapshot: %u bytes, %d entries; no app FAT access",
                 (unsigned)g_frontend.archive_size, g_frontend.zip.count);
    } else {
        ESP_LOGE(TAG, "frontend unavailable (status %d); replace ZIP over USB and restart",
                 g_frontend_status);
    }
    ESP_LOGI(TAG, "CPU power management: %d-%d MHz, light sleep disabled",
             power.min_freq_mhz, power.max_freq_mhz);
#if CONFIG_TINYUSB_CDC_ENABLED
    ESP_LOGI(TAG, "Boot reset reason: %d", (int)esp_reset_reason());
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH
    log_last_crash();
#endif
#endif
    /* app_main puede terminar: las tareas USB, DNS y HTTP siguen funcionando.
     * Se evitan despertares periódicos para leer flash o escribir logs CDC. */
}

