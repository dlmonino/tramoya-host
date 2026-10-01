# PS5 WKAL ESP32-S3 Host

An ESP32-S3 firmware that merges the **PC host** flow of
[itsPLK/ps5-webkit-autoloader](https://github.com/itsPLK/ps5-webkit-autoloader)
with a **USB drive** — one device, plugged straight into the PS5:

1. **USB**: the ESP32 enumerates as a mass-storage drive (TinyUSB MSC over a
   wear-levelled FAT partition). The PS5 reads `ps5_autoloader/` payloads
   from it; a PC can rewrite `frontend.zip` and drop new payloads.
2. **WiFi**: the ESP32 brings up an open AP (`PS5 WKAL`). DHCP hands clients
   the AP's IP as their DNS server; DNS :53 answers `manuals.playstation.net`
   with the AP IP and NXDOMAINs everything else.
3. **HTTPS :443 / HTTP :80** serve the frontend **from the same FAT
   partition**, streaming entries (tinfl ring buffer) — the zip never lives
   in the firmware image or in RAM.

Both roles run simultaneously: PS5 powers the device over USB, mounts the
drive, connects its WiFi to the AP, opens the User's Guide, and the exploit
chain loads payloads straight off the drive. Update flow: unplug from PS5 →
plug into a PC → replace `frontend.zip` → unplug → replug (power cycle
reloads).

## Hardware targets

All ESP32-S3 flash/PSRAM combinations are supported — pick the board with
`--target` (or `make build TARGET=...`); each gets its own partition layout,
sdkconfig and build directory:

| target          | flash | PSRAM | PSRAM mode | storage partition |
|-----------------|-------|-------|------------|-------------------|
| `esp32s3-n4r2`  | 4 MB  | 2 MB  | quad       | 2.44 MB           |
| `esp32s3-n8r2`  | 8 MB  | 2 MB  | quad       | 5.81 MB           |
| `esp32s3-n8r8`  | 8 MB  | 8 MB  | octal      | 5.81 MB           |
| `esp32s3-n16r2` | 16 MB | 2 MB  | quad       | 13.81 MB          |
| `esp32s3-n16r8` | 16 MB | 8 MB  | octal      | 13.81 MB          |
| `esp32s3-n32r8` | 32 MB | 8 MB  | octal      | 29.87 MB          |

`python3 tools/build.py targets` prints the live list from
`targets/boards.json`.

## The frontend is built from upstream — never committed

This repository does **not** vendor `frontend.zip` or the upstream exploit
frontend. At packaging time the build:

1. resolves the **latest release tag** of the upstream
   `ps5-webkit-autoloader` project (GitHub API, with a git-tags fallback; pin
   one with `--tag v0.5.2` or `UPSTREAM_TAG=`),
2. clones that tag **with submodules** into `.deps/upstream/<tag>/` (cached
   and reused while the tag still points at the same commit),
3. runs upstream's own preparation scripts (`download_deps.sh`,
   `apply_relapse_patch.sh`, `apply_slopkit_patch.sh`, `apply_umtx2_patch.sh`),
4. packs `frontend/autoloader` + `pc-host/overrides` into `frontend.zip`
   (DEFLATE, upstream path filters, generated icons, `[[…PLACEHOLDER]]`
   version/build substitution and the unsupported-browser splash fix).

Every artifact records exactly where its frontend came from in a
`frontend.zip-source.json` (tag, commit, SHA256) — also embedded into
`firmware-manifest.json`.

### Just want frontend.zip for the USB drive?

You do not need ESP-IDF for this — only Python 3, git and bash:

```sh
python3 tools/build_frontend_zip.py
# → frontend.zip (+ frontend.zip-source.json), checked against the
#   esp32s3-n4r2 storage budget by default; add --target esp32s3-n8r8
#   for an 8 MB board, --payload installer.elf for the installer flow.
```

Then: plug the ESP32-S3 into a PC, copy `frontend.zip` onto the exposed
drive (replace the existing one), eject, unplug, replug into the PS5.

## Build the firmware

```sh
# one-time: ESP-IDF v5.4.2 next to this repo
git clone -b v5.4.2 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git esp-idf
cd esp-idf && ./install.sh esp32s3

# build (activate the IDF environment first: . ./esp-idf/export.sh)
python3 tools/build.py package --target esp32s3-n4r2
```

or via the wrapper:

```sh
make targets                 # list board targets
make build  TARGET=esp32s3-n8r8
make package TARGET=esp32s3-n8r8
make zip     UPSTREAM_TAG=v0.5.2   # frontend.zip only, into main/
make storage FRONTEND_ZIP=my.zip  # rebuild just the FAT image
```

`package` produces `build/<target>/ps5_wkal_host-full.bin`, a complete
first-flash image (bootloader, partition table, app, storage) sized to the
board's flash. Write it at **0x0**. It resets NVS and replaces storage
contents — use it for the first flash; later updates are file copies on the
USB drive, not reflashes (no OTA by design).

Flashing needs an explicit port — nothing is auto-selected:

```sh
make flash  TARGET=esp32s3-n4r2 PORT=/dev/ttyACM0
make monitor TARGET=esp32s3-n4r2 PORT=/dev/ttyACM0
make flash-all ... --erase-storage   # also rewrite storage (erases USB files)
```

## CI (GitHub Actions)

`.github/workflows/build.yml`:

- **Run → Build firmware** (workflow_dispatch): builds all six targets (or a
  comma-separated subset via `targets`), each packaging the frontend from the
  latest upstream release (pin with `upstream_tag`). Artifacts:
  `frontend` (zip + provenance) and `firmware-<target>` per board
  (full-flash image, manifest, storage image).
- **Push a `v*` tag**: same matrix plus a GitHub release containing
  `ps5-wkal-<tag>-<board>-full.bin` for every board plus `frontend.zip`.
- Every run also executes the host test suite (unit + ASan on the runner,
  full FAT round-trip inside the ESP-IDF v5.4.2 container).

## Tests

```sh
python3 tools/build.py test         # needs make + gcc (+ zlib)
python3 tools/build.py test-asan
```

The suite (`tools/tests/`) compiles the exact sources the firmware runs
(`main/wk_*.c`, `main/http_server.c`, `main/miniz.c`) on the host:

- **DNS differential** — wire queries fed to both the firmware's
  `wk_dns_handle_packet()` and the reference `pc-host/host.py` builders;
  responses must match byte-for-byte.
- **Path mapping differential** — `wk_zip_map_path()` vs the reference
  `_relative_path()` over generated URI cases (queries, `/app/`,
  `/document/.../ps5/`, `%2e%2e` folding, `..` chains).
- **zip index** — hand-built STORE+DEFLATE archive plus garbage rejection.
- **HTTP core** — `wk_http_serve_get()` through a memory sink: every entry of
  a real archive inflates to its declared size with matching CRC32.
- **payload.elf zlib cross-check** — tinfl loop vs independent zlib inflate.
- **HTTP handler** — the real `wk_httpd_handle_get` via a mock
  `esp_http_server`.
- **FAT round-trip** (needs ESP-IDF) — `storagegen.py` image read back with
  IDF's `fatfsparse`, byte-exact, then served through the firmware zip index.

Run it whenever `wk_*.c` or `http_server.c` changes.

## Size budget (4 MB board, measured)

| item | size |
|------|------|
| app (TLS, DNS, MSC/CDC — no frontend) | ~894 KiB of 1408 KiB factory |
| crash record partition | 128 KiB |
| storage partition (FAT, wear-levelled) | 2.4375 MB |
| frontend.zip from upstream release (DEFLATE) | ~1.95 MB |
| free on the drive | ~0.5 MB |

The installer-ELF frontend variant (~2.9 MB) does not fit 4 MB flash — use an
8 MB+ board for it. RAM: the zip index (~30 KB) and 48 KB/conn buffers; the
archive streams from FAT through the tinfl ring buffer, never fully in RAM.

## Notes / gotchas

- The frontend (exploit chain, patches, icons) is GPL-3.0 upstream work;
  this repository only packages and serves it. Payloads are fetched from the
  upstream-pinned releases at build time — nothing exploit-related is
  committed here.
- Vendored miniz uses private names for ROM-provided symbols: the ESP32-S3
  ROM ships an incompatible `tinfl_decompressor` layout, and resolving to ROM
  corrupts the heap. `build.py` checks the linked implementation and the HTTP
  object's references on every build.
- Certificates are embedded as `EMBED_TXTFILES` (mbedTLS needs the trailing
  NUL in the PEM length); `build.py` verifies cert/key bytes and public-key
  match in the linked ELF. Regenerated automatically on first build.
- `auto_mount_off=1` + a linker-wrapped `tud_msc_test_unit_ready_cb` keeps
  the local FAT mount while MSC advertises the medium. Host and app read;
  only the PC host writes.
- If a PC rewrites the FAT while the app is serving, the app's cached FAT is
  stale until the next power cycle — hence: unplug → edit → replug.
- HTTPS **and** HTTP both run; the PS5 hits 443, browsers can test on 80.
  Local reachability responses cover `/netstart/icst|ps4|ps5`,
  `/connecttest.txt`, Microsoft connect-test hosts and the networktest
  streams so the console's Wi-Fi check settles against the AP.
- `CONFIG_SPIRAM_MODE_QUAD`/`OCT` per target matches the module; boards with
  a different PSRAM than listed need their own entry in `targets/`.
- No OTA: factory is single-slot by design (updates are file copies).
- `esp_tinyusb`/`tinyusb` are managed components downloaded from the
  Espressif registry on first build (network needed once).

## License

Firmware sources in `main/` and `tools/`: GPL-3.0-or-later, consistent with
the upstream project this host packages (see `LICENSE`). miniz is public
domain; the upstream frontend keeps its own GPL-3.0 licensing and is fetched
unmodified from the pinned release tags.
