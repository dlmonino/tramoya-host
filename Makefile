# Activate the ESP-IDF environment before invoking make.
TARGET ?= esp32s3-n4r2
PYTHON ?= python3
OPTIONS = --target $(TARGET) $(if $(FRONTEND_ZIP),--frontend "$(FRONTEND_ZIP)",) $(if $(PAYLOAD_DIR),--payload-dir "$(PAYLOAD_DIR)",)
.PHONY: all targets deps build storage package test test-asan menuconfig flash flash-all monitor zip
all: build
targets deps build storage package test test-asan menuconfig:
	$(PYTHON) tools/build.py $@ $(OPTIONS)
flash monitor:
	$(PYTHON) tools/build.py $@ $(OPTIONS) --port "$(PORT)"
flash-all:
	$(PYTHON) tools/build.py flash $(OPTIONS) --port "$(PORT)" --erase-storage
# frontend.zip is never committed: it is built from the upstream
# ps5-webkit-autoloader release (latest release tag by default; pin with
# UPSTREAM_TAG=vX.Y.Z or point UPSTREAM at an existing local clone).
zip:
	$(PYTHON) tools/build_frontend_zip.py --out main/frontend.zip --target $(TARGET) $(if $(UPSTREAM_TAG),--tag $(UPSTREAM_TAG),) $(if $(UPSTREAM),--upstream "$(UPSTREAM)",) $(if $(PAYLOAD),--payload "$(PAYLOAD)",)
