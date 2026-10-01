SHELL := /bin/bash
CC ?= gcc
IDF_EXPORT ?= $(IDF_PATH)/export.sh
# Sanitizer mode is selected with ASAN=1 (re-evaluated by the sub-make)
# instead of re-passing CFLAGS, whose nested quotes do not survive shells.
ifeq ($(ASAN),1)
SAN_CFLAGS = -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_LDFLAGS = -fsanitize=address,undefined -Wl,--gc-sections
else
SAN_CFLAGS = -O2 -g
SAN_LDFLAGS = -Wl,--gc-sections
endif
CFLAGS ?= $(SAN_CFLAGS)
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
          -DCONFIG_WKAL_SELECTED_EXPLOIT='"relapse"' \
          -I$(MAIN) -I$(TESTS) -I$(ESP_STUB)
LDFLAGS = $(SAN_LDFLAGS)
LIBS = -lz -pthread

MAIN    := ../../main
TESTS   := .
ESP_STUB:= ./esp_stub
BIN     := wk_tests

# The archive under test (set by tools/build.py from build.py --frontend or
# the synthetic fixture); injected as a C string where a source needs the
# path, e.g. the storage tests copy it into their scratch directory.
ZIPDEF = -DWKAL_ZIP_SRC=\"$(WKAL_TEST_ZIP)\"

GEN := frontend_zip_blob.c

LOCAL_SRCS := test_main.c test_fixtures.c test_mem_sink.c esp_http_server_stub.c \
              test_dns.c test_zip.c test_http.c test_http_handler.c \
              test_storage.c frontend_zip_blob.c
MAIN_SRCS  := wk_dns.c wk_connectivity.c wk_zip.c wk_http.c wk_storage.c wk_source.c http_server.c

OBJS := $(LOCAL_SRCS:.c=.o) $(MAIN_SRCS:.c=.o) miniz.o

.PHONY: all asan refdns refzip blob clean

all: $(GEN) $(BIN) zip_verify_helper
	@./$(BIN) || (echo "=== TESTS FAILED ==="; exit 1)
	@# The FAT round-trip needs the ESP-IDF fatfs tooling; on IDF-less hosts
	@# (e.g. the CI unit job) skip it with a notice. The full suite runs
	@# inside the IDF container in CI.
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

# Main sources -> local objects (same basename, built here).
$(MAIN_SRCS:.c=.o): %.o: $(MAIN)/%.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c -o $@ $<

test_storage.o: test_storage.c
	$(CC) $(CFLAGS) $(ZIPDEF) -ffunction-sections -fdata-sections -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c -o $@ $<

frontend_zip_blob.c: gen_frontend_zip_blob.py $(WKAL_TEST_ZIP)
	python3 gen_frontend_zip_blob.py

blob:
	python3 gen_frontend_zip_blob.py

asan:
	$(MAKE) clean
	$(MAKE) ASAN=1 $(GEN) $(BIN)
	@./$(BIN) || (echo "=== ASAN TESTS FAILED ==="; exit 1)

clean:
	rm -f $(BIN) zip_verify_helper $(OBJS) zip_verify_helper.o
