SHELL := /bin/bash
CC ?= gcc
IDF_EXPORT ?= $(IDF_PATH)/export.sh
# Apple ld and GNU ld use different dead-code elimination flags.
ifeq ($(shell uname -s),Darwin)
GC_LDFLAG := -Wl,-dead_strip
else
GC_LDFLAG := -Wl,--gc-sections
endif
# ASAN=1 activa los sanitizadores también en el make anidado, sin reenviar
# CFLAGS con comillas que el intérprete podría transformar.
ifeq ($(ASAN),1)
SAN_CFLAGS = -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_LDFLAGS = -fsanitize=address,undefined $(GC_LDFLAG)
else
SAN_CFLAGS = -O2 -g
SAN_LDFLAGS = $(GC_LDFLAG)
endif
CFLAGS ?= -O2 -g
override CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
          -DCONFIG_WKAL_SELECTED_EXPLOIT='"relapse"' \
          -I$(MAIN) -I$(TESTS) -I$(ESP_STUB)
LDFLAGS = $(SAN_LDFLAGS)
# Los sanitizadores son obligatorios incluso con CFLAGS/LDFLAGS heredados
# o pasados en la línea de make. Se añaden después de los flags del usuario.
ifeq ($(ASAN),1)
override CFLAGS += -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
override LDFLAGS += -fsanitize=address,undefined
endif
LIBS = -lz -pthread

MAIN    := ../../main
TESTS   := .
ESP_STUB:= ./esp_stub
BIN     := wk_tests

# ZIP de prueba elegido por tools/build.py: frontend externo o datos sintéticos.
# La ruta se incorpora como cadena C para las pruebas que necesitan copiarlo.
ZIPDEF = -DWKAL_ZIP_SRC=\"$(WKAL_TEST_ZIP)\"

GEN := frontend_zip_blob.c

LOCAL_SRCS := test_main.c test_fixtures.c test_mem_sink.c esp_http_server_stub.c \
              test_dns.c test_zip.c test_http.c test_http_handler.c \
              test_storage.c test_frontend.c test_wifi_config.c frontend_zip_blob.c
MAIN_SRCS  := wk_dns.c wk_connectivity.c wk_zip.c wk_http.c wk_storage.c wk_frontend.c wk_source.c wk_wifi_config.c http_server.c

OBJS := $(LOCAL_SRCS:.c=.o) $(MAIN_SRCS:.c=.o) miniz.o

.PHONY: all asan refdns refzip blob clean

all: $(GEN) $(BIN) zip_verify_helper
	@./$(BIN) || (echo "=== TESTS FAILED ==="; exit 1)
	@# La prueba completa de FAT necesita las herramientas de ESP-IDF.
	@# Sin ese entorno se omite con un aviso. La suite completa se ejecuta
	@# en el contenedor IDF de integración continua.
	@if [ -z "$$IDF_PATH" ]; then \
		echo "IDF_PATH unset - skipping FAT round-trip test"; exit 0; fi; \
	WORK=$$(mktemp -d) && cd $$WORK && \
		source $(IDF_EXPORT) >/dev/null 2>&1 && \
		python3 $(CURDIR)/test_fat_image.py; rc=$$?; cd /; rm -rf "$$WORK"; \
		exit $$rc || (echo "=== FAT IMAGE TESTS FAILED ==="; exit 1)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

ZIP_HELPER_SRCS := wk_zip.c wk_http.c wk_source.c
MINIZ := ../../.deps/miniz
miniz.o: $(MINIZ)/miniz.c $(MINIZ)/miniz.h $(MAIN)/wkal_miniz_namespace.h
	$(CC) $(CFLAGS) -include $(MAIN)/wkal_miniz_namespace.h -ffunction-sections -fdata-sections -c -o $@ $<
zip_verify_helper: zip_verify_helper.o $(ZIP_HELPER_SRCS:.c=.o) miniz.o
	$(CC) $(CFLAGS) -o $@ zip_verify_helper.o $(ZIP_HELPER_SRCS:.c=.o) miniz.o $(LDFLAGS) $(LIBS)

$(OBJS): test.h $(GEN)
wk_http.o miniz.o: $(MAIN)/miniz.h $(MAIN)/wkal_miniz_namespace.h

# Compilar los fuentes de main en objetos locales con el mismo nombre.
$(MAIN_SRCS:.c=.o): %.o: $(MAIN)/%.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c -o $@ $<

test_storage.o: test_storage.c
	$(CC) $(CFLAGS) $(ZIPDEF) -ffunction-sections -fdata-sections -c -o $@ $<

test_frontend.o: test_frontend.c
	$(CC) $(CFLAGS) $(ZIPDEF) -ffunction-sections -fdata-sections -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c -o $@ $<

frontend_zip_blob.c: gen_frontend_zip_blob.py $(WKAL_TEST_ZIP)
	python3 gen_frontend_zip_blob.py

blob:
	python3 gen_frontend_zip_blob.py

asan:
	$(MAKE) clean
	$(MAKE) ASAN=1 all

clean:
	rm -f $(BIN) zip_verify_helper $(OBJS) zip_verify_helper.o
