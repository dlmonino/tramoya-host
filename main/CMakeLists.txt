set(app_srcs
    "main.c"
    "wk_zip.c"
    "wk_dns.c"
    "wk_connectivity.c"
    "wk_http.c"
    "wk_storage.c"
    "wk_source.c"
    "http_server.c"
    "dns_server.c"
    "usb_storage.c"
    "../.deps/miniz/miniz.c"
)

idf_component_register(
    SRCS ${app_srcs}
    INCLUDE_DIRS "." "../.deps/miniz"
    REQUIRES nvs_flash esp_wifi esp_netif esp_http_server esp_https_server mbedtls fatfs wear_levelling esp_partition
    PRIV_REQUIRES esp_psram spi_flash espcoredump esp_pm
    EMBED_FILES "splash.html"
    # mbedTLS detects PEM only when the supplied length includes its trailing NUL.
    EMBED_TXTFILES "certs/servercert.pem" "certs/prvtkey.pem"
)

# A direct idf.py build with an old sdkconfig must fail here rather than
# producing firmware that aborts in esp_pm_configure() at boot.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    if(NOT CONFIG_PM_ENABLE OR CONFIG_PM_DFS_INIT_AUTO OR
       NOT CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ EQUAL 160)
        message(FATAL_ERROR "CPU power settings are stale; build with tools/build.py to migrate sdkconfig")
    endif()
endif()

# Preserve the local read mount while MSC advertises the medium to the host.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=tud_msc_test_unit_ready_cb")

# Give external miniz private names: the ROM exports incompatible names.
target_compile_options(${COMPONENT_LIB} PRIVATE
    "-include" "${CMAKE_CURRENT_LIST_DIR}/wkal_miniz_namespace.h")
