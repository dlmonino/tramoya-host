[Español](README.md) · **English**

# TRAMOYA Host

TRAMOYA is a USB-powered standalone Wi-Fi host project for serving homebrew
content to a PS5 without providing Internet access. The development target
is **PS5 firmware 13.60** and **ESP32-S3 N16R8** hardware (16 MB flash,
8 MB PSRAM), with content updates over USB MSC. A dongle form factor is
a later goal.

This is a fork of [kxn/ps5-wkal-esp32s3](https://github.com/kxn/ps5-wkal-esp32s3),
based on revision `70aed11faa1a657cd25cc5a52781c2b3764398f5`, retaining its
history, authorship and licenses. Build artifacts still use the internal name
`ps5_wkal_host`; this documentation update does not change the firmware.

## Current status — 9 October 2026

**Physical diagnostics have passed. Full-firmware integration and PS5 testing
remain pending.** The separate `esp32s3-servidor-diagnostico` project was
flashed and tested on one N16R8 board; the second board remains a reference
running the previously tested `hello_world` firmware.

- Passed: startup, PSRAM, WPA2 Wi-Fi, DHCP, DNS, HTTP/HTTPS, Internet isolation,
  and USB MSC reads, writes and persistence.
- Stability: ten minutes, 61 samples, zero errors, no detected reboot and
  stable internal memory after warm-up.
- Pending: complete preparation of this repository's full firmware, integrate
  the selected content, validate actual served-content updates through MSC
  and repeat the checks against that final build before testing on PS5.

The diagnostic firmware serves embedded pages. Its MSC test does not prove
USB updates of the served frontend or compatibility with PS5 13.60.
See [Status and validation](docs/project-status.en.md) for results and limits.
This repository's `--frontend-source diagnostic` option generates a different
test page; it does **not** reproduce the independently validated firmware.

The following technical guide is retained from the base project. It describes
the inherited architecture and tools, not completed TRAMOYA tests on PS5.
In particular, the base AP below is open; the validated WPA2 AP belongs to
the separate diagnostic firmware.

## Base project architecture

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

## Idle power and heat

CPU power management now uses **80 MHz when idle / 160 MHz while tasks are
runnable**, following the USB-safe approach in
[the remote-buddy power changes](https://github.com/kxn/vibe-remote-buddy-firmware/pull/1).
APB stays at 80 MHz for USB OTG and PSRAM; automatic light sleep and deep
sleep are not used. USB storage and the Wi-Fi hotspot remain available.
See ESP-IDF's [power management documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/api-reference/system/power_management.html).

The independently tested diagnostic firmware uses a fixed 160 MHz CPU.
Its results do not validate the base firmware's dynamic power configuration.

The five-second heartbeat and repeated flash crash-record reads are removed;
boot/crash diagnostics run once, and per-request DNS/HTTP logs are debug-level.
The main task exits after startup. DNS blocks waiting for packets and backs
off on socket errors; TinyUSB already waits for USB events.

**An always-on Wi-Fi AP still draws radio power even without page requests.**
ESP32-S3's [Wi-Fi Modem-sleep](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32s3/api-guides/wifi.html#station-sleep)
works in station-only mode, so enabling `WIFI_PS_MIN_MODEM` does not make
this hotspot sleep. Fewer application tasks do not guarantee lower idle
power than a BLE receiver. Further radio savings would require a different
operating mode, such as explicitly turning off the hotspot after use.

`tools/build.py build/package/flash` also updates the power settings in an
existing per-board `sdkconfig`, preserving other settings, and validates the
resolved configuration. Direct `idf.py` builds need a fresh configuration
or the equivalent manual settings. This is a **firmware update**: replacing
`frontend.zip` alone cannot enable it. Flash the correct board target without
`--erase-storage` to preserve USB files; a `*-full.bin` overwrites them.

For a hardware comparison, use the same USB supply, enclosure, ambient
temperature, and Wi-Fi client state. Wait for temperatures to settle and
measure idle USB current before/after; then check PS5 reconnection, User's
Guide HTTPS loading, payload reads, and PC USB writes. Build/host tests do
not establish a measured temperature or current reduction.

## Hardware targets

The following ESP32-S3 flash/PSRAM profiles are configured — pick the board with
`--target` (or `make build TARGET=...`); each gets its own partition layout,
sdkconfig and build directory:

| target          | flash | PSRAM | PSRAM mode | storage partition (MiB, approx.) |
|-----------------|-------|-------|------------|-------------------|
| `esp32s3-n4r2`  | 4 MB  | 2 MB  | quad       | 2.44             |
| `esp32s3-n8r2`  | 8 MB  | 2 MB  | quad       | 5.81             |
| `esp32s3-n8r8`  | 8 MB  | 8 MB  | octal      | 5.81             |
| `esp32s3-n16r2` | 16 MB | 2 MB  | quad       | 13.81            |
| `esp32s3-n16r8` | 16 MB | 8 MB  | octal      | 13.81            |
| `esp32s3-n32r8` | 32 MB | 8 MB  | octal      | 29.81            |

`python3 tools/build.py targets` prints the live list from
`targets/boards.json`.

These are available configurations, not physical validation results for every
board. **The N16R8 base and diagnostic partition tables differ:** storage starts
at `0x230000` in this repository and `0x310000` in the tested diagnostic project;
their application partitions are 2 MiB and 3 MiB respectively. Always use a
matching bootloader, partition table and application. Preserving flash sectors
while changing the layout does not guarantee a usable existing FAT volume.

## The USB drive: layout & payload autoload

The exposed volume is a plain FAT drive — open it from a PC and you get:

```
frontend.zip            the web frontend the AP serves (see below)
ps5_autoloader/         payload drop-in dir read by the PS5 side
  README.txt            short on-drive instructions
  autoload.txt          (optional) load list, see format below
  etaHEN.elf            payload ELFs you place here
  kstuff.elf
  ...
```

After the exploit chain jailbreaks the console it loads the **unified
autoloader** (bundled in the frontend as `payloads/payload.elf`). The
autoloader waits for the ELF loader, then looks for an `autoload.txt`
config, highest priority first:

1. `ps5_autoloader_<app>/autoload.txt` on the USB drive — `<app>` is
   `bdjb` for the Blu-ray-player jailbreak or the entry app's Title ID
   (e.g. `PPSA01650` for YouTube)
2. `ps5_autoloader_<app>/autoload.txt` in the PS5's internal `/data`
3. `ps5_autoloader/autoload.txt` on the USB drive — **the usual case on
   this device**
4. `ps5_autoloader/autoload.txt` in internal `/data`

If a config is found, every payload listed in it is launched in order.
If none is found, the autoloader starts its bundled **Payload Manager**
so payloads can be picked on the console instead.

### `autoload.txt` format

```
# a line starting with '#' is a comment
etaHEN.elf              # resolved relative to this file's directory
!1500                   # sleep 1500 ms before the next line
kstuff.elf
@sync                   # optional, see below
```

- one entry per line; blank lines and `#` comments are ignored
- file names are relative to the directory containing `autoload.txt`;
  absolute paths (starting with `/`) are used as-is
- `!<ms>` sleeps before the next line — timing-sensitive payloads
  (e.g. kstuff right after etaHEN) usually want a pause
- `@sync` (best placed last): once every payload has loaded without a
  single error, the whole active directory is copied to the PS5's
  internal `/data`, verified byte-by-byte, and removed from the drive —
  later boots then run from the console's own storage. Note that on this
  device `@sync` writes to the emulated USB volume **from the PS5
  side**, which is not part of the supported update flow; treat it as
  experimental. The supported flow is: edit the drive from a PC with the
  device unplugged from the console.

Payload ELFs are not included — drop in the ones you use (etaHEN,
kstuff, ...) from their own projects. To bake payloads into the storage
image at flash time instead, use `make storage PAYLOAD_DIR=<dir>` (or
`build.py --payload-dir <dir>`).

Update flow: unplug from the PS5 → plug into a PC → edit files →
eject → replug into the PS5. The firmware caches the FAT/zip index until
the next power cycle, so edits made while it is serving do not take
effect until a replug.

TRAMOYA still needs to validate exclusive storage ownership during writes and
the subsequent reload of served content. The tested diagnostic firmware unmounts
its local FAT before handing storage to the computer, but serves embedded pages;
it does not exercise this base firmware's shared FAT/ZIP serving path.

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

Version tags shown here are inherited syntax examples, not compatibility
recommendations for PS5 13.60. Pin and verify the chosen release, revision and
dependencies before preparing the full TRAMOYA frontend.

### Just want frontend.zip for the USB drive?

You do not need ESP-IDF for this — only Python 3, git and bash:

```sh
python3 tools/build_frontend_zip.py --target esp32s3-n16r8 --tag v0.5.2
# → frontend.zip (+ frontend.zip-source.json), checked against the
#   selected N16R8 storage budget. The tool defaults to esp32s3-n4r2
#   and the latest release when those options are omitted.
```

Then: plug the ESP32-S3 into a PC, copy `frontend.zip` onto the exposed
drive (replace the existing one), eject, unplug, replug into the PS5.

## Build the firmware

Use ESP-IDF **5.4.2**. Reuse an existing installation; the example below uses
the macOS SDK location from the TRAMOYA validation environment. Run project
commands from the repository root:

```sh
. "$HOME/esp/esp-idf-v5.4.2/export.sh"
python3 tools/build.py build --target esp32s3-n16r8
# Package a frontend ZIP that has already been prepared and checked:
python3 tools/build.py package --target esp32s3-n16r8 --frontend frontend.zip
```

Without `--frontend`, packaging prepares the external frontend by default;
pin its version with `--upstream-tag`. `--frontend-source diagnostic` generates
this repository's benign placeholder page. Neither option reproduces the separate
diagnostic firmware already tested. Build/test tools may download dependencies.

or via the wrapper:

```sh
make targets                 # list board targets
make build   TARGET=esp32s3-n16r8
make package TARGET=esp32s3-n16r8 FRONTEND_ZIP=frontend.zip
make zip     TARGET=esp32s3-n16r8 UPSTREAM_TAG=v0.5.2
make storage TARGET=esp32s3-n16r8 FRONTEND_ZIP=frontend.zip
```

`package` produces `build/<target>/ps5_wkal_host-full.bin`, a complete
first-flash image (bootloader, partition table, app, storage) sized to the
board's flash. Write it at **0x0**. It resets NVS and replaces storage
contents — use it for the first flash; later updates are file copies on the
USB drive, not reflashes (no OTA by design).

Flashing needs an explicit port. This is base-project syntax, not a request
to reflash the validated diagnostic board:

```sh
# Replace this with the confirmed port for the test board.
TRAMOYA_PORT=/dev/cu.CONFIRMED_PORT
make flash   TARGET=esp32s3-n16r8 PORT="$TRAMOYA_PORT"
make monitor TARGET=esp32s3-n16r8 PORT="$TRAMOYA_PORT"
```

`flash` preserves storage sectors, but only a compatible partition layout can
preserve the volume. `make flash-all` or `build.py flash --erase-storage` also
writes storage and replaces USB files; supply the matching target and port.

To enter the ROM loader, hold BOOT, press and release RST, then release BOOT.
To boot the application, press RST with BOOT released. The diagnostic's native
USB Serial/JTAG port disappears when TinyUSB MSC takes over; its logs are
configured on UART0 at 115200 baud. The board's possible UART bridge is unverified.
The separate `hello_world` recovery image set is kept outside this repository.

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

## Inherited size budget (4 MB board)

These are base-project figures, not measurements of the N16R8 diagnostic build.

| item | size |
|------|------|
| app (TLS, DNS, MSC/CDC — no frontend) | ~894 KiB of 1408 KiB factory |
| crash record partition | 128 KiB |
| storage partition (FAT, wear-levelled) | 2.4375 MiB |
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
