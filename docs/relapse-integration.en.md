**English** · [Español](integracion-relapse.md) · [Status](project-status.en.md)

# Minimal Relapse integration for PS5 13.60

Prepared on 14 October 2026. The candidate uses the existing Relapse checkout at
`d8e6896b5cb33b04f1e038a5d698d3d4aee5947c`. Its README declares support through
13.60 and the firmware selection, offsets and loader flow match that target.
All 53 local files match the pinned commit. **The minimal hardware test passed
on David's PS5 13.60:** his photos show WebKit readiness, kernel read/write,
ready privileges, elfldr return `0x0`, and the final TRAMOYA marker with optional
payloads disabled. The minimal chain and ELF loader startup are physically
validated for that run. His earlier direct observation confirmed the inert
TRAMOYA page in the User's Guide over `tramoya_host`, with Wi-Fi only and no LAN.

The minimal ZIP includes the existing `elfldr-ps5-1360.elf` and
`kexp_2026_05_25.bin` selected by Relapse. Their hashes are pinned to that
repository snapshot; their independent builds have not been reproduced.
The root page enables a manual launch only for PS5 13.60, inside a log frame.
The frame is cleared before loading the controller to discard an active route
restored after a browser crash. Opening the internal
`/relapse/index.html` URL directly also starts the chain: the root page is an
interaction gate, not a security boundary.
Actual Guide session/frame restoration remains a hardware-test limitation;
the simulation checks that the code clears the frame when executed.

The R2 hook for loading optional payloads is disabled. The local kstuff reports
1.11, but its SHA-256 differs from the official v1.11 release digest. The local
ShadowMount+ contains `1.7beta1` but matches none of the 30 release digests
examined. The local etaHEN is an ELF wrapper whose version and 13.60 support
have not been established. These exact binaries remain excluded pending
identification. Official support for a different release does not establish
support for the local bytes. See the [full inventory](integracion-relapse.md).

The retained WebKit Autoloader routes 13.60 to Relapse, but its prepared tree
is incomplete. Unified Autoloader and Payload Manager are preserved as source;
they are not included in this minimal package. UMTX2 and Poops are outside the
target firmware according to the retained frontend's firmware table. Software
preparation downloaded no new payload binaries and ran no console code. The
subsequent hardware execution is recorded separately.

Primary sources: [Relapse](https://github.com/ntfargo/Relapse-Exploit),
[WebKit Autoloader](https://github.com/itsPLK/ps5-webkit-autoloader),
[Kstuff Lite v1.11](https://github.com/EchoStretch/kstuff-lite/releases/tag/v1.11),
[ShadowMount+](https://github.com/drakmor/ShadowMountPlus/releases), and
[etaHEN](https://github.com/etaHEN/etaHEN/releases/tag/2.5B).

`tools/build_relapse_frontend.py` checks `tools/relapse-13.60.lock.json`, keeps
upstream bytes except for the documented optional-payload hook, and normalizes
ZIP filenames to lowercase for the firmware index. MIT notices are retained.
The original ELF loader listens on port 9021; localhost-only binding is not
claimed. Use the previously validated isolated network.

The package has **51 files, 260,378 bytes**, SHA-256
`8335f216cc9d590130fe567e5862de092a53e59b4f6e540e3deafb97f0934529`.
It is below the conservative 1 MiB packaging budget. Physical loading of this
ZIP passed; free PSRAM headroom and larger ZIPs were not measured or tested.
The builder refuses overwrites, preserves its
input and records provenance and zlib runtime next to the ZIP. It performs no
network, USB or console actions. Set `TRAMOYA_RELAPSE_SOURCE` to the pinned
local source and run:

```sh
python3 tools/build_relapse_frontend.py \
  --source "$TRAMOYA_RELAPSE_SOURCE" \
  --out build/integracion-relapse/frontend.zip --target esp32s3-n16r8
```

Normal and ASan/UBSan suites retain mandatory FAT coverage. Additional checks
cover tampering, symlinks, overwrite protection, deterministic packaging, the
manual gate, JavaScript parsing without running Relapse, all resources through
the real HTTP C core and the actual candidate's FAT round trip.

The firmware remains **927,920 bytes**, SHA-256
`334f23534285f706fdf69e12b9210b5f0e6566770711f851d64cb887fac42e0a`.
No flashing is required. The local USB kit preserves the tested inert ZIP and
WIFI.TXT, requires the MSC ownership witness and prior HTTPS identity, creates
backups and checks exact bytes after writing and rebooting. Its verifier reads
all 51 resources over HTTPS without executing scripts. WIFI.TXT is never
rewritten. Default credentials, WPA2, reference board and recovery images remain
available. Restoring the ESP32 ZIP does not undo payload actions on the console.
Historical HTTP curl 56 remains unexplained. The 14 October 23:43 USB record is
VERIFIED: all 51 HTTPS resources match the ZIP and the client checked WIFI.TXT.
Photos IMG_4373/4374 show the subsequent successful minimal chain. The firmware
hash identifies the retained local artifact, not flash readback. Preparation
manifests remain frozen; the later result is in
`outputs/integracion-relapse-20261014/prueba-fisica-ps5/resultado.json` locally.
The minimal test did not transfer another ELF. On 15 October David subsequently
confirmed payload loading and ShadowMount+ operation with the [standalone profile](payloads.en.md).
Repeatability, stability and actual Guide session restoration remain untested.
The 30-minute run applies to the earlier inert ZIP.


## Next standalone profile

[Next standalone profile: kstuff + ShadowMount+](payloads.en.md).
