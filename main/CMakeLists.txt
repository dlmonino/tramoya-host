set(app_srcs
    "main.c"
    "wk_zip.c"
    "wk_dns.c"
    "wk_connectivity.c"
    "wk_http.c"
    "wk_storage.c"
    "wk_frontend.c"
    "wk_source.c"
    "wk_wifi_config.c"
    "http_server.c"
    "dns_server.c"
    "usb_storage.c"
    "../.deps/miniz/miniz.c"
)

# El componente USB puede proceder del registro o de una copia local.
idf_build_get_property(component_manager IDF_COMPONENT_MANAGER)
if(NOT component_manager)
    set(usb_component esp_tinyusb)
else()
    set(usb_component espressif__esp_tinyusb)
endif()

idf_component_register(
    SRCS ${app_srcs}
    INCLUDE_DIRS "." "../.deps/miniz"
    REQUIRES nvs_flash esp_wifi esp_netif esp_http_server esp_https_server mbedtls fatfs wear_levelling esp_partition
    PRIV_REQUIRES esp_psram spi_flash espcoredump esp_pm ${usb_component}
    EMBED_FILES "splash.html"
    # mbedTLS reconoce PEM si la longitud incluye el NUL final.
    EMBED_TXTFILES "certs/servercert.pem" "certs/prvtkey.pem"
)

# Se rechaza una configuración antigua para evitar un fallo de
# esp_pm_configure() durante el arranque.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    if(NOT CONFIG_PM_ENABLE OR CONFIG_PM_DFS_INIT_AUTO OR
       NOT CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ EQUAL 160)
        message(FATAL_ERROR "CPU power settings are stale; build with tools/build.py to migrate sdkconfig")
    endif()
endif()

# Usar nombres privados para miniz: los de la ROM son incompatibles.
target_compile_options(${COMPONENT_LIB} PRIVATE
    "-include" "${CMAKE_CURRENT_LIST_DIR}/wkal_miniz_namespace.h")
