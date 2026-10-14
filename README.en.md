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
`ps5_wkal_host`.

## Wi-Fi configuration over USB

The firmware reads `WIFI.TXT` at the USB volume root during startup. New
storage images include these defaults:

```ini
ssid=tramoya_host
password=tramoya_host
```

Authentication is WPA2-PSK. Edit the file, eject the volume and restart to apply
changes. If the file is missing or invalid, Wi-Fi stays off and USB remains
available for correction. See the [format and procedure](docs/wifi-configuration.en.md).
The credential change, missing/invalid file and USB recovery scenarios passed on hardware on 14 October; see the status record.

## Current status — 14 October 2026

**WIFI.TXT functional validation is complete for the tested scenarios.** The
N16R8 firmware based on `7c197bc` was flashed with the inert diagnostic ZIP:
WPA2, DHCP, local A resolution, external-name rejection, HTTP/HTTPS, interface
isolation, credential changes and USB configuration recovery were exercised.
The AAAA response defect is fixed in software under the [DNS contract](docs/dns.en.md);
its DNS hardware test and HTTP/HTTPS/isolation controls passed on 14 October
(75 checks). HTTP curl 56 remains
unexplained; curl 63 came from the Mac being on the home Wi-Fi.

The separate 9 October diagnostic and its ten-minute stability result remain
historical evidence for that firmware only. Its board is kept as the reference.
A controlled HTTP 56 run completed 61 requests without reproducing the error;
its cause remains open. The new PSRAM frontend snapshot passed the inert USB
update and WIFI.TXT integrity test on 14 October. The same candidate passed a
physical 30-minute run of DNS, HTTP/HTTPS, MSC reads and isolation checks:
181 complete samples, no failed checks and no reset observed over CDC. The
historical HTTP curl 56 remains unexplained.
David confirmed that the PS5 User's Guide displays the ESP32's inert page.
The status photo shows `192.168.1.3/24` with gateway and DNS `192.168.1.1`;
David confirmed `tramoya_host` over Wi-Fi and no LAN cable. Combined with the
AP-only firmware and previous egress checks, this validates the tested network
topology's isolation. Direct egress from the console, HTTPS transport and MSC
on PS5 were not tested. A [minimal Relapse integration for 13.60](docs/relapse-integration.en.md)
passed its hardware test: WebKit, kernel and elfldr startup were confirmed on
the PS5. The USB update verified all 51 resources and preserved WIFI.TXT.
Chain repeatability remains pending; the inert ZIP is retained. The later
kstuff/ShadowMount+ profile has its own hardware result below.
See [status and evidence](docs/project-status.en.md).

## Base project architecture

An ESP32-S3 firmware that merges the **PC host** flow of
[itsPLK/ps5-webkit-autoloader](https://github.com/itsPLK/ps5-webkit-autoloader)
with a **USB drive** — one device, plugged straight into the PS5:

1. **USB**: the ESP32 enumerates as a mass-storage drive (TinyUSB MSC over a
   wear-levelled FAT partition). The PS5 reads `ps5_autoloader/` payloads
   from it; a PC can rewrite `frontend.zip` and drop new payloads.
2. **WiFi**: the ESP32 brings up the WPA2 AP defined in `WIFI.TXT`. DHCP hands clients
   the AP's IP as their DNS server; DNS :53 answers `manuals.playstation.net`
   with the AP IP for IN A/ANY; other local record types return NODATA,
   and external names are rejected. See the [DNS contract](docs/dns.en.md).
3. **HTTPS :443 / HTTP :80** serve an immutable copy of `frontend.zip` made
   in PSRAM at startup. FAT is unmounted before MSC becomes available to the
   computer. The ZIP is not embedded in the application image and must fit in
   the largest free PSRAM block with a 512 KiB margin.

The intended PS5 workflow remains unvalidated. The software now releases FAT
before exposing MSC; the inert ZIP update and integrity check passed on the
board. The 30-minute stability run passed and the inert page appeared in the
PS5 User's Guide; longer runs and the complete USB/PS5 workflow remain untested.

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
WIFI.TXT                SSID and WPA2 password read at startup
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
eject → restart. The new firmware serves a PSRAM snapshot until reboot. FAT is
closed before the computer receives MSC. If the ZIP exceeds available PSRAM or
cannot be indexed, USB remains available for correction and the web frontend is absent.
The inert write/reload/integrity path passed on hardware; the older
diagnostic firmware is separate evidence. The updater requires this candidate's
HTTPS `/__tramoya_storage` witness before writing or restoring the ZIP; the
older DNS firmware does not offer it.

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
export PATH="$HOME/.espressif/python_env/idf5.4_py3.12_env/bin:$PATH"
. "$HOME/esp/esp-idf-v5.4.2/export.sh"
python3 tools/build.py build --target esp32s3-n16r8
# Package a frontend ZIP that has already been prepared and checked:
python3 tools/build.py package --target esp32s3-n16r8 --frontend frontend.zip
```

Without `--frontend`, packaging requires `--upstream-tag` to prepare the external frontend;
omitting both fails before downloading or building. `--frontend-source diagnostic` generates
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
  explicitly pinned upstream release (`upstream_tag` or `TRAMOYA_UPSTREAM_TAG`). Artifacts:
  `frontend` (zip + provenance) and `firmware-<target>` per board
  (full-flash image, manifest, storage image).
- **Push a `v*` tag**: same matrix plus a GitHub release containing
  `ps5-wkal-<tag>-<board>-full.bin` for every board plus `frontend.zip`.
- Every run also executes the host test suite (unit + ASan on the runner,
  full FAT round-trip inside the ESP-IDF v5.4.2 container).

## Tests

```sh
python3 tools/build.py test --target esp32s3-n16r8  # quick host suite; FAT requires IDF
python3 tools/build.py test-asan --target esp32s3-n16r8
```

The suite (`tools/tests/`) compiles the exact sources the firmware runs
(`main/wk_*.c`, `main/http_server.c`, `main/miniz.c`) on the host:

- **DNS contract** — corrected fixed wire vectors and independent matrices
  for types, classes, flags, opcodes and boundaries. The inherited host.py
  errors are deliberately not preserved; see [DNS](docs/dns.en.md).
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
new candidate copies the ZIP into PSRAM before exposing MSC; the archive must
fit with a 512 KiB margin. Earlier firmware streamed it from FAT.

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
- `auto_mount_off=1` prevents TinyUSB from remounting FAT in the application.
  The candidate closes the application FAT mount before creating the USB LUN.
  HTTP/HTTPS read only the PSRAM snapshot; edits appear after reboot.
- HTTPS **and** HTTP both run; the intended PS5 guide flow uses 443, and
  browsers can test on 80. Local reachability responses cover
  `/netstart/icst|ps4|ps5`, `/connecttest.txt`, Microsoft connect-test hosts
  and the networktest streams. The transport and Wi-Fi check behavior of this
  PS5 have not been recorded.
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

## Local checks and reproducible releases (2026-10-12)

See the [development guide](DESARROLLO.md). For the validated N16R8 board on macOS:

```sh
export PATH="$HOME/.espressif/python_env/idf5.4_py3.12_env/bin:$PATH"
source "$HOME/esp/esp-idf-v5.4.2/export.sh"
python3 tools/build.py test --target esp32s3-n16r8 --require-fat
python3 tools/build.py test-asan --target esp32s3-n16r8 --require-fat
bash tools/install_git_hooks.sh
```

The Makefile selects Apple/GNU linker flags. Both full suites include FAT;
`--require-fat` rejects missing IDF instead of silently skipping that test.
The pinned local Gitleaks hook scans staged content. `checks.yml` runs host
tests on macOS/Linux and IDF integration for PRs and pushes to main; remote
results are available in GitHub Actions for each run. Full releases
require an explicit `upstream_tag` or repository variable `TRAMOYA_UPSTREAM_TAG`.
Local storage/package requires a verified `--frontend` or `--upstream-tag`,
or the explicit offline diagnostic frontend. Record ZIP SHA-256 and firmware
manifest; newly generated certificates prevent a blanket bit-reproducibility claim.

Every build.py operation except targets/deps requires --target; make requires
TARGET for those operations. No implicit board profile is selected.


## Standalone payload profile — 15 October 2026

The [kstuff + ShadowMount+ profile](docs/payloads.en.md) uses no etaHEN:
56 files, 2,297,244 bytes, unchanged firmware. Software checks pass. David then
confirmed payload loading and successful ShadowMount+ execution/operation on his
PS5 13.60. Repeatability and stability remain pending; recovery is preserved.
